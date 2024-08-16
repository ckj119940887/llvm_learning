#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <map>

#define DEBUG_TYPE "opcodeCounter"

using namespace llvm;

namespace
{
    struct CountOpcode : public FunctionPass
    {
        static char ID;
        CountOpcode() : FunctionPass(ID) {}

        std::map<std::string, int> opcodeCounter;

        virtual bool runOnFunction(Function &F) override
        {
            outs() << "Function: " << F.getName() << "\n";
            for (Function::iterator bb = F.begin(); bb != F.end(); ++bb)
            {
                for (BasicBlock::iterator i = bb->begin(); i != bb->end(); ++i)
                {
                    if (opcodeCounter.find(i->getOpcodeName()) == opcodeCounter.end())
                        opcodeCounter[i->getOpcodeName()] = 1;
                    else
                        opcodeCounter[i->getOpcodeName()]++;
                }
            }

            std::map<std::string, int>::iterator i = opcodeCounter.begin();
            std::map<std::string, int>::iterator e = opcodeCounter.end();
            while (i != e)
            {
                outs() << i->first << ": " << i->second << "\n";
                ++i;
            }

            outs() << "\n";
            opcodeCounter.clear();
            return false;
        }
    };
}

char CountOpcode::ID = 0;
static RegisterPass<CountOpcode> X("opcodeCounter", "Count LLVM IR Opcodes", false, false);