//===-- sanitizer_allocator_dlsym.h -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Hack: Sanitizer initializer calls dlsym which may need to allocate and call
// back into uninitialized sanitizer.
//
//===----------------------------------------------------------------------===//

#ifndef SANITIZER_ALLOCATOR_DLSYM_H
#define SANITIZER_ALLOCATOR_DLSYM_H

#include "sanitizer_allocator_internal.h"
#include "sanitizer_common/sanitizer_allocator_checks.h"
#include "sanitizer_common/sanitizer_internal_defs.h"

namespace __sanitizer {

template <typename Details>
struct DlSymAllocator {
  static bool Use() {
    // Fuchsia doesn't use dlsym-based interceptors.
    return !SANITIZER_FUCHSIA && UNLIKELY(Details::UseImpl());
  }

  static bool PointerIsMine(const void *ptr) {
    // Fuchsia doesn't use dlsym-based interceptors.
    return !SANITIZER_FUCHSIA &&
           UNLIKELY(internal_allocator()->FromPrimary(ptr));
  }

  static void *Allocate(uptr size_in_bytes, uptr align = kWordSize) {
    void *ptr = InternalAlloc(size_in_bytes, nullptr, align);
    CHECK(internal_allocator()->FromPrimary(ptr));
    Details::OnAllocate(ptr, GetSize(ptr));
    return ptr;
  }

  static void *Callocate(usize nmemb, usize size) {
    void *ptr = InternalCalloc(nmemb, size);
    CHECK(internal_allocator()->FromPrimary(ptr));
    Details::OnAllocate(ptr, GetSize(ptr));
    return ptr;
  }

  static void Free(void *ptr) {
    uptr size = GetSize(ptr);
    Details::OnFree(ptr, size);
    InternalFree(ptr);
  }

  static void *Realloc(void *ptr, uptr new_size) {
    if (!ptr)
      return Allocate(new_size);
    CHECK(internal_allocator()->FromPrimary(ptr));
    if (!new_size) {
      Free(ptr);
      return nullptr;
    }
    uptr size = GetSize(ptr);
    uptr memcpy_size = Min(new_size, size);
    void *new_ptr = Allocate(new_size);
    if (new_ptr)
      internal_memcpy(new_ptr, ptr, memcpy_size);
    Free(ptr);
    return new_ptr;
  }

  static void *ReallocArray(void *ptr, uptr count, uptr size) {
    CHECK(!CheckForCallocOverflow(count, size));
    return Realloc(ptr, count * size);
  }

  static uptr GetSize(void *ptr) {
    return internal_allocator()->GetActuallyAllocatedSize(ptr);
  }

  static void OnAllocate(const void *ptr, uptr size) {}
  static void OnFree(const void *ptr, uptr size) {}
};

}  // namespace __sanitizer

#endif  // SANITIZER_ALLOCATOR_DLSYM_H
