//===-- Differential test for log1pf --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PerfTest.h"
#include "src/math/log1pf.h"

#include <math.h>

int main() {
  SINGLE_INPUT_SINGLE_OUTPUT_PERF(float, LIBC_NAMESPACE::log1pf, ::log1pf,
                                  "log1pf_perf.log")
  return 0;
}
