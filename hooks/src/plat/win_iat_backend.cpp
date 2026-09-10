/**
 * @file win_iat_backend.cpp
 * @brief tray_hooks 的 Windows 后端：真 IAT 改写 + AUTOMATIC 晚加载补 hook。
 *
 * AUTOMATIC（首选）：LdrRegisterDllNotification（ntdll），新 DLL 加载后对其 IAT
 * 重放活跃 stub。回退：IAT hook LoadLibrary*（易与 CRT 重入，可用环境变量关闭）。
 */

#include "internal/backend.hpp"
#include "plat/win_iat_patch.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#endif

namespace tray_hooks {
namespace detail {

#ifdef _WIN32

namespace {

/** 通过本编译单元内地址反查所在模块；静态链进 EXE 时等于主模块 */
HMODULE SelfModule() {
  HMODULE m = 0;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&SelfModule), &m);
  return m;
}

/**
 * 仅当 hook 引擎位于独立 DLL（如 tray_memprobe.dll）时才跳过自身，
 * 避免改到探针自己的导入。静态链进 EXE 时不得跳过主模块，否则白盒/进程内
 * IAT（如对本 EXE 导入的 wb_target）永远 0 命中。
 */
HMODULE SkipModuleForPatch() {
  HMODULE self = SelfModule();
  HMODULE main_mod = GetModuleHandleW(NULL);
  if (self && main_mod && self == main_mod) {
    return 0;
  }
  return self;
}

typedef HMODULE(WINAPI* LoadLibraryA_fn)(LPCSTR);
typedef HMODULE(WINAPI* LoadLibraryW_fn)(LPCWSTR);
typedef HMODULE(WINAPI* LoadLibraryExA_fn)(LPCSTR, HANDLE, DWORD);
typedef HMODULE(WINAPI* LoadLibraryExW_fn)(LPCWSTR, HANDLE, DWORD);

LoadLibraryA_fn g_real_LLA = 0;
LoadLibraryW_fn g_real_LLW = 0;
LoadLibraryExA_fn g_real_LLEA = 0;
LoadLibraryExW_fn g_real_LLEW = 0;

// ---- ntdll LdrRegisterDllNotification（AUTOMATIC 首选，避免 IAT hook LoadLibrary）----
#ifndef LDR_DLL_NOTIFICATION_REASON_LOADED
#define LDR_DLL_NOTIFICATION_REASON_LOADED 1
#endif

typedef struct _TRAY_UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
} TRAY_UNICODE_STRING;

typedef struct _TRAY_LDR_DLL_LOADED_NOTIFICATION_DATA {
  ULONG Flags;
  const TRAY_UNICODE_STRING* FullDllName;
  const TRAY_UNICODE_STRING* BaseDllName;
  PVOID DllBase;
  ULONG SizeOfImage;
} TRAY_LDR_DLL_LOADED_NOTIFICATION_DATA;

typedef union _TRAY_LDR_DLL_NOTIFICATION_DATA {
  TRAY_LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
  TRAY_LDR_DLL_LOADED_NOTIFICATION_DATA Unloaded;
} TRAY_LDR_DLL_NOTIFICATION_DATA;

typedef VOID(NTAPI* PFN_LdrDllNotification)(
    ULONG NotificationReason,
    const TRAY_LDR_DLL_NOTIFICATION_DATA* NotificationData,
    PVOID Context);

typedef LONG(NTAPI* PFN_LdrRegisterDllNotification)(
    ULONG Flags,
    PFN_LdrDllNotification NotificationFunction,
    PVOID Context,
    PVOID* Cookie);

typedef LONG(NTAPI* PFN_LdrUnregisterDllNotification)(PVOID Cookie);

PFN_LdrRegisterDllNotification g_LdrRegister = 0;
PFN_LdrUnregisterDllNotification g_LdrUnregister = 0;
PVOID g_ldr_cookie = 0;

}  // namespace

/** single 模式：caller_path 子串匹配（大小写不敏感） */
static int AllowBySubstr(const char* caller_path, void* arg) {
  Stub* s = static_cast<Stub*>(arg);
  if (!s || s->caller_path.empty()) {
    return 1;
  }
  if (!caller_path) {
    return 0;
  }
  std::string a(caller_path), b(s->caller_path);
  for (size_t i = 0; i < a.size(); ++i) {
    a[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
  }
  for (size_t i = 0; i < b.size(); ++i) {
    b[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
  }
  return a.find(b) != std::string::npos ? 1 : 0;
}

class WinIatBackend;
static WinIatBackend* g_win = 0;

class WinIatBackend : public Backend {
public:
  const char* name() const {
    if (!automatic_) {
      return "win_iat";
    }
    return use_ldr_notify_ ? "win_iat(ldr)" : "win_iat(auto)";
  }

  int init(tray_hooks_mode_t mode) {
    mode_ = mode;
    automatic_ = (mode == TRAY_HOOKS_MODE_AUTOMATIC);
    use_ldr_notify_ = false;
    g_win = this;

    if (automatic_) {
      HMODULE k32 = GetModuleHandleA("kernel32.dll");
      if (k32) {
        g_real_LLA = reinterpret_cast<LoadLibraryA_fn>(
            GetProcAddress(k32, "LoadLibraryA"));
        g_real_LLW = reinterpret_cast<LoadLibraryW_fn>(
            GetProcAddress(k32, "LoadLibraryW"));
        g_real_LLEA = reinterpret_cast<LoadLibraryExA_fn>(
            GetProcAddress(k32, "LoadLibraryExA"));
        g_real_LLEW = reinterpret_cast<LoadLibraryExW_fn>(
            GetProcAddress(k32, "LoadLibraryExW"));
      }
      InstallLoaderHooks();
    }
    return TRAY_HOOKS_OK;
  }

  void uninit() {
    if (g_ldr_cookie && g_LdrUnregister) {
      g_LdrUnregister(g_ldr_cookie);
      g_ldr_cookie = 0;
    }
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < loader_patches_.size(); ++i) {
      iat::Restore(loader_patches_[i]);
    }
    loader_patches_.clear();
    for (std::size_t i = 0; i < patches_.size(); ++i) {
      iat::Restore(patches_[i]);
    }
    patches_.clear();
    stub_patches_.clear();
    active_.clear();
    if (g_win == this) {
      g_win = 0;
    }
  }

  Stub* hook(Stub* stub) {
    if (!stub) {
      return 0;
    }
    const char* dll =
        stub->callee_path.empty() ? 0 : stub->callee_path.c_str();

    // 组装 allow：partial 用回调；single 用 caller_path 子串
    struct AllowCtx {
      Stub* s;
    } ctx;
    ctx.s = stub;
    int (*allow_fn)(const char*, void*) = 0;
    void* allow_arg = 0;
    if (stub->scope == 1 && stub->caller_allow) {
      allow_fn = stub->caller_allow;
      allow_arg = stub->caller_allow_arg;
    } else if (stub->scope == 0 && !stub->caller_path.empty()) {
      allow_fn = &AllowBySubstr;
      allow_arg = stub;
    }

    std::vector<iat::Patch> local;
    const int n = iat::PatchAllModules(dll, stub->sym_name.c_str(),
                                       stub->new_func, &local,
                                       SkipModuleForPatch(), allow_fn, allow_arg);
    if (!local.empty() && local[0].original) {
      stub->prev_func = local[0].original;
    } else {
      void* prev = resolve_sym(dll, stub->sym_name.c_str());
      stub->prev_func = prev ? prev : stub->new_func;
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (std::size_t i = 0; i < local.size(); ++i) {
        patches_.push_back(local[i]);
      }
      stub_patches_[stub] = local;
      active_.push_back(stub);
    }

    std::fprintf(stderr,
                 "[tray_hooks] win_iat: hooked '%s' in %d IAT slot(s) "
                 "(scope=%d prev=%p proxy=%p)\n",
                 stub->sym_name.c_str(), n, stub->scope, stub->prev_func,
                 stub->new_func);
    return stub;
  }

  int unhook(Stub* stub) {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < active_.size(); ++i) {
      if (active_[i] == stub) {
        active_.erase(active_.begin() + static_cast<std::ptrdiff_t>(i));
        break;
      }
    }
    std::map<Stub*, std::vector<iat::Patch> >::iterator it =
        stub_patches_.find(stub);
    if (it == stub_patches_.end()) {
      return TRAY_HOOKS_OK;
    }
    for (std::size_t i = 0; i < it->second.size(); ++i) {
      iat::Restore(it->second[i]);
    }
    stub_patches_.erase(it);
    return TRAY_HOOKS_OK;
  }

  void* resolve_sym(const char* module, const char* sym) {
    if (!sym || !*sym) {
      return 0;
    }
    HMODULE h = 0;
    if (module && *module) {
      h = GetModuleHandleA(module);
      if (!h) {
        // AUTOMATIC 下勿走被 hook 的 LoadLibrary，直接用已保存的真函数
        if (g_real_LLA) {
          h = g_real_LLA(module);
        } else {
          h = LoadLibraryA(module);
        }
      }
    }
    if (!h) {
      const char* cands[] = {"ucrtbase.dll", "msvcrt.dll", "kernel32.dll",
                             "ntdll.dll", 0};
      for (int i = 0; cands[i]; ++i) {
        h = GetModuleHandleA(cands[i]);
        if (h) {
          void* p = reinterpret_cast<void*>(GetProcAddress(h, sym));
          if (p) {
            return p;
          }
        }
      }
      return 0;
    }
    return reinterpret_cast<void*>(GetProcAddress(h, sym));
  }

  /** 新模块加载后：只补该模块 IAT（幂等） */
  void OnModuleLoaded(HMODULE mod) {
    if (!mod || !automatic_ || refreshing_) {
      return;
    }
    // OnModuleLoaded 只处理「新加载」的 DLL；静态链时 SelfModule==EXE，勿误跳过
    if (mod == SkipModuleForPatch() && SkipModuleForPatch() != 0) {
      return;
    }
    refreshing_ = true;
    std::vector<Stub*> snap;
    {
      std::lock_guard<std::mutex> lock(mu_);
      snap = active_;
    }
    int total = 0;
    for (std::size_t i = 0; i < snap.size(); ++i) {
      Stub* s = snap[i];
      if (!s) {
        continue;
      }
      char path_buf[MAX_PATH];
      path_buf[0] = '\0';
      GetModuleFileNameA(mod, path_buf, MAX_PATH);
      if (s->scope == 1 && s->caller_allow) {
        if (!s->caller_allow(path_buf, s->caller_allow_arg)) {
          continue;
        }
      } else if (s->scope == 0 && !s->caller_path.empty()) {
        if (!AllowBySubstr(path_buf, s)) {
          continue;
        }
      }
      const char* dll = s->callee_path.empty() ? 0 : s->callee_path.c_str();
      std::vector<iat::Patch> local;
      total += iat::PatchModule(mod, dll, s->sym_name.c_str(), s->new_func,
                                &local);
      if (!local.empty()) {
        std::lock_guard<std::mutex> lock(mu_);
        for (std::size_t j = 0; j < local.size(); ++j) {
          patches_.push_back(local[j]);
          stub_patches_[s].push_back(local[j]);
        }
      }
    }
    refreshing_ = false;
    if (total > 0) {
      std::fprintf(stderr,
                   "[tray_hooks] win_iat(auto): patched %d slot(s) on new "
                   "module %p\n",
                   total, static_cast<void*>(mod));
    }
  }

private:
  void InstallLoaderHooks() {
    // 首选 LdrRegisterDllNotification（Vista+），无 IAT hook LoadLibrary，避免 CRT 重入
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
      g_LdrRegister = reinterpret_cast<PFN_LdrRegisterDllNotification>(
          GetProcAddress(ntdll, "LdrRegisterDllNotification"));
      g_LdrUnregister = reinterpret_cast<PFN_LdrUnregisterDllNotification>(
          GetProcAddress(ntdll, "LdrUnregisterDllNotification"));
    }
    if (g_LdrRegister) {
      const LONG st =
          g_LdrRegister(0, &LdrNotifyThunk, this, &g_ldr_cookie);
      if (st >= 0 && g_ldr_cookie) {
        use_ldr_notify_ = true;
        std::fprintf(stderr,
                     "[tray_hooks] win_iat: AUTOMATIC via "
                     "LdrRegisterDllNotification\n");
        return;
      }
    }

    // 回退：IAT hook LoadLibrary*（可用 TRAY_HOOKS_IAT_LOADLIBRARY=1 强制）
    const char* force = std::getenv("TRAY_HOOKS_IAT_LOADLIBRARY");
    if (force && (*force == '0' || *force == 'n' || *force == 'N')) {
      std::fprintf(stderr,
                   "[tray_hooks] win_iat: LdrNotify unavailable and "
                   "IAT LoadLibrary disabled\n");
      return;
    }
    HMODULE skip = SkipModuleForPatch();
    struct Item {
      const char* sym;
      void* proxy;
    };
    const Item items[] = {
        {"LoadLibraryA", reinterpret_cast<void*>(&ProxyLoadLibraryA)},
        {"LoadLibraryW", reinterpret_cast<void*>(&ProxyLoadLibraryW)},
        {"LoadLibraryExA", reinterpret_cast<void*>(&ProxyLoadLibraryExA)},
        {"LoadLibraryExW", reinterpret_cast<void*>(&ProxyLoadLibraryExW)},
        {0, 0}};
    for (int i = 0; items[i].sym; ++i) {
      iat::PatchAllModules("kernel32.dll", items[i].sym, items[i].proxy,
                           &loader_patches_, skip);
      iat::PatchAllModules(0, items[i].sym, items[i].proxy, &loader_patches_,
                           skip);
    }
    std::fprintf(stderr,
                 "[tray_hooks] win_iat: AUTOMATIC fallback LoadLibrary IAT "
                 "(%zu patches)\n",
                 loader_patches_.size());
  }

  static VOID NTAPI LdrNotifyThunk(ULONG reason,
                                   const TRAY_LDR_DLL_NOTIFICATION_DATA* data,
                                   PVOID) {
    if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED || !data || !g_win) {
      return;
    }
    HMODULE mod = reinterpret_cast<HMODULE>(data->Loaded.DllBase);
    if (mod) {
      g_win->OnModuleLoaded(mod);
    }
  }

  static HMODULE WINAPI ProxyLoadLibraryA(LPCSTR name) {
    HMODULE m = g_real_LLA ? g_real_LLA(name) : 0;
    if (g_win && m) {
      g_win->OnModuleLoaded(m);
    }
    return m;
  }

  static HMODULE WINAPI ProxyLoadLibraryW(LPCWSTR name) {
    HMODULE m = g_real_LLW ? g_real_LLW(name) : 0;
    if (g_win && m) {
      g_win->OnModuleLoaded(m);
    }
    return m;
  }

  static HMODULE WINAPI ProxyLoadLibraryExA(LPCSTR name, HANDLE h, DWORD flags) {
    HMODULE m = g_real_LLEA ? g_real_LLEA(name, h, flags) : 0;
    if (g_win && m) {
      g_win->OnModuleLoaded(m);
    }
    return m;
  }

  static HMODULE WINAPI ProxyLoadLibraryExW(LPCWSTR name, HANDLE h,
                                            DWORD flags) {
    HMODULE m = g_real_LLEW ? g_real_LLEW(name, h, flags) : 0;
    if (g_win && m) {
      g_win->OnModuleLoaded(m);
    }
    return m;
  }

  tray_hooks_mode_t mode_;
  bool automatic_ = false;
  bool use_ldr_notify_ = false;
  bool refreshing_ = false;
  std::mutex mu_;
  std::vector<iat::Patch> patches_;
  std::vector<iat::Patch> loader_patches_;
  std::map<Stub*, std::vector<iat::Patch> > stub_patches_;
  std::vector<Stub*> active_;
};

Backend* CreateBackend() { return new WinIatBackend(); }

#endif

}  // namespace detail
}  // namespace tray_hooks
