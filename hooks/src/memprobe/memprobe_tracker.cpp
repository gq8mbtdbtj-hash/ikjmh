/**
 * @file memprobe_tracker.cpp
 * @brief 内存/CPU 记账核心（与 hook 改写解耦）。
 *
 * 数据流：
 *   ProxyMalloc / LD_PRELOAD malloc / domain_alloc
 *     → OnAlloc：可选 backtrace → g_live[ptr] + domains[] 计数
 *   ProxyFree / free / domain_free
 *     → OnFree：按 ptr 找回域与 size，扣 live_*
 *
 * 重入：OnAlloc 内部可能 malloc（栈符号化、unordered_map），用
 * tls_reentry 丢弃内层记账，避免死锁/爆栈。
 *
 * CPU：Win=GetProcessTimes；POSIX=/proc/self/stat + CLOCK_MONOTONIC 差分。
 */

#include "tray_hooks/memprobe.h"
#include "tray_hooks/backtrace.h"
#include "memprobe_internal.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#else
#include <time.h>
#include <unistd.h>
#endif

namespace {

/** 一块仍存活分配的元数据（dump Top-N 用） */
struct AllocRec {
  tray_mem_domain_t dom;
  size_t size;
  int nframes;                     /**< 0=未采样栈 */
  tray_hooks_frame_t frames[16];   /**< 采样时最多 16 帧 */
};

std::mutex g_mu;
std::unordered_map<void*, AllocRec> g_live;  /**< ptr → 记录；同 ptr 再分配会覆盖 */
tray_memprobe_stats_t g_stats;
bool g_started = false;
bool g_track = true;  /**< TRAY_MEMPROBE_DISABLE 时可关记账仍保留 API */
bool g_domain_on[TRAY_MEM_DOMAIN_COUNT];

unsigned g_sample_n = 1;   /**< TRAY_MEMPROBE_SAMPLE：每 N 次采栈 */
size_t g_min_size = 0;     /**< TRAY_MEMPROBE_MIN_SIZE */
std::string g_log_path;    /**< TRAY_MEMPROBE_LOG */
std::atomic<uint64_t> g_alloc_seq{0};  /**< 全局分配序号，用于采样取模 */

#if defined(_WIN32)
ULARGE_INTEGER g_last_cpu_kernel = {};
ULARGE_INTEGER g_last_cpu_user = {};
ULARGE_INTEGER g_last_wall = {};
#else
uint64_t g_last_user_j = 0;
uint64_t g_last_sys_j = 0;
uint64_t g_last_wall_us = 0;
#endif

const char* DomainName(tray_mem_domain_t d) {
  static const char* k[] = {"heap", "mmap", "gpu", "npu", "neon", "custom"};
  if (d < 0 || d >= TRAY_MEM_DOMAIN_COUNT) {
    return "?";
  }
  return k[d];
}

void EnableAllDomains() {
  for (int i = 0; i < TRAY_MEM_DOMAIN_COUNT; ++i) {
    g_domain_on[i] = true;
  }
}

/** 解析 TRAY_MEMPROBE_DOMAINS；空/缺省=全开；支持 all / neon|neno */
void ParseDomainsEnv(const char* s) {
  if (!s || !*s) {
    EnableAllDomains();
    return;
  }
  for (int i = 0; i < TRAY_MEM_DOMAIN_COUNT; ++i) {
    g_domain_on[i] = false;
  }
  std::string tmp(s);
  for (size_t i = 0; i < tmp.size(); ++i) {
    if (tmp[i] == ',') {
      tmp[i] = ' ';
    }
  }
  // 子串匹配即可（"heap,mmap" / "all"）
  if (std::strstr(s, "heap")) g_domain_on[TRAY_MEM_HEAP] = true;
  if (std::strstr(s, "mmap")) g_domain_on[TRAY_MEM_MMAP] = true;
  if (std::strstr(s, "gpu")) g_domain_on[TRAY_MEM_GPU] = true;
  if (std::strstr(s, "npu")) g_domain_on[TRAY_MEM_NPU] = true;
  if (std::strstr(s, "neon") || std::strstr(s, "neno")) {
    g_domain_on[TRAY_MEM_NEON] = true;
  }
  if (std::strstr(s, "custom")) g_domain_on[TRAY_MEM_CUSTOM] = true;
  if (std::strstr(s, "all")) EnableAllDomains();
}

/** Win: WorkingSet / PagefileUsage；Linux: /proc/self/statm */
void ReadProcMem(uint64_t* rss, uint64_t* vsz) {
  *rss = 0;
  *vsz = 0;
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc;
  std::memset(&pmc, 0, sizeof(pmc));
  pmc.cb = sizeof(pmc);
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    *rss = static_cast<uint64_t>(pmc.WorkingSetSize);
    *vsz = static_cast<uint64_t>(pmc.PagefileUsage);
  }
#else
  FILE* f = std::fopen("/proc/self/statm", "r");
  if (!f) {
    return;
  }
  unsigned long size_pages = 0, res_pages = 0;
  if (std::fscanf(f, "%lu %lu", &size_pages, &res_pages) == 2) {
    const long page = sysconf(_SC_PAGESIZE);
    if (page > 0) {
      *vsz = static_cast<uint64_t>(size_pages) * static_cast<uint64_t>(page);
      *rss = static_cast<uint64_t>(res_pages) * static_cast<uint64_t>(page);
    }
  }
  std::fclose(f);
#endif
}

/** 顶层兼容字段镜像 heap 域，避免旧调用方改结构体 */
void SyncCompatFields() {
  const tray_memprobe_domain_stats_t& h = g_stats.domains[TRAY_MEM_HEAP];
  g_stats.alloc_calls = h.alloc_calls;
  g_stats.free_calls = h.free_calls;
  g_stats.live_blocks = h.live_blocks;
  g_stats.live_bytes = h.live_bytes;
  g_stats.peak_bytes = h.peak_bytes;
  g_stats.sampled_allocs = h.sampled_allocs;
}

/** start() 时一次性读环境变量（之后改 env 不热更新） */
void LoadEnv() {
  EnableAllDomains();
  const char* s = std::getenv("TRAY_MEMPROBE_SAMPLE");
  if (s && *s) {
    const int v = std::atoi(s);
    if (v > 0) {
      g_sample_n = static_cast<unsigned>(v);
    }
  }
  s = std::getenv("TRAY_MEMPROBE_MIN_SIZE");
  if (s && *s) {
    g_min_size = static_cast<size_t>(std::strtoul(s, 0, 10));
  }
  s = std::getenv("TRAY_MEMPROBE_LOG");
  if (s && *s) {
    g_log_path = s;
  }
  s = std::getenv("TRAY_MEMPROBE_DISABLE");
  if (s && (*s == '1' || *s == 'y' || *s == 'Y')) {
    g_track = false;
  }
  s = std::getenv("TRAY_MEMPROBE_DOMAINS");
  if (s) {
    ParseDomainsEnv(s);
  }
}

}  // namespace

namespace tray_memprobe {
namespace detail {

/** 本线程 OnAlloc/OnFree 嵌套深度；>1 表示采集路径触发了二次分配 */
thread_local int tls_reentry = 0;

struct ReentryGuard {
  ReentryGuard() { ++tls_reentry; }
  ~ReentryGuard() { --tls_reentry; }
  /** true=已在外层采集中，内层应跳过记账 */
  bool active() const { return tls_reentry > 1; }
};

bool DomainEnabled(tray_mem_domain_t dom) {
  if (dom < 0 || dom >= TRAY_MEM_DOMAIN_COUNT) {
    return false;
  }
  return g_domain_on[dom];
}

void OnAlloc(tray_mem_domain_t dom, void* p, size_t size) {
  if (!p || !g_started || !g_track || !DomainEnabled(dom)) {
    return;
  }
  ReentryGuard g;
  if (g.active()) {
    return;
  }

  AllocRec rec;
  std::memset(&rec, 0, sizeof(rec));
  rec.dom = dom;
  rec.size = size;
  rec.nframes = 0;

  // 先采栈（可能分配），再持锁更新；skip=2 去掉 OnAlloc/proxy
  const uint64_t seq = ++g_alloc_seq;
  const bool sample =
      (size >= g_min_size) && (g_sample_n <= 1 || (seq % g_sample_n) == 0);
  if (sample) {
    rec.nframes = tray_hooks_backtrace(rec.frames, 16, 2);
  }

  std::lock_guard<std::mutex> lock(g_mu);
  tray_memprobe_domain_stats_t& ds = g_stats.domains[dom];
  ds.alloc_calls++;
  // 同地址重复登记：先从旧域扣 live（realloc 场景由 OnFree+OnAlloc 组合）
  std::unordered_map<void*, AllocRec>::iterator it = g_live.find(p);
  if (it != g_live.end()) {
    tray_memprobe_domain_stats_t& old =
        g_stats.domains[it->second.dom];
    if (old.live_bytes >= it->second.size) {
      old.live_bytes -= it->second.size;
    }
    if (old.live_blocks > 0) {
      old.live_blocks--;
    }
  }
  g_live[p] = rec;
  ds.live_blocks++;
  ds.live_bytes += size;
  if (ds.live_bytes > ds.peak_bytes) {
    ds.peak_bytes = ds.live_bytes;
  }
  if (sample) {
    ds.sampled_allocs++;
  }
  ReadProcMem(&g_stats.rss_bytes, &g_stats.vsz_bytes);
  SyncCompatFields();
}

void OnFree(tray_mem_domain_t dom, void* p) {
  if (!p || !g_started || !g_track) {
    return;
  }
  ReentryGuard g;
  if (g.active()) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  std::unordered_map<void*, AllocRec>::iterator it = g_live.find(p);
  if (it == g_live.end()) {
    if (DomainEnabled(dom)) {
      g_stats.domains[dom].free_calls++;
    }
    return;
  }
  tray_mem_domain_t d = it->second.dom;
  tray_memprobe_domain_stats_t& ds = g_stats.domains[d];
  ds.free_calls++;
  if (ds.live_bytes >= it->second.size) {
    ds.live_bytes -= it->second.size;
  }
  if (ds.live_blocks > 0) {
    ds.live_blocks--;
  }
  g_live.erase(it);
  SyncCompatFields();
}

}  // namespace detail
}  // namespace tray_memprobe

extern "C" void tray_memprobe_domain_alloc(tray_mem_domain_t dom,
                                           void* ptr,
                                           size_t size) {
  tray_memprobe::detail::OnAlloc(dom, ptr, size);
}

extern "C" void tray_memprobe_domain_free(tray_mem_domain_t dom, void* ptr) {
  tray_memprobe::detail::OnFree(dom, ptr);
}

/**
 * 刷新进程 CPU 累计与「相对上次」占用率。
 * @note 持 g_mu；调用方勿在已持锁路径再调（dump 在锁外调本函数）。
 */
extern "C" void tray_memprobe_sample_cpu(void) {
  std::lock_guard<std::mutex> lock(g_mu);
#if defined(_WIN32)
  FILETIME create, exit_t, kernel, user;
  if (!GetProcessTimes(GetCurrentProcess(), &create, &exit_t, &kernel, &user)) {
    return;
  }
  ULARGE_INTEGER k, u, wall;
  k.LowPart = kernel.dwLowDateTime;
  k.HighPart = kernel.dwHighDateTime;
  u.LowPart = user.dwLowDateTime;
  u.HighPart = user.dwHighDateTime;
  GetSystemTimeAsFileTime(reinterpret_cast<FILETIME*>(&wall));

  g_stats.cpu.process_sys_us = k.QuadPart / 10;  // 100ns -> us
  g_stats.cpu.process_user_us = u.QuadPart / 10;

  if (g_last_wall.QuadPart != 0) {
    const ULONGLONG d_wall = wall.QuadPart - g_last_wall.QuadPart;
    const ULONGLONG d_cpu =
        (k.QuadPart - g_last_cpu_kernel.QuadPart) +
        (u.QuadPart - g_last_cpu_user.QuadPart);
    if (d_wall > 0) {
      g_stats.cpu.process_cpu_percent =
          (100.0 * static_cast<double>(d_cpu)) / static_cast<double>(d_wall);
    }
  }
  g_last_cpu_kernel = k;
  g_last_cpu_user = u;
  g_last_wall = wall;
#else
  FILE* f = std::fopen("/proc/self/stat", "r");
  if (!f) {
    return;
  }
  // fields: pid ... utime(14) stime(15) ... num_threads(20)
  unsigned long utime = 0, stime = 0;
  long num_threads = 0;
  int pid = 0;
  char comm[256];
  char state = 0;
  // skip to utime: use fscanf carefully
  if (std::fscanf(f,
                  "%d %255s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu "
                  "%lu %*d %*d %*d %*d %ld",
                  &pid, comm, &state, &utime, &stime, &num_threads) >= 5) {
    const long hz = sysconf(_SC_CLK_TCK);
    if (hz > 0) {
      g_stats.cpu.process_user_us =
          static_cast<uint64_t>(utime) * 1000000ull / static_cast<uint64_t>(hz);
      g_stats.cpu.process_sys_us =
          static_cast<uint64_t>(stime) * 1000000ull / static_cast<uint64_t>(hz);
    }
    g_stats.cpu.num_threads = num_threads > 0 ? static_cast<uint32_t>(num_threads) : 0;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const uint64_t wall_us =
        static_cast<uint64_t>(ts.tv_sec) * 1000000ull +
        static_cast<uint64_t>(ts.tv_nsec) / 1000ull;
    if (g_last_wall_us != 0) {
      const uint64_t d_wall = wall_us - g_last_wall_us;
      const uint64_t d_cpu = (g_stats.cpu.process_user_us + g_stats.cpu.process_sys_us) -
                             (g_last_user_j + g_last_sys_j);
      // g_last_* stored in us already below
      if (d_wall > 0) {
        g_stats.cpu.process_cpu_percent =
            (100.0 * static_cast<double>(d_cpu)) / static_cast<double>(d_wall);
      }
    }
    g_last_user_j = g_stats.cpu.process_user_us;
    g_last_sys_j = g_stats.cpu.process_sys_us;
    g_last_wall_us = wall_us;
  }
  std::fclose(f);
#endif
}

extern "C" void tray_memprobe_start(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_started) {
    return;
  }
  LoadEnv();
  std::memset(&g_stats, 0, sizeof(g_stats));
  g_started = true;
  std::fprintf(stderr, "[tray_memprobe] started domains=");
  for (int i = 0; i < TRAY_MEM_DOMAIN_COUNT; ++i) {
    if (g_domain_on[i]) {
      std::fprintf(stderr, "%s,", DomainName(static_cast<tray_mem_domain_t>(i)));
    }
  }
  std::fprintf(stderr, " sample=%u\n", g_sample_n);
}

extern "C" void tray_memprobe_stop(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_started = false;
}

extern "C" void tray_memprobe_get_stats(tray_memprobe_stats_t* out) {
  if (!out) {
    return;
  }
  tray_memprobe_sample_cpu();
  std::lock_guard<std::mutex> lock(g_mu);
  ReadProcMem(&g_stats.rss_bytes, &g_stats.vsz_bytes);
  SyncCompatFields();
  *out = g_stats;
}

/**
 * 快照 live 表后按 size 降序打印 Top20 + 各域汇总。
 * 写文件时用 ReentryGuard，避免 fprintf 内部路径再记账。
 */
extern "C" void tray_memprobe_dump(const char* path) {
  tray_memprobe::detail::ReentryGuard guard;
  tray_memprobe_sample_cpu();
  std::vector<std::pair<void*, AllocRec> > snap;
  tray_memprobe_stats_t st;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    ReadProcMem(&g_stats.rss_bytes, &g_stats.vsz_bytes);
    SyncCompatFields();
    st = g_stats;
    snap.reserve(g_live.size());
    for (std::unordered_map<void*, AllocRec>::iterator it = g_live.begin();
         it != g_live.end(); ++it) {
      snap.push_back(*it);
    }
  }
  std::sort(snap.begin(), snap.end(),
            [](const std::pair<void*, AllocRec>& a,
               const std::pair<void*, AllocRec>& b) {
              return a.second.size > b.second.size;
            });

  const char* out_path = path;
  if (!out_path || !*out_path) {
    out_path = g_log_path.empty() ? 0 : g_log_path.c_str();
  }
  FILE* fp = out_path ? std::fopen(out_path, "w") : stderr;
  if (!fp) {
    fp = stderr;
  }

  std::fprintf(fp, "=== tray_memprobe dump ===\n");
  std::fprintf(fp, "rss=%llu vsz=%llu cpu_user_us=%llu cpu_sys_us=%llu cpu%%=%.2f threads=%u\n",
               static_cast<unsigned long long>(st.rss_bytes),
               static_cast<unsigned long long>(st.vsz_bytes),
               static_cast<unsigned long long>(st.cpu.process_user_us),
               static_cast<unsigned long long>(st.cpu.process_sys_us),
               st.cpu.process_cpu_percent, st.cpu.num_threads);
  for (int i = 0; i < TRAY_MEM_DOMAIN_COUNT; ++i) {
    const tray_memprobe_domain_stats_t& d = st.domains[i];
    if (d.alloc_calls == 0 && d.live_bytes == 0) {
      continue;
    }
    std::fprintf(fp,
                 "[%s] alloc=%llu free=%llu live_blocks=%llu live_bytes=%llu "
                 "peak=%llu sampled=%llu\n",
                 DomainName(static_cast<tray_mem_domain_t>(i)),
                 static_cast<unsigned long long>(d.alloc_calls),
                 static_cast<unsigned long long>(d.free_calls),
                 static_cast<unsigned long long>(d.live_blocks),
                 static_cast<unsigned long long>(d.live_bytes),
                 static_cast<unsigned long long>(d.peak_bytes),
                 static_cast<unsigned long long>(d.sampled_allocs));
  }

  const size_t top_n = snap.size() < 20 ? snap.size() : 20;
  for (size_t i = 0; i < top_n; ++i) {
    const AllocRec& r = snap[i].second;
    std::fprintf(fp, "\n-- live %p size=%zu domain=%s --\n", snap[i].first,
                 r.size, DomainName(r.dom));
    if (r.nframes > 0) {
      char buf[2048];
      tray_hooks_backtrace_format(r.frames, r.nframes, buf, sizeof(buf));
      std::fputs(buf, fp);
    }
  }
  if (fp != stderr) {
    std::fclose(fp);
    std::fprintf(stderr, "[tray_memprobe] dump -> %s\n", out_path);
  }
}

extern "C" int tray_memprobe_ping(void) { return 1; }
