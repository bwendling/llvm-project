//===-- InlineAsmPrepare - Prepare inline asm for code generation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/InlineAsmPrepare.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include <sstream>

using namespace llvm;

#define DEBUG_TYPE "inline-asm-prepare"

namespace {

class InlineAsmPrepare : public FunctionPass {
  InlineAsmPrepare(InlineAsmPrepare &) = delete;

public:
  InlineAsmPrepare() : FunctionPass(ID) {}

  void getAnalysisUsage(AnalysisUsage &AU) const override {}
  bool runOnFunction(Function &F) override;

  static char ID;
};

char InlineAsmPrepare::ID = 0;

} // end anonymous namespace

INITIALIZE_PASS(InlineAsmPrepare, DEBUG_TYPE,
                "Convert inline asm \"rm\" insts for fast register allocation",
                false, false)
FunctionPass *llvm::createInlineAsmPass() { return new InlineAsmPrepare(); }

// For each inline asm, the "rm" constraint needs to default to "m" for the
// fast register allocator.
static SmallVector<CallBase *, 4> findInlineAsms(Function &F) {
  SmallVector<CallBase *, 4> InlineAsms;

  for_each(F, [&](BasicBlock &BB) {
    for_each(BB, [&](Instruction &I) {
      CallBase *CB = dyn_cast<CallBase>(&I);
      if (!CB || !CB->isInlineAsm())
        return;
      InlineAsms.push_back(CB);
      CB->dump();
    });
  });

  return InlineAsms;
}

static bool isRegMemConstraint(StringRef Constraint) {
  return Constraint.size() == 2 && is_contained(Constraint, "r") &&
    is_contained(Constraint, "m");
}

// Convert instances of the "rm" constraints into "m".
static std::string convertConstraintsToMemory(StringRef ConstraintStr) {
  auto I = ConstraintStr.begin(), E = ConstraintStr.end();
  std::ostringstream Out;

  while (I != E) {
    if (*I == '=') {
      Out << *I;
      ++I;
    }
    if (*I == '*') {
      Out << '*';
      ++I;
    }
    if (*I == '+') {
      Out << '+';
      ++I;
    }

    auto Comma = std::find(I, E, ',');
    std::string Sub(I, Comma);
    if (isRegMemConstraint(Sub))
      Out << 'm';
    else
      Out << Sub;

    if (Comma == E)
      break;

    Out << ',';
    I = Comma + 1;
  }

  return Out.str();
}

bool InlineAsmPrepare::runOnFunction(Function &F) {
  // Only process "rm" on x86 platforms.
  if (F.getParent()->getTargetTriple().getArch() != Triple::x86 &&
      F.getParent()->getTargetTriple().getArch() != Triple::x86_64)
    return false;

  SmallVector<CallBase *, 4> IAs = findInlineAsms(F);
  if (IAs.empty())
    return false;

  bool Changed = false;
  for (CallBase *CB : IAs) {
    InlineAsm *IA = cast<InlineAsm>(CB->getCalledOperand());
    InlineAsm::ConstraintInfoVector Constraints = IA->ParseConstraints();

#if 0
    for (auto &C : Constraints) {
      errs() << "Type: " << C.Type << "\n"
             << "Early Clobber: " << C.isEarlyClobber << "\n"
             << "Matching Input: " << C.MatchingInput << "\n"
             << "Commutative: " << C.isCommutative << "\n"
             << "Indirect: " << C.isIndirect << "\n"
             << "Multiple Alternative: " << C.isMultipleAlternative << "\n";

      errs() << "Constraint Codes:\n";
      for (auto Code : C.Codes)
        errs() << "\t" << Code << "\n";

      for (auto &CI : C.multipleAlternatives) {
        errs() << "Multiple Alt Constraints:\n";
        for (auto Code : CI.Codes)
          errs() << "\t" << Code << "\n";
      }

      if (C.Codes.size() == 2 && is_contained(C.Codes, "r") &&
          is_contained(C.Codes, "m")) {
        errs() << "Needs processing.\n";
      }
    }
#endif

    std::string NewConstraintStr =
        convertConstraintsToMemory(IA->getConstraintString());

    errs() << "OLD CONSTRAINT STR: " << IA->getConstraintString() << "\n";
    errs() << "NEW CONSTRAINT STR: " << NewConstraintStr << "\n";
  }

  return Changed;
}

PreservedAnalyses InlineAsmPreparePass::run(Function &F,
                                            FunctionAnalysisManager &FAM) {
  InlineAsmPrepare IAP;

  bool Changed = IAP.runOnFunction(F);
  if (!Changed)
    return PreservedAnalyses::all();

  return PreservedAnalyses::all();
}
