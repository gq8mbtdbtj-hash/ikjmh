/**
 * @file collector.cpp
 * @brief 事件汇聚 + 进程/线程/tag/模块过滤降噪。
 */

#include "tray_hooks/collector.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace {

tray_hooks_sink_fn g_sink = 0;
void* g_sink_user = 0;
std::mutex g_mu;
uint64_t g_total = 0;
uint64_t g_dropped = 0;
std::atomic<uint64_t> g_seq{0};

struct FilterState {
  bool enabled = false;
  std::vector<std::string> allow_tags;
  std::vector<std::string> deny_tags;
  std::vector<uint32_t> allow_tids;
  std::string allow_process;
  std::vector<std::string> allow_modules;
  uint64_t min_arg0 = 0;
  unsigned sample_n = 1;
};

FilterState g_filter;

int64_t NowMs() {
  using clock = std::chrono::system_clock;
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             clock::now().time_since_epoch())
      .count();
}

uint32_t CurrentTid() {
#if defined(_WIN32)
  return static_cast<uint32_t>(GetCurrentThreadId());
#else
  return static_cast<uint32_t>(pthread_self() & 0xffffffffu);
#endif
}

uint32_t CurrentPid() {
#if defined(_WIN32)
  return static_cast<uint32_t>(GetCurrentProcessId());
#else
  return static_cast<uint32_t>(getpid());
#endif
}

void FillProcessName(char* out, size_t n) {
  if (!out || n == 0) {
    return;
  }
  out[0] = '\0';
#if defined(_WIN32)
  char path[MAX_PATH];
  DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
  if (!len) {
    return;
  }
  const char* base = path;
  for (char* p = path; *p; ++p) {
    if (*p == '\\' || *p == '/') {
      base = p + 1;
    }
  }
  std::strncpy(out, base, n - 1);
#else
  char path[512];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len <= 0) {
    return;
  }
  path[len] = '\0';
  const char* base = path;
  for (char* p = path; *p; ++p) {
    if (*p == '/') {
      base = p + 1;
    }
  }
  std::strncpy(out, base, n - 1);
#endif
  out[n - 1] = '\0';
}

std::vector<std::string> SplitCsv(const char* s) {
  std::vector<std::string> out;
  if (!s || !*s) {
    return out;
  }
  std::string cur;
  for (const char* p = s; ; ++p) {
    if (*p == ',' || *p == '\0') {
      // trim spaces
      size_t b = 0, e = cur.size();
      while (b < e && (cur[b] == ' ' || cur[b] == '\t')) {
        ++b;
      }
      while (e > b && (cur[e - 1] == ' ' || cur[e - 1] == '\t')) {
        --e;
      }
      if (e > b) {
        out.push_back(cur.substr(b, e - b));
      }
      cur.clear();
      if (*p == '\0') {
        break;
      }
    } else {
      cur.push_back(*p);
    }
  }
  return out;
}

bool ListContains(const std::vector<std::string>& list, const char* item) {
  if (!item) {
    return false;
  }
  for (size_t i = 0; i < list.size(); ++i) {
    if (list[i] == item) {
      return true;
    }
  }
  return false;
}

bool PassFilter(const tray_hooks_event_t& ev) {
  FilterState f;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_filter.enabled) {
      return true;
    }
    f = g_filter;
  }

  if (!f.deny_tags.empty() && ListContains(f.deny_tags, ev.tag)) {
    return false;
  }
  if (!f.allow_tags.empty() && !ListContains(f.allow_tags, ev.tag)) {
    return false;
  }
  if (ev.arg0 < f.min_arg0) {
    return false;
  }
  if (!f.allow_tids.empty()) {
    bool ok = false;
    for (size_t i = 0; i < f.allow_tids.size(); ++i) {
      if (f.allow_tids[i] == ev.tid) {
        ok = true;
        break;
      }
    }
    if (!ok) {
      return false;
    }
  }
  if (!f.allow_process.empty()) {
    if (!std::strstr(ev.process, f.allow_process.c_str())) {
      return false;
    }
  }
  if (!f.allow_modules.empty()) {
    bool hit = false;
    for (int i = 0; i < ev.nframes && !hit; ++i) {
      for (size_t m = 0; m < f.allow_modules.size(); ++m) {
        if (ev.frames[i].module[0] &&
            std::strstr(ev.frames[i].module, f.allow_modules[m].c_str())) {
          hit = true;
          break;
        }
      }
    }
    if (!hit) {
      return false;
    }
  }
  if (f.sample_n > 1) {
    const uint64_t seq = ++g_seq;
    if ((seq % f.sample_n) != 0) {
      return false;
    }
  }
  return true;
}

void ApplyFilterLocked(const tray_hooks_filter_t* src) {
  g_filter = FilterState();
  if (!src) {
    g_filter.enabled = false;
    return;
  }
  g_filter.allow_tags = SplitCsv(src->allow_tags);
  g_filter.deny_tags = SplitCsv(src->deny_tags);
  g_filter.allow_modules = SplitCsv(src->allow_modules);
  g_filter.allow_process = src->allow_process ? src->allow_process : "";
  g_filter.min_arg0 = src->min_arg0;
  g_filter.sample_n = src->sample_n;
  if (g_filter.sample_n == 0) {
    g_filter.sample_n = 1;
  }
  std::vector<std::string> tids = SplitCsv(src->allow_tids);
  for (size_t i = 0; i < tids.size(); ++i) {
    g_filter.allow_tids.push_back(
        static_cast<uint32_t>(std::strtoul(tids[i].c_str(), 0, 10)));
  }
  g_filter.enabled =
      !g_filter.allow_tags.empty() || !g_filter.deny_tags.empty() ||
      !g_filter.allow_tids.empty() || !g_filter.allow_process.empty() ||
      !g_filter.allow_modules.empty() || g_filter.min_arg0 > 0 ||
      g_filter.sample_n > 1;
}

}  // namespace

extern "C" void tray_hooks_collector_set_sink(tray_hooks_sink_fn sink,
                                              void* user) {
  std::lock_guard<std::mutex> lock(g_mu);
  g_sink = sink;
  g_sink_user = user;
}

extern "C" void tray_hooks_collector_set_filter(const tray_hooks_filter_t* f) {
  std::lock_guard<std::mutex> lock(g_mu);
  ApplyFilterLocked(f);
}

extern "C" void tray_hooks_collector_apply_env_filter(void) {
  tray_hooks_filter_t f;
  std::memset(&f, 0, sizeof(f));
  f.allow_tags = std::getenv("TRAY_HOOKS_FILTER_TAGS");
  f.deny_tags = std::getenv("TRAY_HOOKS_FILTER_DENY_TAGS");
  f.allow_tids = std::getenv("TRAY_HOOKS_FILTER_TIDS");
  f.allow_process = std::getenv("TRAY_HOOKS_FILTER_PROCESS");
  f.allow_modules = std::getenv("TRAY_HOOKS_FILTER_MODULES");
  const char* min_a = std::getenv("TRAY_HOOKS_FILTER_MIN_ARG0");
  if (min_a && *min_a) {
    f.min_arg0 = static_cast<uint64_t>(std::strtoull(min_a, 0, 10));
  }
  const char* sample = std::getenv("TRAY_HOOKS_FILTER_SAMPLE");
  if (sample && *sample) {
    f.sample_n = static_cast<unsigned>(std::atoi(sample));
  }
  tray_hooks_collector_set_filter(&f);
}

extern "C" void tray_hooks_collector_record(const char* tag,
                                            const tray_hooks_frame_t* frames,
                                            int nframes,
                                            uint64_t arg0) {
  tray_hooks_event_t ev;
  std::memset(&ev, 0, sizeof(ev));
  ev.timestamp_ms = NowMs();
  if (tag) {
    std::strncpy(ev.tag, tag, sizeof(ev.tag) - 1);
  }
  ev.arg0 = arg0;
  ev.pid = CurrentPid();
  ev.tid = CurrentTid();
  FillProcessName(ev.process, sizeof(ev.process));

  tray_hooks_frame_t local[64];
  const bool need_stack_for_module_filter = [&]() {
    std::lock_guard<std::mutex> lock(g_mu);
    return g_filter.enabled && !g_filter.allow_modules.empty();
  }();

  if (!frames && need_stack_for_module_filter) {
    nframes = tray_hooks_backtrace(local, 64, 1);
    frames = local;
  } else if (!frames) {
    // 延迟：先不过滤模块时仍采栈（sink/APM 需要）；无 sink 且无 module 过滤可跳过？
    // 保持原行为：总是可采
    nframes = tray_hooks_backtrace(local, 64, 1);
    frames = local;
  }
  if (nframes < 0) {
    nframes = 0;
  }
  if (nframes > 64) {
    nframes = 64;
  }
  ev.nframes = nframes;
  if (nframes > 0 && frames) {
    std::memcpy(ev.frames, frames,
                sizeof(tray_hooks_frame_t) * static_cast<size_t>(nframes));
  }

  if (!PassFilter(ev)) {
    std::lock_guard<std::mutex> lock(g_mu);
    ++g_dropped;
    return;
  }

  tray_hooks_sink_fn sink = 0;
  void* user = 0;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    ++g_total;
    sink = g_sink;
    user = g_sink_user;
  }
  if (sink) {
    sink(&ev, user);
  }
}

extern "C" uint64_t tray_hooks_collector_total(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_total;
}

extern "C" uint64_t tray_hooks_collector_dropped(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  return g_dropped;
}
