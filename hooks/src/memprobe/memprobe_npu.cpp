/**
 * @file memprobe_npu.cpp
 * @brief NPU 域精确尺寸 hook（Ascend ACL / 通用 size 参数约定）。
 *
 * 默认尝试：
 *   aclrtMalloc(void** ptr, size_t size, int policy)
 *   aclrtFree(void* ptr)
 *   aclrtMallocHost / aclrtFreeHost（记入 NPU 域，便于统一观测）
 *
 * 其它厂商符号仍用 TRAY_MEMPROBE_NPU_SYMS 占位 ABI，或 domain_alloc。
 */

#include "tray_hooks/hooks.h"
#include "tray_hooks/memprobe.h"
#include "memprobe_internal.hpp"

#include <cstddef>
#include <cstdint>

namespace tray_memprobe {
namespace npu {
namespace {

void HookOne(const char* sym, void* proxy) {
  tray_hooks_hook_all(0, sym, proxy, 0, 0);
}

typedef int (*acl_malloc_fn)(void** ptr, std::size_t size, int policy);
typedef int (*acl_free_fn)(void* ptr);

int ProxyAclrtMalloc(void** ptr, std::size_t size, int policy) {
  acl_malloc_fn prev = reinterpret_cast<acl_malloc_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyAclrtMalloc)));
  const int rc = prev ? prev(ptr, size, policy) : -1;
  if (rc == 0 && ptr && *ptr) {
    detail::OnAlloc(TRAY_MEM_NPU, *ptr, size);
  }
  return rc;
}

int ProxyAclrtMallocHost(void** ptr, std::size_t size) {
  typedef int (*fn)(void**, std::size_t);
  fn prev = reinterpret_cast<fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyAclrtMallocHost)));
  const int rc = prev ? prev(ptr, size) : -1;
  if (rc == 0 && ptr && *ptr) {
    detail::OnAlloc(TRAY_MEM_NPU, *ptr, size);
  }
  return rc;
}

int ProxyAclrtFree(void* ptr) {
  detail::OnFree(TRAY_MEM_NPU, ptr);
  acl_free_fn prev = reinterpret_cast<acl_free_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyAclrtFree)));
  return prev ? prev(ptr) : -1;
}

int ProxyAclrtFreeHost(void* ptr) {
  detail::OnFree(TRAY_MEM_NPU, ptr);
  acl_free_fn prev = reinterpret_cast<acl_free_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyAclrtFreeHost)));
  return prev ? prev(ptr) : -1;
}

}  // namespace

void InstallPreciseNpuHooks() {
  if (!detail::DomainEnabled(TRAY_MEM_NPU)) {
    return;
  }
  HookOne("aclrtMalloc", reinterpret_cast<void*>(&ProxyAclrtMalloc));
  HookOne("aclrtMallocHost", reinterpret_cast<void*>(&ProxyAclrtMallocHost));
  HookOne("aclrtFree", reinterpret_cast<void*>(&ProxyAclrtFree));
  HookOne("aclrtFreeHost", reinterpret_cast<void*>(&ProxyAclrtFreeHost));
}

}  // namespace npu
}  // namespace tray_memprobe
