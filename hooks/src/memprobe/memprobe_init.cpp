/**
 * @file memprobe_init.cpp
 * @brief so/dll 加载后自动 start + 安装真 hook（无需改业务工程编译）。
 *
 * 入口时机：
 *   - ELF: constructor(101) — 尽量早于一般业务 .init，但仍晚于部分 CRT
 *   - Win: DllMain(PROCESS_ATTACH) — 禁止复杂操作；此处仅 start+hook
 *
 * 持续观测：若设置 TRAY_HOOKS_APM_FILE / URL，则 apply filter + 启动 APM；
 * 另起轻量线程周期 emit memprobe 统计 JSON。
 *
 * 注意：DllMain 内拿 loader lock；勿再 LoadLibrary 非依赖 DLL。
 * IAT 改写只碰已映射模块，符合此约束。APM 工作线程在 start 后创建。
 */

#include "tray_hooks/apm.h"
#include "tray_hooks/memprobe.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" void tray_memprobe_install_hooks(void);

namespace {

std::atomic<bool> g_stats_stop{true};
std::thread g_stats_thread;

void EmitStatsLine() {
  tray_memprobe_stats_t st;
  std::memset(&st, 0, sizeof(st));
  tray_memprobe_get_stats(&st);
  char line[512];
  std::snprintf(
      line, sizeof(line),
      "{\"type\":\"memprobe.stats\",\"heap_live\":%llu,\"heap_peak\":%llu,"
      "\"rss\":%llu,\"cpu_pct\":%.2f,\"dropped_hint\":0}",
      static_cast<unsigned long long>(st.live_bytes),
      static_cast<unsigned long long>(st.peak_bytes),
      static_cast<unsigned long long>(st.rss_bytes),
      st.cpu.process_cpu_percent);
  tray_hooks_apm_emit_raw(line);
}

void StatsWorker() {
  int interval = 5000;
  const char* iv = std::getenv("TRAY_HOOKS_APM_INTERVAL_MS");
  if (iv && *iv) {
    const int v = std::atoi(iv);
    if (v > 0) {
      interval = v;
    }
  }
  while (!g_stats_stop.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    if (g_stats_stop.load()) {
      break;
    }
    EmitStatsLine();
  }
}

void StartObservability() {
  const char* f = std::getenv("TRAY_HOOKS_APM_FILE");
  const char* u = std::getenv("TRAY_HOOKS_APM_URL");
  if ((!f || !*f) && (!u || !*u)) {
    // 仅过滤、不上报时也允许
    tray_hooks_collector_apply_env_filter();
    return;
  }
  if (tray_hooks_apm_start_from_env() != 0) {
    return;
  }
  if (!g_stats_stop.exchange(false)) {
    return;  // already running
  }
  g_stats_thread = std::thread(StatsWorker);
}

void StopObservability() {
  g_stats_stop = true;
  if (g_stats_thread.joinable()) {
    g_stats_thread.join();
  }
  tray_hooks_apm_flush();
  tray_hooks_apm_stop();
}

}  // namespace

extern "C" int tray_memprobe_start_apm(void) {
  StartObservability();
  return 0;
}

#if !defined(_WIN32)

namespace {

void OnExitDump() {
  const char* auto_dump = std::getenv("TRAY_MEMPROBE_DUMP_ATEXIT");
  if (auto_dump && (*auto_dump == '0' || *auto_dump == 'n' || *auto_dump == 'N')) {
    return;
  }
  tray_memprobe_dump(0);
}

}  // namespace

__attribute__((constructor(101))) static void MemprobeCtor() {
  tray_memprobe_start();
  tray_memprobe_install_hooks();
  StartObservability();
  std::atexit(OnExitDump);
}

__attribute__((destructor(101))) static void MemprobeDtor() {
  StopObservability();
  tray_memprobe_stop();
}

#else

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE self, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(self);
    tray_memprobe_start();
    tray_memprobe_install_hooks();
    // 避免在 loader lock 下起线程：排队到下一时刻
    // 使用简单的 QueueUserAPC 不合适；直接起线程在多数场景可用，
    // 若遇死锁可改设 TRAY_MEMPROBE_APM_DEFER=1 由业务首条 API 触发。
    const char* defer = std::getenv("TRAY_MEMPROBE_APM_DEFER");
    if (!(defer && (*defer == '1' || *defer == 'y'))) {
      StartObservability();
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    const char* auto_dump = std::getenv("TRAY_MEMPROBE_DUMP_ATEXIT");
    if (!(auto_dump && (*auto_dump == '0' || *auto_dump == 'n'))) {
      tray_memprobe_dump(0);
    }
    StopObservability();
    tray_memprobe_stop();
  }
  return TRUE;
}

#endif
