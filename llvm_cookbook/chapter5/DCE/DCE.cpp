#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"

namespace llvm
{
    struct MYADCE : public FunctionPass
    {
        static char ID;
        MYADCE() : FunctionPass(ID)
        {
            initializeMYADCEPass(*PassRegistry::getPassRegistry());
        }

        bool runOnFunction(Function &F) override;

        void getAnalysisUsage(AnalysisUsage &AU) const override
        {
            AU.setPreservesCFG();
        }
    };
}

char MYADCE::ID = 0;
INITIALIZE_PASS(MYADCE, "myadce", "My Advanced Dead Code Elimination", false, false);