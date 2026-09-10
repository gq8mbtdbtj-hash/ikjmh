/**
 * @file memprobe_install.cpp
 * @brief 无编译注入后：用真 IAT/PLT hook 安装采集点（heap/mmap/扩展域）。
 *
 * 与 alloc_interpose.cpp 的关系：
 *   - Windows：仅靠本文件（IAT）；无 LD_PRELOAD 等价物
 *   - ELF：本文件改 GOT；LD_PRELOAD 时还可叠加符号导出（双重保护）
 *
 * Proxy 约定：先/后调用 tray_hooks_get_prev(本 proxy)，再 OnAlloc/OnFree。
 * free 路径先记账再真 free，避免 free 后栈上仍引用脏指针去解符号。
 *
 * GPU：默认走 memprobe_gpu.cpp 精确 ABI（CUDA/OpenCL/Vulkan）。
 * TRAY_MEMPROBE_GPU_SYMS 额外符号仍用 4 参占位（size=0）。
 * NPU/NEON：占位或 domain_alloc。
 */

#include "tray_hooks/hooks.h"
#include "tray_hooks/collector.h"
#include "tray_hooks/memprobe.h"
#include "memprobe_internal.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/types.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

bool CollectEnabled() {
  static int cached = -1;
  if (cached < 0) {
    const char* s = std::getenv("TRAY_MEMPROBE_COLLECT");
    cached = (s && (*s == '1' || *s == 'y' || *s == 'Y')) ? 1 : 0;
  }
  return cached == 1;
}

void CollectAlloc(const char* tag, size_t n) {
  if (CollectEnabled()) {
    tray_hooks_collector_record(tag, NULL, 0, static_cast<uint64_t>(n));
  }
}

typedef void* (*malloc_fn)(size_t);
typedef void (*free_fn)(void*);
typedef void* (*calloc_fn)(size_t, size_t);
typedef void* (*realloc_fn)(void*, size_t);

#if !defined(_WIN32)
typedef void* (*mmap_fn)(void*, size_t, int, int, int, off_t);
typedef int (*munmap_fn)(void*, size_t);
#else
typedef LPVOID(WINAPI* virtual_alloc_fn)(LPVOID, SIZE_T, DWORD, DWORD);
typedef BOOL(WINAPI* virtual_free_fn)(LPVOID, SIZE_T, DWORD);
#endif

void* ProxyMalloc(size_t n) {
  // CALL_PREV：走 hook 前登记的真实 malloc（勿直接调 malloc，会死递归）
  malloc_fn prev = reinterpret_cast<malloc_fn>(tray_hooks_get_prev((void*)ProxyMalloc));
  void* p = prev ? prev(n) : 0;
  CollectAlloc("malloc", n);
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, p, n);
  return p;
}

void ProxyFree(void* p) {
  tray_memprobe::detail::OnFree(TRAY_MEM_HEAP, p);
  free_fn prev = reinterpret_cast<free_fn>(tray_hooks_get_prev((void*)ProxyFree));
  if (prev) {
    prev(p);
  }
}

void* ProxyCalloc(size_t a, size_t b) {
  calloc_fn prev = reinterpret_cast<calloc_fn>(tray_hooks_get_prev((void*)ProxyCalloc));
  void* p = prev ? prev(a, b) : 0;
  CollectAlloc("calloc", a * b);
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, p, a * b);
  return p;
}

void* ProxyRealloc(void* p, size_t n) {
  if (p) {
    tray_memprobe::detail::OnFree(TRAY_MEM_HEAP, p);
  }
  realloc_fn prev =
      reinterpret_cast<realloc_fn>(tray_hooks_get_prev((void*)ProxyRealloc));
  void* q = prev ? prev(p, n) : 0;
  CollectAlloc("realloc", n);
  tray_memprobe::detail::OnAlloc(TRAY_MEM_HEAP, q, n);
  return q;
}

#if !defined(_WIN32)
void* ProxyMmap(void* addr, size_t len, int prot, int flags, int fd, off_t off) {
  mmap_fn prev = reinterpret_cast<mmap_fn>(tray_hooks_get_prev((void*)ProxyMmap));
  void* p = prev ? prev(addr, len, prot, flags, fd, off) : 0;
  if (p && p != reinterpret_cast<void*>(-1)) {
    CollectAlloc("mmap", len);
    tray_memprobe::detail::OnAlloc(TRAY_MEM_MMAP, p, len);
  }
  return p;
}

int ProxyMunmap(void* addr, size_t len) {
  tray_memprobe::detail::OnFree(TRAY_MEM_MMAP, addr);
  munmap_fn prev =
      reinterpret_cast<munmap_fn>(tray_hooks_get_prev((void*)ProxyMunmap));
  return prev ? prev(addr, len) : -1;
}
#else
LPVOID WINAPI ProxyVirtualAlloc(LPVOID a, SIZE_T s, DWORD t, DWORD p) {
  virtual_alloc_fn prev = reinterpret_cast<virtual_alloc_fn>(
      tray_hooks_get_prev((void*)ProxyVirtualAlloc));
  LPVOID r = prev ? prev(a, s, t, p) : 0;
  if (r) {
    CollectAlloc("VirtualAlloc", static_cast<size_t>(s));
    tray_memprobe::detail::OnAlloc(TRAY_MEM_MMAP, r, static_cast<size_t>(s));
  }
  return r;
}

BOOL WINAPI ProxyVirtualFree(LPVOID a, SIZE_T s, DWORD t) {
  tray_memprobe::detail::OnFree(TRAY_MEM_MMAP, a);
  virtual_free_fn prev = reinterpret_cast<virtual_free_fn>(
      tray_hooks_get_prev((void*)ProxyVirtualFree));
  return prev ? prev(a, s, t) : FALSE;
}
#endif

/** 自定义 GPU 符号占位（未知 ABI，只能记事件） */
void* ProxyGpuAllocGeneric(void* a, void* b, void* c, void* d) {
  typedef void* (*fn4)(void*, void*, void*, void*);
  fn4 prev = reinterpret_cast<fn4>(
      tray_hooks_get_prev(reinterpret_cast<void*>(ProxyGpuAllocGeneric)));
  void* p = prev ? prev(a, b, c, d) : 0;
  if (p) {
    tray_memprobe::detail::OnAlloc(TRAY_MEM_GPU, p, 0);
  }
  return p;
}

void* ProxyNpuAlloc(void* a, void* b, void* c, void* d) {
  typedef void* (*fn4)(void*, void*, void*, void*);
  fn4 prev = reinterpret_cast<fn4>(tray_hooks_get_prev((void*)ProxyNpuAlloc));
  void* p = prev ? prev(a, b, c, d) : 0;
  if (p) {
    tray_memprobe::detail::OnAlloc(TRAY_MEM_NPU, p, 0);
  }
  return p;
}

void* ProxyNeonAlloc(void* a, void* b, void* c, void* d) {
  typedef void* (*fn4)(void*, void*, void*, void*);
  fn4 prev = reinterpret_cast<fn4>(tray_hooks_get_prev((void*)ProxyNeonAlloc));
  void* p = prev ? prev(a, b, c, d) : 0;
  if (p) {
    tray_memprobe::detail::OnAlloc(TRAY_MEM_NEON, p, 0);
  }
  return p;
}

/** callee_dll==NULL：不限导入 DLL（Win CRT 走 api-ms-win-crt-* 时必需） */
void HookOne(const char* callee_dll, const char* sym, void* proxy) {
  tray_hooks_hook_all(callee_dll, sym, proxy, 0, 0);
}

/** 解析逗号分隔符号列表并 hook */
void HookSymList(const char* list, void* proxy) {
  if (!list || !*list || !proxy) {
    return;
  }
  std::string s(list);
  size_t start = 0;
  while (start < s.size()) {
    size_t comma = s.find(',', start);
    std::string sym = s.substr(
        start, comma == std::string::npos ? std::string::npos : comma - start);
    if (!sym.empty()) {
      HookOne(0, sym.c_str(), proxy);
    }
    if (comma == std::string::npos) {
      break;
    }
    start = comma + 1;
  }
}

/**
 * GPU 精确 hook + 可选自定义符号；NPU/NEON 仅环境变量。
 */
void HookGpuNpuFromEnv() {
  tray_memprobe::gpu::InstallPreciseGpuHooks();

  const char* gpu = std::getenv("TRAY_MEMPROBE_GPU_SYMS");
  if (tray_memprobe::detail::DomainEnabled(TRAY_MEM_GPU) && gpu && *gpu) {
    HookSymList(gpu, reinterpret_cast<void*>(ProxyGpuAllocGeneric));
  }

  const char* npu = std::getenv("TRAY_MEMPROBE_NPU_SYMS");
  if (tray_memprobe::detail::DomainEnabled(TRAY_MEM_NPU) && npu && *npu) {
    HookSymList(npu, reinterpret_cast<void*>(ProxyNpuAlloc));
  }

  const char* neon = std::getenv("TRAY_MEMPROBE_NEON_SYMS");
  if (tray_memprobe::detail::DomainEnabled(TRAY_MEM_NEON) && neon && *neon) {
    HookSymList(neon, reinterpret_cast<void*>(ProxyNeonAlloc));
  }
}

}  // namespace

extern "C" void tray_memprobe_install_hooks(void) {
  // 必须先 init：后续 HookOne → Backend::hook
  if (tray_hooks_init(TRAY_HOOKS_MODE_AUTOMATIC) != TRAY_HOOKS_OK) {
    return;
  }

  if (tray_memprobe::detail::DomainEnabled(TRAY_MEM_HEAP)) {
#if defined(_WIN32)
    // 现代 MSVC 常从 api-ms-win-crt-*.dll 导入，勿写死 ucrtbase
    HookOne(0, "malloc", (void*)ProxyMalloc);
    HookOne(0, "free", (void*)ProxyFree);
    HookOne(0, "calloc", (void*)ProxyCalloc);
    HookOne(0, "realloc", (void*)ProxyRealloc);
#else
    HookOne(0, "malloc", (void*)ProxyMalloc);
    HookOne(0, "free", (void*)ProxyFree);
    HookOne(0, "calloc", (void*)ProxyCalloc);
    HookOne(0, "realloc", (void*)ProxyRealloc);
#endif
  }

  if (tray_memprobe::detail::DomainEnabled(TRAY_MEM_MMAP)) {
#if defined(_WIN32)
    HookOne(0, "VirtualAlloc", (void*)ProxyVirtualAlloc);
    HookOne(0, "VirtualFree", (void*)ProxyVirtualFree);
#else
    HookOne(0, "mmap", (void*)ProxyMmap);
    HookOne(0, "munmap", (void*)ProxyMunmap);
#endif
  }

  HookGpuNpuFromEnv();
  tray_memprobe_sample_cpu();
}
