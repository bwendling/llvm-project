; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 5
; RUN: opt -passes=ipsccp -force-specialization -S < %s | FileCheck %s

define float @test(ptr %this, float %cm, i1 %0) strictfp {
; CHECK-LABEL: define float @test(
; CHECK-SAME: ptr [[THIS:%.*]], float [[CM:%.*]], i1 [[TMP0:%.*]]) #[[ATTR0:[0-9]+]] {
; CHECK-NEXT:  [[ENTRY:.*:]]
; CHECK-NEXT:    [[CMP:%.*]] = call i1 @llvm.experimental.constrained.fcmps.f32(float [[CM]], float 0.000000e+00, metadata !"ole", metadata !"fpexcept.strict")
; CHECK-NEXT:    [[CALL295:%.*]] = call float @test.specialized.1(ptr null, float 0.000000e+00, i1 false)
; CHECK-NEXT:    ret float 0.000000e+00
;
entry:
  %cmp = call i1 @llvm.experimental.constrained.fcmps.f32(float %cm, float 0.000000e+00, metadata !"ole", metadata !"fpexcept.strict") #0
  %call295 = call float @test(ptr null, float 0.000000e+00, i1 false) #0
  ret float 0.000000e+00
}

