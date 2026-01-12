//===-- InlineAsmPrepare - Prepare inline asm for code generation ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/InlineAsmPrepare.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

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

INITIALIZE_PASS_BEGIN(InlineAsmPrepare, "inlineasmprepare",
                      "Prepare inline asm", false, false)
INITIALIZE_PASS_END(InlineAsmPrepare, "inlineasmprepare", "Prepare inline asm",
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

PreservedAnalyses InlineAsmPreparePass::run(Function &F,
                                            FunctionAnalysisManager &FAM) {
  PreservedAnalyses PA;
  SmallVector<CallBase *, 4> IAs = findInlineAsms(F);

  if (IAs.empty())
    return PA;

  return PA;
}

bool InlineAsmPrepare::runOnFunction(Function &F) {
  bool Changed = false;
  SmallVector<CallBase *, 4> IAs = findInlineAsms(F);

  if (IAs.empty())
    return Changed;

  return Changed;
}
