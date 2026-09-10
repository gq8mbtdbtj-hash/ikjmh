/**
 * @file alloc_interpose.cpp
 * @brief ELF 符号插桩：导出 malloc 族，经 RTLD_NEXT 调真实实现。
 *
 * 适用：
 *   - `LD_PRELOAD=libtray_memprobe.so`：动态链接器优先解析到本 so
 *   - 与 PLT hook 双开：PRELOAD 覆盖「直接引用」；GOT 覆盖已绑定调用者
 *
 * 限制：
 *   - patchelf --add-needed  alone 时，若符号仍绑定 libc，本文件不生效，
 *     需配合 elf_plt_patch（见 PLATFORM.md）或同时 LD_PRELOAD
 *   - Windows 不编译本文件（用 IAT）
 *
 * new/delete 转到本文件的 malloc/free，保证 C++ 堆也进 heap 域。
 */

#include "tray_hooks/memprobe.h"
#include "memprobe_internal.hpp"

#include <cstddef>
#include <cstring>

#if !defined(_WIN32)

#include <dlfcn.h>
#include <new>
#include <pthread.h>

namespace {

typedef void* (*malloc_fn)(size_t);
typedef void (*free_fn)(void*);
typedef void* (*calloc_fn)(size_t, size_t);
typedef void* (*realloc_fn)(void*, size_t);
typedef int (*posix_memalign_fn)(void**, size_t, size_t);
typedef void* (*aligned_alloc_fn)(size_t, size_t);

malloc_fn real_malloc = 0;
free_fn real_free = 0;
calloc_fn real_calloc = 0;
realloc_fn real_realloc = 0;
posix_memalign_fn real_posix_memalign = 0;
aligned_alloc_fn real_aligned_alloc = 0;

pthread_once_t once_control = PTHREAD_ONCE_INIT;

/** 解析「下一个」libc 实现；勿用 RTLD_DEFAULT（会指回自己） */
void ResolveOnce() {
  real_malloc = reinterpret_cast<malloc_fn>(dlsym(RTLD_NEXT, "malloc"));
  real_free = reinterpret_cast<free_fn>(dlsym(RTLD_NEXT, "free"));
  real_calloc = reinterpret_cast<calloc_fn>(dlsym(RTLD_NEXT, "calloc"));
  real_realloc = reinterpret_cast<realloc_fn>(dlsym(RTLD_NEXT, "realloc"));
  real_posix_memalign =
      reinterpret_cast<posix_memalign_fn>(dlsym(RTLD_NEXT, "posix_memalign"));
  real_aligned_alloc =
      reinterpret_cast<aligned_alloc_fn>(dlsym(RTLD_NEXT, "aligned_alloc"));
}

void EnsureResolved() { pthread_once(&once_control, ResolveOnce); }

}  // namespace

extern "C" {

__attribute__((visibility("default"))) void* malloc(size_t size) {
  EnsureResolved();
  void* p = real_malloc ? real_malloc(size) : 0;
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, p, size);
  return p;
}

__attribute__((visibility("default"))) void free(void* ptr) {
  EnsureResolved();
  tray_memprobe::detail::OnFree(TRAY_MEM_HEAP, ptr);
  if (real_free) {
    real_free(ptr);
  }
}

__attribute__((visibility("default"))) void* calloc(size_t nmemb, size_t size) {
  EnsureResolved();
  void* p = 0;
  if (real_calloc) {
    p = real_calloc(nmemb, size);
  } else if (real_malloc) {
    const size_t total = nmemb * size;
    p = real_malloc(total);
    if (p) {
      std::memset(p, 0, total);
    }
  }
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, p, nmemb * size);
  return p;
}

__attribute__((visibility("default"))) void* realloc(void* ptr, size_t size) {
  EnsureResolved();
  if (ptr) {
    tray_memprobe::detail::OnFree(TRAY_MEM_HEAP, ptr);
  }
  void* p = real_realloc ? real_realloc(ptr, size) : 0;
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, p, size);
  return p;
}

__attribute__((visibility("default"))) int posix_memalign(void** memptr,
                                                          size_t alignment,
                                                          size_t size) {
  EnsureResolved();
  int rc = -1;
  if (real_posix_memalign) {
    rc = real_posix_memalign(memptr, alignment, size);
  }
  if (rc == 0 && memptr) {
    tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, *memptr, size);
  }
  return rc;
}

__attribute__((visibility("default"))) void* aligned_alloc(size_t alignment,
                                                           size_t size) {
  EnsureResolved();
  void* p = real_aligned_alloc ? real_aligned_alloc(alignment, size) : 0;
  if (!p && real_posix_memalign) {
    if (real_posix_memalign(&p, alignment, size) != 0) {
      p = 0;
    }
  }
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, p, size);
  return p;
}

}  // extern "C"

void* operator new(std::size_t size) {
  void* p = malloc(size);
  if (!p) {
    throw std::bad_alloc();
  }
  return p;
}

void* operator new[](std::size_t size) {
  void* p = malloc(size);
  if (!p) {
    throw std::bad_alloc();
  }
  return p;
}

void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
#if __cplusplus >= 201402L
void operator delete(void* p, std::size_t) noexcept { free(p); }
void operator delete[](void* p, std::size_t) noexcept { free(p); }
#endif

#endif  // !_WIN32
