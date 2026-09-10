/**
 * @file memprobe_gpu.cpp
 * @brief GPU 域精确尺寸 hook（CUDA Driver/Runtime、OpenCL、Vulkan）。
 *
 * 按真实 ABI 取 size / device 指针记账；不再使用「4 指针占位、size=0」。
 * 自定义符号仍可用 TRAY_MEMPROBE_GPU_SYMS + 通用占位（见 memprobe_install）。
 *
 * 成功码约定：CUDA/Vulkan 为 0；OpenCL 返回非空 cl_mem。
 */

#include "tray_hooks/hooks.h"
#include "tray_hooks/memprobe.h"
#include "memprobe_internal.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstddef>

namespace tray_memprobe {
namespace gpu {
namespace {

void HookOne(const char* sym, void* proxy) {
  tray_hooks_hook_all(0, sym, proxy, 0, 0);
}

// ---- CUDA Driver: CUresult cuMemAlloc(_v2)(CUdeviceptr*, size_t) ----
typedef int (*cu_mem_alloc_fn)(std::uint64_t* dptr, std::size_t bytesize);
typedef int (*cu_mem_free_fn)(std::uint64_t dptr);

int ProxyCuMemAlloc(std::uint64_t* dptr, std::size_t bytesize) {
  cu_mem_alloc_fn prev = reinterpret_cast<cu_mem_alloc_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyCuMemAlloc)));
  const int rc = prev ? prev(dptr, bytesize) : -1;
  if (rc == 0 && dptr && *dptr) {
    detail::OnAlloc(TRAY_MEM_GPU, reinterpret_cast<void*>(static_cast<uintptr_t>(*dptr)),
                    bytesize);
  }
  return rc;
}

int ProxyCuMemAllocV2(std::uint64_t* dptr, std::size_t bytesize) {
  cu_mem_alloc_fn prev = reinterpret_cast<cu_mem_alloc_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyCuMemAllocV2)));
  const int rc = prev ? prev(dptr, bytesize) : -1;
  if (rc == 0 && dptr && *dptr) {
    detail::OnAlloc(TRAY_MEM_GPU, reinterpret_cast<void*>(static_cast<uintptr_t>(*dptr)),
                    bytesize);
  }
  return rc;
}

int ProxyCuMemFree(std::uint64_t dptr) {
  if (dptr) {
    detail::OnFree(TRAY_MEM_GPU,
                   reinterpret_cast<void*>(static_cast<uintptr_t>(dptr)));
  }
  cu_mem_free_fn prev = reinterpret_cast<cu_mem_free_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyCuMemFree)));
  return prev ? prev(dptr) : -1;
}

int ProxyCuMemFreeV2(std::uint64_t dptr) {
  if (dptr) {
    detail::OnFree(TRAY_MEM_GPU,
                   reinterpret_cast<void*>(static_cast<uintptr_t>(dptr)));
  }
  cu_mem_free_fn prev = reinterpret_cast<cu_mem_free_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyCuMemFreeV2)));
  return prev ? prev(dptr) : -1;
}

// ---- CUDA Runtime: cudaError_t cudaMalloc(void**, size_t) ----
typedef int (*cuda_malloc_fn)(void** dev, std::size_t size);
typedef int (*cuda_free_fn)(void* dev);

int ProxyCudaMalloc(void** dev, std::size_t size) {
  cuda_malloc_fn prev = reinterpret_cast<cuda_malloc_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyCudaMalloc)));
  const int rc = prev ? prev(dev, size) : -1;
  if (rc == 0 && dev && *dev) {
    detail::OnAlloc(TRAY_MEM_GPU, *dev, size);
  }
  return rc;
}

int ProxyCudaFree(void* dev) {
  detail::OnFree(TRAY_MEM_GPU, dev);
  cuda_free_fn prev = reinterpret_cast<cuda_free_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyCudaFree)));
  return prev ? prev(dev) : -1;
}

// ---- OpenCL: clCreateBuffer(..., size_t size, ...) ----
typedef void* (*cl_create_buffer_fn)(void* context, std::uint64_t flags,
                                     std::size_t size, void* host_ptr,
                                     int* errcode_ret);
typedef int (*cl_release_fn)(void* mem);

void* ProxyClCreateBuffer(void* context, std::uint64_t flags, std::size_t size,
                          void* host_ptr, int* errcode_ret) {
  cl_create_buffer_fn prev = reinterpret_cast<cl_create_buffer_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyClCreateBuffer)));
  void* mem = prev ? prev(context, flags, size, host_ptr, errcode_ret) : 0;
  if (mem) {
    detail::OnAlloc(TRAY_MEM_GPU, mem, size);
  }
  return mem;
}

int ProxyClReleaseMemObject(void* mem) {
  detail::OnFree(TRAY_MEM_GPU, mem);
  cl_release_fn prev = reinterpret_cast<cl_release_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyClReleaseMemObject)));
  return prev ? prev(mem) : -1;
}

// ---- Vulkan: vkAllocateMemory / vkFreeMemory ----
// VkMemoryAllocateInfo: sType, pNext, allocationSize(VkDeviceSize), memoryTypeIndex
struct VkMemoryAllocateInfoLite {
  std::uint32_t sType;
  const void* pNext;
  std::uint64_t allocationSize;
  std::uint32_t memoryTypeIndex;
};

typedef int (*vk_alloc_fn)(void* device, const VkMemoryAllocateInfoLite* info,
                           const void* allocator, void** memory);
typedef void (*vk_free_fn)(void* device, void* memory, const void* allocator);

int ProxyVkAllocateMemory(void* device, const VkMemoryAllocateInfoLite* info,
                          const void* allocator, void** memory) {
  vk_alloc_fn prev = reinterpret_cast<vk_alloc_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyVkAllocateMemory)));
  const int rc = prev ? prev(device, info, allocator, memory) : -1;
  if (rc == 0 && memory && *memory && info) {
    detail::OnAlloc(TRAY_MEM_GPU, *memory,
                    static_cast<std::size_t>(info->allocationSize));
  }
  return rc;
}

void ProxyVkFreeMemory(void* device, void* memory, const void* allocator) {
  detail::OnFree(TRAY_MEM_GPU, memory);
  vk_free_fn prev = reinterpret_cast<vk_free_fn>(
      tray_hooks_get_prev(reinterpret_cast<void*>(&ProxyVkFreeMemory)));
  if (prev) {
    prev(device, memory, allocator);
  }
}

}  // namespace

void InstallPreciseGpuHooks() {
  if (!detail::DomainEnabled(TRAY_MEM_GPU)) {
    return;
  }

  // 精确 ABI（库未加载则 0 命中，无害；AUTOMATIC 下晚加载会补）
  HookOne("cuMemAlloc", reinterpret_cast<void*>(&ProxyCuMemAlloc));
  HookOne("cuMemAlloc_v2", reinterpret_cast<void*>(&ProxyCuMemAllocV2));
  HookOne("cuMemFree", reinterpret_cast<void*>(&ProxyCuMemFree));
  HookOne("cuMemFree_v2", reinterpret_cast<void*>(&ProxyCuMemFreeV2));

  HookOne("cudaMalloc", reinterpret_cast<void*>(&ProxyCudaMalloc));
  HookOne("cudaFree", reinterpret_cast<void*>(&ProxyCudaFree));

  HookOne("clCreateBuffer", reinterpret_cast<void*>(&ProxyClCreateBuffer));
  HookOne("clReleaseMemObject",
          reinterpret_cast<void*>(&ProxyClReleaseMemObject));

  HookOne("vkAllocateMemory", reinterpret_cast<void*>(&ProxyVkAllocateMemory));
  HookOne("vkFreeMemory", reinterpret_cast<void*>(&ProxyVkFreeMemory));
}

}  // namespace gpu
}  // namespace tray_memprobe
