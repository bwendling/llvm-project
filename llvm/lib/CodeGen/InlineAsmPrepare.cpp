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
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/DerivedTypes.h"
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
    });
  });

  return InlineAsms;
}

static bool isRegMemConstraint(StringRef Constraint) {
  // Strip prefixes like '=', '+', '*'
  while (!Constraint.empty() && (Constraint[0] == '=' ||
                                Constraint[0] == '+' ||
                                Constraint[0] == '*')) {
    Constraint = Constraint.substr(1);
  }
  // Check for "rm" now.
  bool Result = Constraint.size() == 2 && Constraint.contains('r') && Constraint.contains('m');
  return Result;
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
  if (!F.getParent()->getTargetTriple().isX86())
    return false;

  SmallVector<CallBase *, 4> IAs = findInlineAsms(F);
  if (IAs.empty())
    return false;

  // errs() << "OLD: "; F.dump();

  bool Changed = false;
  for (CallBase *CB : IAs) {
    InlineAsm *IA = cast<InlineAsm>(CB->getCalledOperand());
    const InlineAsm::ConstraintInfoVector &Constraints = IA->ParseConstraints();

    bool HasRM = false;
    StringRef OriginalConstraintStr = IA->getConstraintString();
    auto StrI = OriginalConstraintStr.begin(), StrE = OriginalConstraintStr.end();

    while (StrI != StrE) {
      if (*StrI == '=') {
        ++StrI;
      }
      if (*StrI == '*') {
        ++StrI;
      }
      if (*StrI == '+') {
        ++StrI;
      }

      auto Comma = std::find(StrI, StrE, ',');
      StringRef Sub(StrI, std::distance(StrI, Comma));
      if (isRegMemConstraint(Sub)) {
        HasRM = true;
        break;
      }

      if (Comma == StrE)
        break;

      StrI = Comma + 1;
    }

    if (!HasRM)
      continue;

    std::string NewConstraintStr =
        convertConstraintsToMemory(IA->getConstraintString());
    if (NewConstraintStr == IA->getConstraintString())
      continue;

    Changed = true;

    IRBuilder<> Builder(CB);
    IRBuilder<> EntryBuilder(&F.getEntryBlock(), F.getEntryBlock().begin());

    unsigned ArgNo = 0;
    unsigned OutputIdx = 0;
    for (const auto &C : Constraints) {
      if (!C.Codes.empty() && isRegMemConstraint(C.Codes[0])) {
        Type *SlotTy = nullptr;
        if (C.Type == InlineAsm::isOutput && !C.hasMatchingInput()) {
          // Output-only
          Type *RetTy = CB->getType();
          if (StructType *ST = dyn_cast<StructType>(RetTy)) {
            SlotTy = ST->getElementType(OutputIdx);
          } else {
            SlotTy = RetTy;
          }
        } else {
          // Input or Read-Write
          SlotTy = CB->getArgOperand(ArgNo)->getType();
        }

        AllocaInst *Slot = EntryBuilder.CreateAlloca(SlotTy, nullptr, "asm_mem");

        if (C.Type == InlineAsm::isInput ||
            (C.Type == InlineAsm::isOutput && C.hasMatchingInput())) {
          // Input part of input-only or read-write
          Builder.CreateStore(CB->getArgOperand(ArgNo), Slot);
        }

        if (C.Type == InlineAsm::isOutput) {
          // Output part of output-only or read-write
          Instruction *InsertPt = CB->getNextNode();
          if (InvokeInst *II = dyn_cast<InvokeInst>(CB))
            InsertPt = &II->getNormalDest()->front();

          if (InsertPt) {
            IRBuilder<> PostBuilder(InsertPt);
            PostBuilder.CreateLoad(SlotTy, Slot, "asm_load");
          } else if (!CB->isTerminator()) {
            // This case should not be hit if we get NextNode
          } else {
            // Terminator, but not invoke, or invoke without normal dest.
            // Insert load before the call. This is not ideal, but it's just
            // for dumping IR.
            Builder.CreateLoad(SlotTy, Slot, "asm_load");
          }
        }
      }

      if (C.Type == InlineAsm::isOutput || C.Type == InlineAsm::isInput) {
        ArgNo++;
      }

      if (C.Type == InlineAsm::isOutput && !C.hasMatchingInput()) {
        OutputIdx++;
      }
    }

    auto *NewIA = InlineAsm::get(
        IA->getFunctionType(), IA->getAsmString(), NewConstraintStr,
        IA->hasSideEffects(), IA->isAlignStack(), IA->getDialect(),
        IA->canThrow());
    errs() << "OLD: ";
    IA->dump();
    errs() << "NEW: ";
    NewIA->dump();
  }

  if (Changed) {
    errs() << "NEW: "; F.dump();
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
