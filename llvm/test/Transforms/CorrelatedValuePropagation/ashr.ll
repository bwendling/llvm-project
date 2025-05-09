; NOTE: Assertions have been autogenerated by utils/update_test_checks.py UTC_ARGS: --version 5
; RUN: opt < %s -passes=correlated-propagation -S | FileCheck %s

define void @test1(i32 %n) {
; CHECK-LABEL: define void @test1(
; CHECK-SAME: i32 [[N:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*]]:
; CHECK-NEXT:    br label %[[FOR_COND:.*]]
; CHECK:       [[FOR_COND]]:
; CHECK-NEXT:    [[A:%.*]] = phi i32 [ [[N]], %[[ENTRY]] ], [ [[SHR:%.*]], %[[FOR_BODY:.*]] ]
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[A]], 1
; CHECK-NEXT:    br i1 [[CMP]], label %[[FOR_BODY]], label %[[FOR_END:.*]]
; CHECK:       [[FOR_BODY]]:
; CHECK-NEXT:    [[SHR]] = lshr i32 [[A]], 5
; CHECK-NEXT:    br label %[[FOR_COND]]
; CHECK:       [[FOR_END]]:
; CHECK-NEXT:    ret void
;
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %a = phi i32 [ %n, %entry ], [ %shr, %for.body ]
  %cmp = icmp sgt i32 %a, 1
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %shr = ashr i32 %a, 5
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret void
}

;; Negative test to show transform doesn't happen unless n > 0.
define void @test2(i32 %n) {
; CHECK-LABEL: define void @test2(
; CHECK-SAME: i32 [[N:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*]]:
; CHECK-NEXT:    br label %[[FOR_COND:.*]]
; CHECK:       [[FOR_COND]]:
; CHECK-NEXT:    [[A:%.*]] = phi i32 [ [[N]], %[[ENTRY]] ], [ [[SHR:%.*]], %[[FOR_BODY:.*]] ]
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[A]], -2
; CHECK-NEXT:    br i1 [[CMP]], label %[[FOR_BODY]], label %[[FOR_END:.*]]
; CHECK:       [[FOR_BODY]]:
; CHECK-NEXT:    [[SHR]] = ashr i32 [[A]], 2
; CHECK-NEXT:    br label %[[FOR_COND]]
; CHECK:       [[FOR_END]]:
; CHECK-NEXT:    ret void
;
entry:
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %a = phi i32 [ %n, %entry ], [ %shr, %for.body ]
  %cmp = icmp sgt i32 %a, -2
  br i1 %cmp, label %for.body, label %for.end

for.body:                                         ; preds = %for.cond
  %shr = ashr i32 %a, 2
  br label %for.cond

for.end:                                          ; preds = %for.cond
  ret void
}

;; Non looping test case.
define void @test3(i32 %n) {
; CHECK-LABEL: define void @test3(
; CHECK-SAME: i32 [[N:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*:]]
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[N]], 0
; CHECK-NEXT:    br i1 [[CMP]], label %[[BB:.*]], label %[[EXIT:.*]]
; CHECK:       [[BB]]:
; CHECK-NEXT:    [[SHR:%.*]] = lshr exact i32 [[N]], 4
; CHECK-NEXT:    br label %[[EXIT]]
; CHECK:       [[EXIT]]:
; CHECK-NEXT:    ret void
;
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %bb, label %exit

bb:
  %shr = ashr exact i32 %n, 4
  br label %exit

exit:
  ret void
}

; looping case where loop has exactly one block
; at the point of ashr, we know that the operand is always greater than 0,
; because of the guard before it, so we can transform it to lshr.
declare void @llvm.experimental.guard(i1,...)
define void @test4(i32 %n) {
; CHECK-LABEL: define void @test4(
; CHECK-SAME: i32 [[N:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*]]:
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[N]], 0
; CHECK-NEXT:    br i1 [[CMP]], label %[[LOOP:.*]], label %[[EXIT:.*]]
; CHECK:       [[LOOP]]:
; CHECK-NEXT:    [[A:%.*]] = phi i32 [ [[N]], %[[ENTRY]] ], [ [[SHR:%.*]], %[[LOOP]] ]
; CHECK-NEXT:    [[COND:%.*]] = icmp sgt i32 [[A]], 2
; CHECK-NEXT:    call void (i1, ...) @llvm.experimental.guard(i1 [[COND]]) [ "deopt"() ]
; CHECK-NEXT:    [[SHR]] = lshr i32 [[A]], 1
; CHECK-NEXT:    br i1 [[COND]], label %[[LOOP]], label %[[EXIT]]
; CHECK:       [[EXIT]]:
; CHECK-NEXT:    ret void
;
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit

loop:
  %a = phi i32 [ %n, %entry ], [ %shr, %loop ]
  %cond = icmp sgt i32 %a, 2
  call void(i1,...) @llvm.experimental.guard(i1 %cond) [ "deopt"() ]
  %shr = ashr i32 %a, 1
  br i1 %cond, label %loop, label %exit

exit:
  ret void
}

; same test as above with assume instead of guard.
declare void @llvm.assume(i1)
define void @test5(i32 %n) {
; CHECK-LABEL: define void @test5(
; CHECK-SAME: i32 [[N:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*]]:
; CHECK-NEXT:    [[CMP:%.*]] = icmp sgt i32 [[N]], 0
; CHECK-NEXT:    br i1 [[CMP]], label %[[LOOP:.*]], label %[[EXIT:.*]]
; CHECK:       [[LOOP]]:
; CHECK-NEXT:    [[A:%.*]] = phi i32 [ [[N]], %[[ENTRY]] ], [ [[SHR:%.*]], %[[LOOP]] ]
; CHECK-NEXT:    [[COND:%.*]] = icmp samesign ugt i32 [[A]], 4
; CHECK-NEXT:    call void @llvm.assume(i1 [[COND]])
; CHECK-NEXT:    [[SHR]] = lshr i32 [[A]], 1
; CHECK-NEXT:    [[LOOPCOND:%.*]] = icmp samesign ugt i32 [[SHR]], 8
; CHECK-NEXT:    br i1 [[LOOPCOND]], label %[[LOOP]], label %[[EXIT]]
; CHECK:       [[EXIT]]:
; CHECK-NEXT:    ret void
;
entry:
  %cmp = icmp sgt i32 %n, 0
  br i1 %cmp, label %loop, label %exit

loop:
  %a = phi i32 [ %n, %entry ], [ %shr, %loop ]
  %cond = icmp sgt i32 %a, 4
  call void @llvm.assume(i1 %cond)
  %shr = ashr i32 %a, 1
  %loopcond = icmp sgt i32 %shr, 8
  br i1 %loopcond, label %loop, label %exit

exit:
  ret void
}

; check that ashr of -1 or 0 is optimized away
define i32 @test6(i32 %f, i32 %g) {
; CHECK-LABEL: define range(i32 -1, 1) i32 @test6(
; CHECK-SAME: i32 [[F:%.*]], i32 [[G:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*:]]
; CHECK-NEXT:    [[TMP0:%.*]] = add i32 [[F]], 1
; CHECK-NEXT:    [[TMP1:%.*]] = icmp ult i32 [[TMP0]], 2
; CHECK-NEXT:    tail call void @llvm.assume(i1 [[TMP1]])
; CHECK-NEXT:    ret i32 [[F]]
;
entry:
  %0 = add i32 %f, 1
  %1 = icmp ult i32 %0, 2
  tail call void @llvm.assume(i1 %1)
  %shr = ashr i32 %f, %g
  ret i32 %shr
}

; same test as above with different numbers
define i32 @test7(i32 %f, i32 %g) {
; CHECK-LABEL: define range(i32 -1, 1) i32 @test7(
; CHECK-SAME: i32 [[F:%.*]], i32 [[G:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*:]]
; CHECK-NEXT:    [[TMP0:%.*]] = and i32 [[F]], -2
; CHECK-NEXT:    [[TMP1:%.*]] = icmp eq i32 [[TMP0]], 6
; CHECK-NEXT:    tail call void @llvm.assume(i1 [[TMP1]])
; CHECK-NEXT:    [[SUB:%.*]] = add nsw i32 [[F]], -7
; CHECK-NEXT:    ret i32 [[SUB]]
;
entry:
  %0 = and i32 %f, -2
  %1 = icmp eq i32 %0, 6
  tail call void @llvm.assume(i1 %1)
  %sub = add nsw i32 %f, -7
  %shr = ashr i32 %sub, %g
  ret i32 %shr
}

; check that ashr of -2 or 1 is not optimized away
define i32 @test8(i32 %f, i32 %g, i1 %s) {
; CHECK-LABEL: define range(i32 -2, 2) i32 @test8(
; CHECK-SAME: i32 [[F:%.*]], i32 [[G:%.*]], i1 [[S:%.*]]) {
; CHECK-NEXT:  [[ENTRY:.*:]]
; CHECK-NEXT:    [[TMP0:%.*]] = ashr i32 -2, [[F]]
; CHECK-NEXT:    [[TMP1:%.*]] = lshr i32 1, [[G]]
; CHECK-NEXT:    [[TMP2:%.*]] = select i1 [[S]], i32 [[TMP0]], i32 [[TMP1]]
; CHECK-NEXT:    ret i32 [[TMP2]]
;
entry:
  %0 = ashr i32 -2, %f
  %1 = ashr i32 1, %g
  %2 = select i1 %s, i32 %0, i32 %1
  ret i32 %2
}

define i32 @may_including_undef(i1 %c.1, i1 %c.2) {
; CHECK-LABEL: define range(i32 -1073741824, 1073741824) i32 @may_including_undef(
; CHECK-SAME: i1 [[C_1:%.*]], i1 [[C_2:%.*]]) {
; CHECK-NEXT:    br i1 [[C_1]], label %[[TRUE_1:.*]], label %[[FALSE:.*]]
; CHECK:       [[TRUE_1]]:
; CHECK-NEXT:    br i1 [[C_2]], label %[[TRUE_2:.*]], label %[[EXIT:.*]]
; CHECK:       [[TRUE_2]]:
; CHECK-NEXT:    br label %[[EXIT]]
; CHECK:       [[FALSE]]:
; CHECK-NEXT:    br label %[[EXIT]]
; CHECK:       [[EXIT]]:
; CHECK-NEXT:    [[P:%.*]] = phi i32 [ 2, %[[TRUE_1]] ], [ 4, %[[TRUE_2]] ], [ undef, %[[FALSE]] ]
; CHECK-NEXT:    [[R:%.*]] = ashr i32 [[P]], 1
; CHECK-NEXT:    ret i32 [[R]]
;
  br i1 %c.1, label %true.1, label %false

true.1:
  br i1 %c.2, label %true.2, label %exit

true.2:
  br label %exit

false:
  br label %exit

exit:
  %p = phi i32 [ 2, %true.1 ], [ 4, %true.2], [ undef, %false ]
  %r = ashr i32 %p, 1
  ret i32 %r
}
