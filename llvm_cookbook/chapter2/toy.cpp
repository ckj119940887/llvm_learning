#include <iostream>
#include <vector>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"

enum Token_Type
{
    EOF_TOKEN = 0,
    NUMERIC_TOKEN,
    IDENTIFIER_TOKEN,
    PARAN_TOKEN,
    DEF_TOKEN
};

// store the value of numeric tokens
static int Numeric_Val;

static std::string Identifier_string;

FILE *file;

static int get_token()
{
    static int LastChar = ' '; // Placeholder value for first character

    while (isspace(LastChar))
    {
        LastChar = fgetc(file);
    }

    if (isalpha(LastChar))
    {
        Identifier_string = LastChar;
        while (isalnum(LastChar = fgetc(file)))
        {
            Identifier_string += LastChar;
        }

        if (Identifier_string == "def")
            return DEF_TOKEN;

        return IDENTIFIER_TOKEN;
    }

    if (isdigit(LastChar))
    {
        std::string NumStr;
        do
        {
            NumStr += LastChar;
            LastChar = fgetc(file);
        } while (isdigit(LastChar));

        Numeric_Val = std::strtod(NumStr.c_str(), nullptr);

        return NUMERIC_TOKEN;
    }

    if (LastChar == '#')
    {
        do
        {
            LastChar = fgetc(file);
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

        if (LastChar != EOF)
            return get_token();
    }

    if (LastChar == EOF)
        return EOF_TOKEN;

    int ThisChar = LastChar;
    LastChar = fgetc(file);
    return ThisChar;
}

// 包含了代码中所有的函数和变量
static llvm::Module *Module_ob;
static llvm::LLVMContext TheGlobalContext;
// 帮助生成 LLVM IR 并且记录程序的当前点，以插入 LLVM 指令;另外，Builder 对象有创建新指令的函数。
static llvm::IRBuilder<> Builder(TheGlobalContext);
// 符号表
static std::map<std::string, llvm::Value *> Named_Values;
static llvm::FunctionPassManager *Global_FP;

class BaseAST
{
public:
    virtual ~BaseAST() {}
    virtual llvm::Value *codegen() = 0;
};

class VariableAST : public BaseAST
{
    std::string Var_Name;

public:
    VariableAST(const std::string &name) : Var_Name(name) {}
    virtual llvm::Value *codegen();
};

llvm::Value *VariableAST::codegen()
{
    llvm::Value *V = Named_Values[Var_Name];
    return V ? V : 0;
}

class NumericAST : public BaseAST
{
    int numeric_val;

public:
    NumericAST(int val) : numeric_val(val) {}
    virtual llvm::Value *codegen();
};

llvm::Value *NumericAST::codegen()
{
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(TheGlobalContext), numeric_val);
}

class BinaryAST : public BaseAST
{
    std::string Bin_Operator;
    BaseAST *LHS, *RHS;

public:
    BinaryAST(const std::string &op, BaseAST *lhs, BaseAST *rhs) : Bin_Operator(op), LHS(lhs), RHS(rhs) {}
    virtual llvm::Value *codegen();
};

llvm::Value *BinaryAST::codegen()
{
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (L == 0 || R == 0)
        return 0;

    switch (atoi(Bin_Operator.c_str()))
    {
    case '+':
        return Builder.CreateAdd(L, R, "addtmp");
    case '-':
        return Builder.CreateSub(L, R, "subtmp");
    case '*':
        return Builder.CreateMul(L, R, "multmp");
    case '/':
        return Builder.CreateUDiv(L, R, "divtmp");

    default:
        return 0;
    }
}

class FunctionDeclAST
{
    std::string Func_Name;
    std::vector<std::string> Arguments;

public:
    FunctionDeclAST(const std::string &name, const std::vector<std::string> &args) : Func_Name(name), Arguments(args) {}
    virtual llvm::Function *codegen();
};

llvm::Function *FunctionDeclAST::codegen()
{
    std::vector<llvm::Type *> Integers(Arguments.size(), llvm::Type::getInt32Ty(TheGlobalContext));
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getInt32Ty(TheGlobalContext), Integers, false);
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, Func_Name, Module_ob);

    if (F->getName() != Func_Name)
    {
        F->eraseFromParent();
        F = Module_ob->getFunction(Func_Name);
        if (!F->empty())
            return 0;
        if (F->arg_size() != Arguments.size())
            return 0;
    }

    unsigned Idx = 0;
    for (llvm::Function::arg_iterator Arg_It = F->arg_begin(); Idx != Arguments.size(); ++Arg_It, ++Idx)
    {
        Arg_It->setName(Arguments[Idx]);
        Named_Values[Arguments[Idx]] = Arg_It;
    }

    return F;
}

class FunctionDefnAST
{
    FunctionDeclAST *Func_Decl;
    BaseAST *Body;

public:
    FunctionDefnAST(FunctionDeclAST *decl, BaseAST *body) : Func_Decl(decl), Body(body) {}
    virtual llvm::Function *codegen();
};

llvm::Function *FunctionDefnAST::codegen()
{
    Named_Values.clear();

    llvm::Function *TheFunction = Func_Decl->codegen();
    if (TheFunction == 0)
        return 0;

    llvm::BasicBlock *BB = llvm::BasicBlock::Create(TheGlobalContext, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    if (llvm::Value *RetVal = Body->codegen())
    {
        Builder.CreateRet(RetVal);
        verifyFunction(*TheFunction);
        return TheFunction;
    }

    TheFunction->eraseFromParent();
    return 0;
}

class FunctionCallAST : public BaseAST
{
    std::string Function_Callee;
    std::vector<BaseAST *> Function_Arguments;

public:
    FunctionCallAST(const std::string &callee, std::vector<BaseAST *> &args) : Function_Callee(callee), Function_Arguments(args) {}
    virtual llvm::Value *codegen();
};

llvm::Value *FunctionCallAST::codegen()
{
    llvm::Function *CalleeF = Module_ob->getFunction(Function_Callee);
    std::vector<llvm::Value *> ArgsV;

    for (unsigned i = 0, e = Function_Arguments.size(); i != e; ++i)
    {
        ArgsV.push_back(Function_Arguments[i]->codegen());
        if (ArgsV.back() == 0)
            return 0;
    }

    return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

static int Current_Token;

static int next_token()
{
    Current_Token = get_token();
    return Current_Token;
}

static BaseAST *numeric_parser();
static BaseAST *identifier_parser();
static BaseAST *paran_parser();
static BaseAST *expression_parser();
static BaseAST *binary_op_parser(int Precedence, BaseAST *LHS);

static BaseAST *Base_Parser()
{
    switch (Current_Token)
    {
    case NUMERIC_TOKEN:
    {
        return numeric_parser();
    }
    case IDENTIFIER_TOKEN:
    {
        return identifier_parser();
    }
    case '(':
    {
        return paran_parser();
    }
    default:
        return 0;
    }
}

static BaseAST *numeric_parser()
{
    BaseAST *Result = new NumericAST(Numeric_Val);
    next_token();
    return Result;
}

static BaseAST *expression_parser()
{
    BaseAST *LHS = Base_Parser();
    if (!LHS)
        return 0;
    return binary_op_parser(0, LHS);
}

static BaseAST *identifier_parser()
{
    std::string IdName = Identifier_string;

    next_token();

    if (Current_Token != '(')
        return new VariableAST(IdName);

    next_token(); // eat '('

    std::vector<BaseAST *> Args;
    if (Current_Token != ')')
    {
        while (1)
        {
            BaseAST *Arg = expression_parser();
            if (!Arg)
                return 0;
            Args.push_back(Arg);

            if (Current_Token == ')')
                break;

            if (Current_Token != ',')
                return 0; // error: expected ','

            next_token(); // eat ','
        }
    }

    next_token(); // eat ')'

    return new FunctionCallAST(IdName, Args);
}

static FunctionDeclAST *func_decl_parser()
{
    if (Current_Token != IDENTIFIER_TOKEN)
        return 0;

    std::string FnName = Identifier_string;

    next_token();

    if (Current_Token != '(')
        return 0; // error: expected '('

    std::vector<std::string> Function_Argument_Names;
    while (next_token() == IDENTIFIER_TOKEN)
        Function_Argument_Names.push_back(Identifier_string);

    if (Current_Token != ')')
        return 0; // error: expected ')'

    next_token(); // eat ')'

    return new FunctionDeclAST(FnName, Function_Argument_Names);
}

static FunctionDefnAST *func_defn_parser()
{
    next_token(); // eat 'def'

    FunctionDeclAST *Func_Decl = func_decl_parser();

    if (Func_Decl == 0)
        return 0;

    if (BaseAST *Body = expression_parser())
        return new FunctionDefnAST(Func_Decl, Body);

    return 0;
}

static std::map<char, int> Operator_Precedence;

static void init_precedence()
{
    Operator_Precedence['-'] = 1;
    Operator_Precedence['+'] = 2;
    Operator_Precedence['/'] = 3;
    Operator_Precedence['*'] = 4;
}

static int getBinOpPrecedence()
{
    if (!isascii(Current_Token))
        return -1;

    int TokPrec = Operator_Precedence[Current_Token];
    if (TokPrec <= 0)
        return -1;

    return TokPrec;
}

static BaseAST *binary_op_parser(int Old_Prec, BaseAST *LHS)
{
    while (1)
    {
        int Operator_Prec = getBinOpPrecedence();
        if (Operator_Prec < Old_Prec)
            return LHS;

        int BinOp = Current_Token;
        next_token();

        BaseAST *RHS = Base_Parser();
        if (!RHS)
            return 0;

        int Next_Prec = getBinOpPrecedence();
        if (Operator_Prec < Next_Prec)
        {
            RHS = binary_op_parser(Operator_Prec + 1, RHS);
            if (!RHS)
                return 0;
        }
        LHS = new BinaryAST(std::to_string(BinOp), LHS, RHS);
    }
}

static BaseAST *paran_parser()
{
    next_token(); // eat '('

    BaseAST *V = expression_parser();

    if (!V)
        return 0;

    if (Current_Token != ')')
        return 0; // error: expected ')'

    return V;
}

static void HandleDefn()
{
    if (FunctionDefnAST *F = func_defn_parser())
    {
        if (llvm::Function *LF = F->codegen())
        {
        }
    }
    else
    {
        next_token();
    }
}

static FunctionDefnAST *top_level_parser()
{
    if (BaseAST *E = expression_parser())
    {
        FunctionDeclAST *Func_Decl = new FunctionDeclAST("", std::vector<std::string>());
        return new FunctionDefnAST(Func_Decl, E);
    }

    return 0;
}

static void HandleTopExpression()
{
    if (FunctionDefnAST *F = top_level_parser())
    {
        if (llvm::Function *LF = F->codegen())
        {
        }
    }
    else
    {
        next_token();
    }
}

static void Driver()
{
    while (1)
    {
        switch (Current_Token)
        {
        case EOF_TOKEN:
            return;

        case ';':
            next_token();
            break;

        case DEF_TOKEN:
            HandleDefn();
            break;

        default:
            HandleTopExpression(); // handle
            break;
        }
    }
}

int main(int argc, char *argv[])
{
    llvm::LLVMContext &Context = TheGlobalContext;

    init_precedence();

    file = fopen(argv[1], "r");
    if (file == 0)
    {
        printf("File not found.\n");
    }

    next_token();

    Module_ob = new llvm::Module("my compiler", Context);

    Driver();

    Module_ob->print(llvm::outs(), 0);

    return 0;
}