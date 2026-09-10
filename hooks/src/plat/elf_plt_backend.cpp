/**
 * @file elf_plt_backend.cpp
 * @brief tray_hooks 的 ELF 后端 + AUTOMATIC（dlopen / android_dlopen_ext）。
 *
 * AUTOMATIC：init 时 hook dlopen（及 Android 的 android_dlopen_ext）；
 * 成功返回后对全部活跃 stub 再跑一遍 PatchSymbol（已改写槽位会跳过）。
 */

#include "internal/backend.hpp"

#if !defined(_WIN32)
#include "plat/elf_plt_patch.hpp"

#include <dlfcn.h>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>
#endif

namespace tray_hooks {
namespace detail {

#if !defined(_WIN32)

namespace {

typedef void* (*dlopen_fn)(const char*, int);
typedef void* (*android_dlopen_ext_fn)(const char*, int, const void*);

dlopen_fn g_real_dlopen = 0;
android_dlopen_ext_fn g_real_android_dlopen_ext = 0;

}  // namespace

class ElfPltBackend;
static ElfPltBackend* g_elf = 0;

class ElfPltBackend : public Backend {
public:
  const char* name() const {
#if defined(__ANDROID__)
    return automatic_ ? "elf_plt(android,auto)" : "elf_plt(android)";
#elif defined(__OHOS__)
    return automatic_ ? "elf_plt(ohos,auto)" : "elf_plt(ohos)";
#elif defined(__QNX__) || defined(__QNXNTO__)
    return automatic_ ? "elf_plt(qnx,auto)" : "elf_plt(qnx)";
#else
    return automatic_ ? "elf_plt(linux,auto)" : "elf_plt(linux)";
#endif
  }

  int init(tray_hooks_mode_t mode) {
    mode_ = mode;
    automatic_ = (mode == TRAY_HOOKS_MODE_AUTOMATIC);
    g_elf = this;

    if (automatic_) {
      // 先 dlsym 真实现，再改 GOT
      g_real_dlopen =
          reinterpret_cast<dlopen_fn>(dlsym(RTLD_NEXT, "dlopen"));
      if (!g_real_dlopen) {
        g_real_dlopen =
            reinterpret_cast<dlopen_fn>(dlsym(RTLD_DEFAULT, "dlopen"));
      }
#if defined(__ANDROID__) || defined(__OHOS__)
      g_real_android_dlopen_ext = reinterpret_cast<android_dlopen_ext_fn>(
          dlsym(RTLD_NEXT, "android_dlopen_ext"));
      if (!g_real_android_dlopen_ext) {
        g_real_android_dlopen_ext = reinterpret_cast<android_dlopen_ext_fn>(
            dlsym(RTLD_DEFAULT, "android_dlopen_ext"));
      }
#endif
      InstallLoaderHooks();
    }
    (void)mode_;
    return TRAY_HOOKS_OK;
  }

  void uninit() {
    std::lock_guard<std::mutex> lock(mu_);
    for (std::size_t i = 0; i < loader_patches_.size(); ++i) {
      elfplt::Restore(loader_patches_[i]);
    }
    loader_patches_.clear();
    for (std::size_t i = 0; i < patches_.size(); ++i) {
      elfplt::Restore(patches_[i]);
    }
    patches_.clear();
    stub_patches_.clear();
    active_.clear();
    if (g_elf == this) {
      g_elf = 0;
    }
  }

  Stub* hook(Stub* stub) {
    if (!stub) {
      return 0;
    }
    void* prev = resolve_sym(
        stub->callee_path.empty() ? 0 : stub->callee_path.c_str(),
        stub->sym_name.c_str());
    stub->prev_func = prev ? prev : stub->new_func;

    const char* caller =
        stub->caller_path.empty() ? 0 : stub->caller_path.c_str();
    std::vector<elfplt::Patch> local;
    const int n = elfplt::PatchSymbol(caller, stub->sym_name.c_str(),
                                      stub->new_func, &local);
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (std::size_t i = 0; i < local.size(); ++i) {
        patches_.push_back(local[i]);
      }
      stub_patches_[stub] = local;
      active_.push_back(stub);
    }
    std::fprintf(stderr,
                 "[tray_hooks] %s: hooked '%s' in %d GOT slot(s)\n", name(),
                 stub->sym_name.c_str(), n);
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
    std::map<Stub*, std::vector<elfplt::Patch> >::iterator it =
        stub_patches_.find(stub);
    if (it == stub_patches_.end()) {
      return TRAY_HOOKS_OK;
    }
    for (std::size_t i = 0; i < it->second.size(); ++i) {
      elfplt::Restore(it->second[i]);
    }
    stub_patches_.erase(it);
    return TRAY_HOOKS_OK;
  }

  void* resolve_sym(const char* module, const char* sym) {
    if (!sym || !*sym) {
      return 0;
    }
    void* handle = RTLD_DEFAULT;
    if (module && *module) {
#if defined(RTLD_NOLOAD)
      handle = g_real_dlopen
                   ? g_real_dlopen(module, RTLD_NOLOAD | RTLD_NOW)
                   : dlopen(module, RTLD_NOLOAD | RTLD_NOW);
#endif
      if (!handle) {
        handle = g_real_dlopen ? g_real_dlopen(module, RTLD_NOW)
                               : dlopen(module, RTLD_NOW);
      }
    }
    return dlsym(handle ? handle : RTLD_DEFAULT, sym);
  }

  /** dlopen 成功后重放全部用户 hook（全进程扫描，幂等） */
  void OnSoLoaded(const char* /*path*/) {
    if (!automatic_ || refreshing_) {
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
      const char* caller =
          s->caller_path.empty() ? 0 : s->caller_path.c_str();
      std::vector<elfplt::Patch> local;
      total += elfplt::PatchSymbol(caller, s->sym_name.c_str(), s->new_func,
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
                   "[tray_hooks] %s: re-patched %d GOT slot(s) after dlopen\n",
                   name(), total);
    }
  }

private:
  void InstallLoaderHooks() {
    if (g_real_dlopen) {
      elfplt::PatchSymbol(0, "dlopen", reinterpret_cast<void*>(&ProxyDlopen),
                          &loader_patches_);
    }
    if (g_real_android_dlopen_ext) {
      elfplt::PatchSymbol(0, "android_dlopen_ext",
                          reinterpret_cast<void*>(&ProxyAndroidDlopenExt),
                          &loader_patches_);
    }
    std::fprintf(stderr,
                 "[tray_hooks] elf_plt: AUTOMATIC loader hooks installed "
                 "(%zu patches)\n",
                 loader_patches_.size());
  }

  static void* ProxyDlopen(const char* path, int flags) {
    void* h = g_real_dlopen ? g_real_dlopen(path, flags) : 0;
    if (g_elf && h) {
      g_elf->OnSoLoaded(path);
    }
    return h;
  }

  static void* ProxyAndroidDlopenExt(const char* path, int flags,
                                     const void* ext) {
    void* h = g_real_android_dlopen_ext
                  ? g_real_android_dlopen_ext(path, flags, ext)
                  : 0;
    if (g_elf && h) {
      g_elf->OnSoLoaded(path);
    }
    return h;
  }

  tray_hooks_mode_t mode_;
  bool automatic_ = false;
  bool refreshing_ = false;
  std::mutex mu_;
  std::vector<elfplt::Patch> patches_;
  std::vector<elfplt::Patch> loader_patches_;
  std::map<Stub*, std::vector<elfplt::Patch> > stub_patches_;
  std::vector<Stub*> active_;
};

Backend* CreateBackend() { return new ElfPltBackend(); }

#endif

}  // namespace detail
}  // namespace tray_hooks
