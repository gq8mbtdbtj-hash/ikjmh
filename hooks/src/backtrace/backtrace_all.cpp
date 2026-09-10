/**
 * @file backtrace_all.cpp
 * @brief 跨平台调用栈采集与文本格式化。
 *
 * | 平台 | 抓栈 | 符号化 |
 * |------|------|--------|
 * | Windows | CaptureStackBackTrace | dbghelp SymFromAddr |
 * | Android/OHOS | _Unwind_Backtrace | dladdr |
 * | 其它 POSIX | backtrace(3) | dladdr |
 *
 * skip：调用方应跳过「本 API + proxy 入口」等无关帧（memprobe 用 skip=2）。
 * 符号可能为空（无调试信息 / 剥离 so），仍保留 module+offset 便于离线 addr2line。
 */

#include "tray_hooks/backtrace.h"

#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#elif defined(__ANDROID__) || defined(__OHOS__)
#include <dlfcn.h>
#include <unwind.h>
#include <stdint.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#endif

#if defined(__ANDROID__) || defined(__OHOS__)
namespace {

struct UnwindState {
  void** frames;
  int max_frames;
  int skip;
  int count;
};

_Unwind_Reason_Code UnwindCb(struct _Unwind_Context* ctx, void* arg) {
  UnwindState* st = static_cast<UnwindState*>(arg);
  if (st->skip > 0) {
    --st->skip;
    return _URC_NO_REASON;
  }
  if (st->count >= st->max_frames) {
    return _URC_END_OF_STACK;
  }
  uintptr_t pc = _Unwind_GetIP(ctx);
  if (pc) {
    st->frames[st->count++] = reinterpret_cast<void*>(pc);
  }
  return _URC_NO_REASON;
}

}  // namespace
#endif

extern "C" int tray_hooks_backtrace(tray_hooks_frame_t* out,
                                    int max_frames,
                                    int skip) {
  if (!out || max_frames <= 0) {
    return 0;
  }
  if (skip < 0) {
    skip = 0;
  }

#ifdef _WIN32
  void* stack[64];
  if (max_frames > 64) {
    max_frames = 64;
  }
  // +1：再跳过本函数自身
  const USHORT n =
      CaptureStackBackTrace(static_cast<DWORD>(skip + 1),
                            static_cast<DWORD>(max_frames), stack, NULL);
  HANDLE proc = GetCurrentProcess();
  static bool sym_ok = false;
  if (!sym_ok) {
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(proc, NULL, TRUE);
    sym_ok = true;
  }
  char sym_buf[sizeof(SYMBOL_INFO) + 256];
  for (USHORT i = 0; i < n; ++i) {
    tray_hooks_frame_t& f = out[i];
    std::memset(&f, 0, sizeof(f));
    f.pc = stack[i];

    HMODULE mod = NULL;
    if (GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(stack[i]), &mod) &&
        mod) {
      GetModuleFileNameA(mod, f.module, sizeof(f.module) - 1);
      f.offset = reinterpret_cast<uintptr_t>(stack[i]) -
                 reinterpret_cast<uintptr_t>(mod);
    }

    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    std::memset(sym_buf, 0, sizeof(sym_buf));
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    DWORD64 disp = 0;
    if (SymFromAddr(proc, reinterpret_cast<DWORD64>(stack[i]), &disp, sym)) {
      std::strncpy(f.symbol, sym->Name, sizeof(f.symbol) - 1);
    }
  }
  return static_cast<int>(n);

#elif defined(__ANDROID__) || defined(__OHOS__)
  void* stack[64];
  if (max_frames > 64) {
    max_frames = 64;
  }
  UnwindState st;
  st.frames = stack;
  st.max_frames = max_frames;
  st.skip = skip + 1;
  st.count = 0;
  _Unwind_Backtrace(UnwindCb, &st);
  for (int i = 0; i < st.count; ++i) {
    tray_hooks_frame_t& f = out[i];
    std::memset(&f, 0, sizeof(f));
    f.pc = stack[i];
    Dl_info info;
    if (dladdr(f.pc, &info) && info.dli_fname) {
      std::strncpy(f.module, info.dli_fname, sizeof(f.module) - 1);
      if (info.dli_sname) {
        std::strncpy(f.symbol, info.dli_sname, sizeof(f.symbol) - 1);
      }
      if (info.dli_fbase) {
        f.offset = reinterpret_cast<uintptr_t>(f.pc) -
                   reinterpret_cast<uintptr_t>(info.dli_fbase);
      }
    }
  }
  return st.count;

#else
  void* stack[64];
  if (max_frames > 64) {
    max_frames = 64;
  }
  const int captured = ::backtrace(stack, max_frames + skip + 1);
  int start = skip + 1;
  if (start > captured) {
    return 0;
  }
  int count = captured - start;
  if (count > max_frames) {
    count = max_frames;
  }
  for (int i = 0; i < count; ++i) {
    tray_hooks_frame_t& f = out[i];
    std::memset(&f, 0, sizeof(f));
    f.pc = stack[start + i];
    Dl_info info;
    if (dladdr(f.pc, &info) && info.dli_fname) {
      std::strncpy(f.module, info.dli_fname, sizeof(f.module) - 1);
      if (info.dli_sname) {
        std::strncpy(f.symbol, info.dli_sname, sizeof(f.symbol) - 1);
      }
      if (info.dli_fbase) {
        f.offset = reinterpret_cast<uintptr_t>(f.pc) -
                   reinterpret_cast<uintptr_t>(info.dli_fbase);
      }
    }
  }
  return count;
#endif
}

extern "C" int tray_hooks_backtrace_format(const tray_hooks_frame_t* frames,
                                           int nframes,
                                           char* buf,
                                           size_t buf_size) {
  if (!buf || buf_size == 0) {
    return 0;
  }
  buf[0] = '\0';
  if (!frames || nframes <= 0) {
    return 0;
  }
  size_t used = 0;
  for (int i = 0; i < nframes; ++i) {
    char line[512];
    const tray_hooks_frame_t& f = frames[i];
    if (f.symbol[0]) {
      std::snprintf(line, sizeof(line), "#%02d %s (%s+0x%lx)\n", i, f.symbol,
                    f.module[0] ? f.module : "?",
                    static_cast<unsigned long>(f.offset));
    } else {
      std::snprintf(line, sizeof(line), "#%02d %p (%s+0x%lx)\n", i, f.pc,
                    f.module[0] ? f.module : "?",
                    static_cast<unsigned long>(f.offset));
    }
    const size_t len = std::strlen(line);
    if (used + len + 1 > buf_size) {
      break;
    }
    std::memcpy(buf + used, line, len + 1);
    used += len;
  }
  return static_cast<int>(used);
}
