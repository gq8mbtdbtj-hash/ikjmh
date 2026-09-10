/**
 * @file win_iat_backend.cpp
 * @brief tray_hooks 的 Windows 后端：真 IAT 改写 + AUTOMATIC 晚加载补 hook。
 *
 * AUTOMATIC：在 init 时 hook LoadLibrary(A|W|ExA|ExW)；新模块加载成功后，
 * 对其 IAT 重放所有仍活跃的用户 stub（已 hook 槽位会被跳过）。
 */

#include "internal/backend.hpp"
#include "plat/win_iat_patch.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <map>
#include <mutex>
#include <vector>
#endif

namespace tray_hooks {
namespace detail {

#ifdef _WIN32

namespace {

HMODULE SelfModule() {
  HMODULE m = 0;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&SelfModule), &m);
  return m;
}

typedef HMODULE(WINAPI* LoadLibraryA_fn)(LPCSTR);
typedef HMODULE(WINAPI* LoadLibraryW_fn)(LPCWSTR);
typedef HMODULE(WINAPI* LoadLibraryExA_fn)(LPCSTR, HANDLE, DWORD);
typedef HMODULE(WINAPI* LoadLibraryExW_fn)(LPCWSTR, HANDLE, DWORD);

LoadLibraryA_fn g_real_LLA = 0;
LoadLibraryW_fn g_real_LLW = 0;
LoadLibraryExA_fn g_real_LLEA = 0;
LoadLibraryExW_fn g_real_LLEW = 0;

}  // namespace

class WinIatBackend;
static WinIatBackend* g_win = 0;

class WinIatBackend : public Backend {
public:
  const char* name() const {
    return automatic_ ? "win_iat(auto)" : "win_iat";
  }

  int init(tray_hooks_mode_t mode) {
    mode_ = mode;
    automatic_ = (mode == TRAY_HOOKS_MODE_AUTOMATIC);
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
      // 先拿到真函数再改 IAT，避免代理内再走被改写的导入
      InstallLoaderHooks();
    }
    return TRAY_HOOKS_OK;
  }

  void uninit() {
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
    void* prev = resolve_sym(dll, stub->sym_name.c_str());
    stub->prev_func = prev ? prev : stub->new_func;

    std::vector<iat::Patch> local;
    const int n = iat::PatchAllModules(dll, stub->sym_name.c_str(),
                                       stub->new_func, &local, SelfModule());
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
                 "(prev=%p proxy=%p)\n",
                 stub->sym_name.c_str(), n, stub->prev_func, stub->new_func);
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
    if (mod == SelfModule()) {
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
    HMODULE skip = SelfModule();
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
      // 部分进程从 api-ms-win-core-libraryloader-*.dll 导入
      iat::PatchAllModules(0, items[i].sym, items[i].proxy, &loader_patches_,
                           skip);
    }
    std::fprintf(stderr,
                 "[tray_hooks] win_iat: AUTOMATIC loader hooks installed "
                 "(%zu patches)\n",
                 loader_patches_.size());
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
