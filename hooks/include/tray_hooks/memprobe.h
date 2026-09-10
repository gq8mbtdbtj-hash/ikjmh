/**
 * @file memprobe.h
 * @brief 无编译注入采集：堆 / mmap / 扩展域内存 + CPU + 调用栈。
 *
 * ## 注入方式（不改业务工程源码）
 *   - Linux/Android/OHOS: `LD_PRELOAD=libtray_memprobe.so` 或
 *     `patchelf --add-needed libtray_memprobe.so <bin>`
 *   - Windows: 注入 `tray_memprobe.dll`（写 PE 导入或运行时 LoadLibrary）；
 *     DllMain 内做 IAT 改写
 *
 * ## 环境变量（详见 hooks/memprobe/README.md）
 *   - TRAY_MEMPROBE_DOMAINS=heap,mmap,gpu,npu,neon,custom|all
 *   - TRAY_MEMPROBE_SAMPLE=N          每 N 次分配采一次栈（1=全采）
 *   - TRAY_MEMPROBE_MIN_SIZE=bytes    小于该尺寸不采栈
 *   - TRAY_MEMPROBE_LOG=path          dump 默认路径
 *   - TRAY_MEMPROBE_DISABLE=1         只装库不记账
 *   - TRAY_MEMPROBE_GPU_SYMS / NPU_SYMS / NEON_SYMS  逗号分隔符号名
 *   - TRAY_MEMPROBE_DUMP_ATEXIT=0     退出时不自动 dump
 *   - TRAY_MEMPROBE_COLLECT=1         分配时写入 collector（供 APM）
 *   - TRAY_HOOKS_APM_FILE / URL       持续观测（见 apm.h）
 *   - TRAY_HOOKS_FILTER_*            降噪过滤（见 collector.h）
 *
 * ## 域说明
 * GPU 默认精确 hook：cuMemAlloc(_v2)/cudaMalloc/clCreateBuffer/vkAllocateMemory
 *（及对应 free），按 ABI 记真实 size。TRAY_MEMPROBE_GPU_SYMS 额外符号为占位 ABI。
 * NPU/NEON 仍为占位或 domain_alloc/free。
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(TRAY_MEMPROBE_EXPORTS)
#    define TRAY_MEMPROBE_API __declspec(dllexport)
#  else
#    define TRAY_MEMPROBE_API __declspec(dllimport)
#  endif
#else
#  define TRAY_MEMPROBE_API __attribute__((visibility("default")))
#endif

/** 内存域（可用 TRAY_MEMPROBE_DOMAINS 过滤） */
typedef enum tray_mem_domain {
  TRAY_MEM_HEAP = 0,   /**< malloc/calloc/realloc/new */
  TRAY_MEM_MMAP = 1,   /**< mmap / VirtualAlloc */
  TRAY_MEM_GPU = 2,    /**< CUDA/OpenCL/Vulkan 等（符号 hook 或主动上报） */
  TRAY_MEM_NPU = 3,    /**< 厂商 NPU runtime */
  TRAY_MEM_NEON = 4,   /**< 自定义加速缓冲（兼容旧名 neno） */
  TRAY_MEM_CUSTOM = 5, /**< 业务自定义域 */
  TRAY_MEM_DOMAIN_COUNT = 6
} tray_mem_domain_t;

/** 单域累计统计 */
typedef struct tray_memprobe_domain_stats {
  uint64_t alloc_calls;    /**< 分配次数（含 size=0 的 hook 命中） */
  uint64_t free_calls;
  uint64_t live_blocks;    /**< 当前未释放块数 */
  uint64_t live_bytes;     /**< 当前未释放字节（扩展域可能估不准） */
  uint64_t peak_bytes;     /**< live_bytes 历史峰值 */
  uint64_t sampled_allocs; /**< 实际采过栈的分配次数 */
} tray_memprobe_domain_stats_t;

/** 进程 CPU（相对上次 sample 的粗略占用率） */
typedef struct tray_memprobe_cpu_stats {
  uint64_t process_user_us;   /**< 进程用户态累计微秒 */
  uint64_t process_sys_us;    /**< 进程内核态累计微秒 */
  double process_cpu_percent; /**< 相对上次采样的估算占用 %，首次为 0 */
  uint32_t num_threads;       /**< Linux: /proc/self/stat；Win 可能为 0 */
} tray_memprobe_cpu_stats_t;

typedef struct tray_memprobe_stats {
  tray_memprobe_domain_stats_t domains[TRAY_MEM_DOMAIN_COUNT];
  uint64_t rss_bytes; /**< 工作集 / RSS */
  uint64_t vsz_bytes; /**< 提交大小 / VSZ */
  tray_memprobe_cpu_stats_t cpu;
  /** 兼容旧字段：等于 domains[HEAP] 同名字段 */
  uint64_t alloc_calls;
  uint64_t free_calls;
  uint64_t live_blocks;
  uint64_t live_bytes;
  uint64_t peak_bytes;
  uint64_t sampled_allocs;
} tray_memprobe_stats_t;

/** 读环境变量并开始记账（DllMain / constructor 已自动调用） */
TRAY_MEMPROBE_API void tray_memprobe_start(void);
TRAY_MEMPROBE_API void tray_memprobe_stop(void);

/** 刷新 CPU/RSS 后拷贝统计快照到 out */
TRAY_MEMPROBE_API void tray_memprobe_get_stats(tray_memprobe_stats_t* out);

/**
 * @brief 写出 Top live 块 + 各域汇总
 * @param path NULL 时用 TRAY_MEMPROBE_LOG，仍空则 stderr
 */
TRAY_MEMPROBE_API void tray_memprobe_dump(const char* path);

/** 业务/扩展域主动上报（GPU/NPU SDK 包装层推荐） */
TRAY_MEMPROBE_API void tray_memprobe_domain_alloc(tray_mem_domain_t dom,
                                                  void* ptr,
                                                  size_t size);
TRAY_MEMPROBE_API void tray_memprobe_domain_free(tray_mem_domain_t dom,
                                                 void* ptr);

/** 刷新 CPU 采样（get_stats / dump 内部也会调用） */
TRAY_MEMPROBE_API void tray_memprobe_sample_cpu(void);

/**
 * @brief PE add-needed 用的占位导出（DllMain 已足以启动探针）
 * @return 恒为 1
 */
TRAY_MEMPROBE_API int tray_memprobe_ping(void);

/**
 * @brief 在 DllMain 外启动 APM（TRAY_MEMPROBE_APM_DEFER=1 时请调）
 * @return 0 成功或未配置；非 0 失败
 */
TRAY_MEMPROBE_API int tray_memprobe_start_apm(void);

#ifdef __cplusplus
}
#endif
