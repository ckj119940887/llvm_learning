#include <iostream>
#include <vector>
#include <map>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"

enum Token_Type
{
    EOF_TOKEN = 0,
    NUMERIC_TOKEN,
    IDENTIFIER_TOKEN,
    PARAN_TOKEN,
    DEF_TOKEN,
    IF_TOKEN,
    THEN_TOKEN,
    ELSE_TOKEN,
    FOR_TOKEN,
    IN_TOKEN,
    UNARY_TOKEN,
    BINARY_TOKEN
};

// store the value of numeric tokens
static int Numeric_Val;

static std::string Identifier_string;

static std::map<char, int> Operator_Precedence;

// static llvm::ExecutionEngine *TheExecutionEngine;

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
        if (Identifier_string == "if")
            return IF_TOKEN;
        if (Identifier_string == "then")
            return THEN_TOKEN;
        if (Identifier_string == "else")
            return ELSE_TOKEN;
        if (Identifier_string == "for")
            return FOR_TOKEN;
        if (Identifier_string == "in")
            return IN_TOKEN;
        if (Identifier_string == "binary")
            return BINARY_TOKEN;
        if (Identifier_string == "unary")
            return UNARY_TOKEN;

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

    switch (atoi(Bin_Operator.c_str()))
    {
    case '<':
        L = Builder.CreateICmpULT(L, R, "cmptmp");
        return Builder.CreateZExt(L, llvm::Type::getInt32Ty(TheGlobalContext), "booltmp");
    case '+':
        return Builder.CreateAdd(L, R, "addtmp");
    case '-':
        return Builder.CreateSub(L, R, "subtmp");
    case '*':
        return Builder.CreateMul(L, R, "multmp");
    case '/':
        return Builder.CreateUDiv(L, R, "divtmp");

    default:
        break;
    }
    llvm::Function *F = Module_ob->getFunction(std::string("binary") + Bin_Operator.c_str());
    llvm::Value *Ops[2] = {L, R};
    return Builder.CreateCall(F, Ops, "binop");
}

class FunctionDeclAST
{
    std::string Func_Name;
    std::vector<std::string> Arguments;
    bool isOperator;
    unsigned Precedence;

public:
    FunctionDeclAST(const std::string &name,
                    const std::vector<std::string> &args,
                    bool isoperator = false,
                    unsigned prec = 0) : Func_Name(name), Arguments(args), isOperator(isoperator), Precedence(prec) {}

    bool isUnaryOp() const { return isOperator && Arguments.size() == 1; }
    bool isBinaryOp() const { return isOperator && Arguments.size() == 2; }

    char getOperatorName() const
    {
        assert(isUnaryOp() || isBinaryOp());
        return Func_Name[Func_Name.size() - 1];
    }

    unsigned getBinaryPrecedence() const
    {
        return Precedence;
    }

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

    if (Func_Decl->isBinaryOp())
        Operator_Precedence[Func_Decl->getOperatorName()] = Func_Decl->getBinaryPrecedence();

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

class ExprIfAST : public BaseAST
{
    BaseAST *Cond, *Then, *Else;

public:
    ExprIfAST(BaseAST *cond, BaseAST *then, BaseAST *else_st) : Cond(cond), Then(then), Else(else_st) {}
    llvm::Value *codegen() override;
};

llvm::Value *ExprIfAST::codegen()
{
    llvm::Value *Condtn = Cond->codegen();
    if (Condtn == 0)
        return 0;
    Condtn = Builder.CreateICmpNE(Condtn, Builder.getInt32(0), "ifcond");

    llvm::Function *TheFunc = Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(TheGlobalContext, "then", TheFunc);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(TheGlobalContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(TheGlobalContext, "ifcont");

    Builder.CreateCondBr(Condtn, ThenBB, ElseBB);

    Builder.SetInsertPoint(ThenBB);
    llvm::Value *ThenV = Then->codegen();
    if (!ThenV)
        return 0;
    Builder.CreateBr(MergeBB);
    // 这条语句加不加都一样，并没有修改ThenBB的值
    ThenBB = Builder.GetInsertBlock();

    TheFunc->getBasicBlockList().push_back(ElseBB);
    Builder.SetInsertPoint(ElseBB);
    llvm::Value *ElseV = Else->codegen();
    if (!ElseV)
        return 0;
    Builder.CreateBr(MergeBB);
    // 这条语句加不加都一样，并没有修改ThenBB的值
    ElseBB = Builder.GetInsertBlock();

    TheFunc->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    llvm::PHINode *PN = Builder.CreatePHI(llvm::Type::getInt32Ty(TheGlobalContext), 2, "iftmp");

    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    return PN;
}

class ExprForAST : public BaseAST
{
    std::string Var_Name;
    BaseAST *Start, *Step, *End, *Body;

public:
    ExprForAST(const std::string &var_name,
               BaseAST *start,
               BaseAST *step,
               BaseAST *end,
               BaseAST *body) : Var_Name(var_name), Start(start), Step(step), End(end), Body(body) {}
    llvm::Value *codegen() override;
};

llvm::Value *ExprForAST::codegen()
{
    llvm::Value *StartVal = Start->codegen();
    if (StartVal == 0)
        return 0;

    llvm::Function *TheFunction = Builder.GetInsertBlock()->getParent();
    llvm::BasicBlock *PreheaderBB = Builder.GetInsertBlock();
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(TheGlobalContext, "loop", TheFunction);

    // 直接跳到循环体LoopBB
    Builder.CreateBr(LoopBB);

    Builder.SetInsertPoint(LoopBB);
    llvm::PHINode *Variable = Builder.CreatePHI(llvm::Type::getInt32Ty(TheGlobalContext), 2, Var_Name.c_str());
    // 来自初始条件
    Variable->addIncoming(StartVal, PreheaderBB);

    llvm::Value *OldVal = Named_Values[Var_Name];
    Named_Values[Var_Name] = Variable;

    // 循环体的生成
    if (Body->codegen() == 0)
        return 0;

    // 步进条件的生成
    llvm::Value *StepVal;
    if (Step)
    {
        // 步进值的生成
        StepVal = Step->codegen();
        if (StepVal == 0)
            return 0;
    }
    else
    {
        // 默认情况下，步进为1
        StepVal = llvm::ConstantInt::get(llvm::Type::getInt32Ty(TheGlobalContext), 1);
    }

    // 步进代码的生成
    llvm::Value *NextVar = Builder.CreateAdd(Variable, StepVal, "nextvar");
    // 循环判断条件的生成
    llvm::Value *EndCond = End->codegen();
    if (EndCond == 0)
        return 0;

    // 不满足循环判断条件，跳转到循环结束代码
    EndCond =
        Builder.CreateICmpNE(EndCond, llvm::ConstantInt::get(llvm::Type::getInt32Ty(TheGlobalContext), 0), "loopcond");

    llvm::BasicBlock *LoopEndBB = Builder.GetInsertBlock();
    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(TheGlobalContext, "afterloop", TheFunction);
    Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
    Builder.SetInsertPoint(AfterBB);
    // 来自循环体
    Variable->addIncoming(NextVar, LoopEndBB);

    if (OldVal)
        Named_Values[Var_Name] = OldVal;
    else
        Named_Values.erase(Var_Name);

    return llvm::ConstantInt::getNullValue(llvm::Type::getInt32Ty(TheGlobalContext));
}

class ExprUnaryAST : public BaseAST
{
    char Opcode;
    BaseAST *Operand;

public:
    ExprUnaryAST(char op, BaseAST *operand) : Opcode(op), Operand(operand) {}
    virtual llvm::Value *codegen();
};

llvm::Value *ExprUnaryAST::codegen()
{
    llvm::Value *OperandV = Operand->codegen();
    if (OperandV == 0)
        return 0;

    llvm::Function *F = Module_ob->getFunction(std::string("unary") + Opcode);
    if (F == nullptr)
        return nullptr;

    return Builder.CreateCall(F, OperandV, "tmp");
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
static BaseAST *If_parser();
static BaseAST *For_parser();
static BaseAST *unary_parser();

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
    case IF_TOKEN:
    {
        return If_parser();
    }
    case FOR_TOKEN:
    {
        return For_parser();
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
    BaseAST *LHS = unary_parser();
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
    std::string FnName;

    unsigned Kind = 0;
    unsigned BinaryPrecedence = 30;

    switch (Current_Token)
    {
    case IDENTIFIER_TOKEN:
        FnName = Identifier_string;
        Kind = 0;
        next_token();
        break;

    case UNARY_TOKEN:
        next_token(); // eat '!'
        if (!isascii(Current_Token))
            return 0; // error: invalid unary operator
        FnName = "unary";
        FnName += (char)Current_Token;
        Kind = 1;
        next_token();
        break;

    case BINARY_TOKEN:
        next_token(); // eat binary operator
        if (!isascii(Current_Token))
            return 0;
        FnName = "binary";
        FnName += (char)Current_Token;
        Kind = 2;
        next_token();

        if (Current_Token == NUMERIC_TOKEN)
        {
            if (Numeric_Val < 1 || Numeric_Val > 100)
                return 0;
            BinaryPrecedence = (unsigned)Numeric_Val;
            next_token(); // eat
        }
        break;

    default:
        return 0;
    }

    if (Current_Token != '(')
        return 0; // error: expected '('

    std::vector<std::string> Function_Argument_Names;
    while (next_token() == IDENTIFIER_TOKEN)
        Function_Argument_Names.push_back(Identifier_string);

    if (Current_Token != ')')
        return 0; // error: expected ')'

    next_token(); // eat ')'

    if (Kind && Function_Argument_Names.size() != Kind)
        return 0;

    return new FunctionDeclAST(FnName, Function_Argument_Names, Kind != 0, BinaryPrecedence);
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

static BaseAST *If_parser()
{
    next_token();

    BaseAST *Cond = expression_parser();
    if (!Cond)
        return 0;

    if (Current_Token != THEN_TOKEN)
        return 0; // error: expected
    next_token();

    BaseAST *Then = expression_parser();
    if (Then == 0)
        return 0;

    if (Current_Token != ELSE_TOKEN)
        return 0; // error: expected
    next_token();

    BaseAST *Else = expression_parser();
    if (!Else)
        return 0;

    return new ExprIfAST(Cond, Then, Else);
}

static BaseAST *For_parser()
{
    next_token(); // eat 'for'

    if (Current_Token != IDENTIFIER_TOKEN)
        return 0;

    std::string IdName = Identifier_string;

    next_token(); // eat '='

    if (Current_Token != '=')
        return 0;

    next_token();

    BaseAST *Start = expression_parser();
    if (Start == 0)
        return 0;
    if (Current_Token != ',')
        return 0;

    next_token();

    BaseAST *End = expression_parser();
    if (End == 0)
        return 0;

    BaseAST *Step = nullptr;
    if (Current_Token == ',')
    {
        next_token();
        Step = expression_parser();
        if (Step == 0)
            return 0;
    }

    if (Current_Token != IN_TOKEN)
        return 0; // error: expected 'in'

    next_token();

    BaseAST *Body = expression_parser();
    if (Body == 0)
        return 0;

    return new ExprForAST(IdName, Start, Step, End, Body);
}

static BaseAST *unary_parser()
{
    if (isascii(Current_Token) || Current_Token == '(' || Current_Token == ',')
        return Base_Parser();

    int Op = Current_Token;

    next_token();

    if (BaseAST *Operand = unary_parser())
        return new ExprUnaryAST(Op, Operand);

    return 0;
}

static void init_precedence()
{
    Operator_Precedence['<'] = 0;
    Operator_Precedence['-'] = 1;
    Operator_Precedence['+'] = 2;
    Operator_Precedence['/'] = 3;
    Operator_Precedence['*'] = 4;
}

static int getBinOpPrecedence()
{
    // if (!isascii(Current_Token))
    //     return -1;

    if (Operator_Precedence.find(Current_Token) == Operator_Precedence.end()) // not found)
        return -1;                                                            // not a recognized operator

    int TokPrec = Operator_Precedence[Current_Token];
    if (TokPrec < 0)
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

        BaseAST *RHS = unary_parser();
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
            // LF->dump();
            // void *FPtr = TheExecutionEngine->getPointerToFunction(LF);
            // int (*Int)() = (int (*)())(intptr_t)FPtr;
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

    // llvm::EngineBuilder EB;
    // TheExecutionEngine = EB.create();

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