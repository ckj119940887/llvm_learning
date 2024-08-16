#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/LoopInfo.h"

using namespace llvm;

namespace
{
    struct FuncBlcokCount : public FunctionPass
    {
        static char ID;
        FuncBlcokCount() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override
        {
            LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

            errs() << "Function: " << F.getName() << "\n";

            for (Loop *const L : LI)
                countBlockInLoop(L, 0);

            return false; // Continue analyzing other functions
        }

        void countBlockInLoop(Loop *const L, unsigned nest)
        {
            unsigned num_Blocks = 0;
            Loop::block_iterator bb;

            for (bb = L->block_begin(); bb != L->block_end(); ++bb)
                num_Blocks++;

            errs() << "Loop nest level: " << nest << ", Number of blocks: " << num_Blocks << "\n";

            std::vector<Loop *> subLoops = L->getSubLoops();

            Loop::iterator j;
            for (j = subLoops.begin(); j != subLoops.end(); ++j)
                countBlockInLoop(*j, nest + 1);
        }

        virtual void getAnalysisUsage(AnalysisUsage &AU) const override
        {
            AU.addRequired<LoopInfoWrapperPass>();
        }
    };
}

char FuncBlcokCount::ID = 0;
static RegisterPass<FuncBlcokCount> X("func-block-count", "Function Count Pass", false, false);