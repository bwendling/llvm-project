//===--- BPF.cpp - Implement BPF target feature support -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements BPF TargetInfo objects.
//
//===----------------------------------------------------------------------===//

#include "BPF.h"
#include "clang/Basic/MacroBuilder.h"
#include "clang/Basic/TargetBuiltins.h"
#include "llvm/ADT/StringRef.h"

using namespace clang;
using namespace clang::targets;

static constexpr int NumBuiltins =
    clang::BPF::LastTSBuiltin - Builtin::FirstTSBuiltin;

#define GET_BUILTIN_STR_TABLE
#include "clang/Basic/BuiltinsBPF.inc"
#undef GET_BUILTIN_STR_TABLE

static constexpr Builtin::Info BuiltinInfos[] = {
#define GET_BUILTIN_INFOS
#include "clang/Basic/BuiltinsBPF.inc"
#undef GET_BUILTIN_INFOS
};
static_assert(std::size(BuiltinInfos) == NumBuiltins);

void BPFTargetInfo::getTargetDefines(const LangOptions &Opts,
                                     MacroBuilder &Builder) const {
  Builder.defineMacro("__bpf__");
  Builder.defineMacro("__BPF__");

  std::string CPU = getTargetOpts().CPU;
  if (CPU == "probe") {
    Builder.defineMacro("__BPF_CPU_VERSION__", "0");
    return;
  }

  Builder.defineMacro("__BPF_FEATURE_ADDR_SPACE_CAST");
  Builder.defineMacro("__BPF_FEATURE_MAY_GOTO");
  Builder.defineMacro("__BPF_FEATURE_ATOMIC_MEM_ORDERING");

  if (CPU.empty())
    CPU = "v3";

  if (CPU == "generic" || CPU == "v1") {
    Builder.defineMacro("__BPF_CPU_VERSION__", "1");
    return;
  }

  std::string CpuVerNumStr = CPU.substr(1);
  Builder.defineMacro("__BPF_CPU_VERSION__", CpuVerNumStr);

  int CpuVerNum = std::stoi(CpuVerNumStr);
  if (CpuVerNum >= 2)
    Builder.defineMacro("__BPF_FEATURE_JMP_EXT");

  if (CpuVerNum >= 3) {
    Builder.defineMacro("__BPF_FEATURE_JMP32");
    Builder.defineMacro("__BPF_FEATURE_ALU32");
  }

  if (CpuVerNum >= 4) {
    Builder.defineMacro("__BPF_FEATURE_LDSX");
    Builder.defineMacro("__BPF_FEATURE_MOVSX");
    Builder.defineMacro("__BPF_FEATURE_BSWAP");
    Builder.defineMacro("__BPF_FEATURE_SDIV_SMOD");
    Builder.defineMacro("__BPF_FEATURE_GOTOL");
    Builder.defineMacro("__BPF_FEATURE_ST");
    Builder.defineMacro("__BPF_FEATURE_LOAD_ACQ_STORE_REL");
  }
}

static constexpr llvm::StringLiteral ValidCPUNames[] = {"generic", "v1", "v2",
                                                        "v3", "v4", "probe"};

bool BPFTargetInfo::isValidCPUName(StringRef Name) const {
  return llvm::is_contained(ValidCPUNames, Name);
}

void BPFTargetInfo::fillValidCPUList(SmallVectorImpl<StringRef> &Values) const {
  Values.append(std::begin(ValidCPUNames), std::end(ValidCPUNames));
}

llvm::SmallVector<Builtin::InfosShard>
BPFTargetInfo::getTargetBuiltins() const {
  return {{&BuiltinStrings, BuiltinInfos}};
}

bool BPFTargetInfo::handleTargetFeatures(std::vector<std::string> &Features,
                                         DiagnosticsEngine &Diags) {
  for (const auto &Feature : Features) {
    if (Feature == "+alu32") {
      HasAlu32 = true;
    }
  }

  return true;
}
