# tray_hooks — 跨平台 Hook 采集 & Backtrace

参考 [xhook](https://github.com/iqiyi/xHook) / [bhook (ByteHook)](https://github.com/bytedance/bhook) 的 **caller 侧 PLT/GOT 改写**思路，提供统一采集 API，覆盖：

| 平台 | Hook 后端（规划） | Backtrace |
|------|-------------------|-----------|
| **Android** | ELF PLT/GOT（可对接 bytehook / xhook） | `_Unwind_Backtrace` / libunwind |
| **Linux** | ELF PLT/GOT（`dl_iterate_phdr`） | `backtrace` / unwind |
| **QNX** | ELF PLT/GOT（QNX ELF + `dl*`） | unwind / 手写 FP |
| **鸿蒙 (OHOS)** | ELF PLT/GOT（与 Android 类似，注意 linker namespace） | unwind |
| **Windows** | **IAT**（PE 导入表，对应 PLT 的 caller 侧） | `CaptureStackBackTrace` |

> 当前仓库交付：**统一 API + 采集器 + 可工作的 backtrace + 各平台后端桩**。  
> 完整生产级 GOT/IAT 改写体量大、需按机型/加固验证；Android 生产建议直接链入 **bytehook**，本目录作为门面与多端采集统一层。

## 架构

```text
          ┌─────────────────────────────┐
          │  App / 埋点 / APM 模块       │
          └─────────────┬───────────────┘
                        │  tray_hooks_* API
          ┌─────────────▼───────────────┐
          │  collector（事件 + 堆栈）    │
          └─────────────┬───────────────┘
               ┌────────┴────────┐
               ▼                 ▼
        hook backend        backtrace
   elf_plt | win_iat     posix | win
```

**与 bhook 对齐的概念：**

- Hook **调用者**（caller）的 GOT/IAT，而不是改 callee 代码（非 inline hook）
- `hook_single` / `hook_partial` / `hook_all`
- Proxy 内可采 backtrace；`call_prev` 调用原符号

## 快速使用

```c
#include "tray_hooks/hooks.h"
#include "tray_hooks/collector.h"

static void* my_malloc(size_t n) {
  tray_hooks_frame_t frames[32];
  int nframes = tray_hooks_backtrace(frames, 32, 1);
  tray_hooks_collector_record("malloc", frames, nframes, (uint64_t)n);
  return tray_hooks_call_prev(my_malloc, n); /* 桩后端可能直接调 libc */
}

void setup(void) {
  tray_hooks_init(TRAY_HOOKS_MODE_AUTO);
  tray_hooks_collector_set_sink(my_sink, NULL);
  tray_hooks_hook_all(NULL, "malloc", (void*)my_malloc, NULL, NULL);
}
```

## 构建

根 `CMakeLists.txt` 选项：

```bash
cmake -S . -B build -DTRAY_DEMO_BUILD_HOOKS=ON
cmake --build build --target tray_hooks
# 可选示例
cmake --build build --target tray_hooks_sample
```

Android NDK：

```bash
cmake -S hooks -B build_hooks \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
```

## 对接生产后端

| 目标 | 建议 |
|------|------|
| Android / Linux | **已自研** `elf_plt_patch`（不链 xhook/bhook） |
| Windows | **已自研** `win_iat_patch` 真 IAT 改写 |
| 无编译采集 | 注入 `tray_memprobe`（见 [memprobe/README.md](memprobe/README.md)） |
| GPU/NPU/NEON | 精确 GPU hook / 环境变量 / `domain_alloc` |
| 持续观测 | `tray_hooks/apm.h`：`TRAY_HOOKS_APM_FILE` / `URL` |
| 降噪 | `TRAY_HOOKS_FILTER_TAGS|TIDS|MODULES|...` |

## 合规说明

本模块面向 **自有进程内的诊断 / APM / 崩溃分析**。请勿用于未授权注入第三方进程或绕过安全机制。

## tray_memprobe（堆栈 + 用量，可 patchelf / PE 注入）

见 **[memprobe/README.md](memprobe/README.md)**。

```bash
cmake --build build --target tray_memprobe memprobe_smoke
LD_PRELOAD=$PWD/build/hooks/libtray_memprobe.so \
  TRAY_MEMPROBE_LOG=/tmp/memprobe.txt \
  ./build/hooks/memprobe_smoke

# 持久注入 ELF：
./hooks/scripts/inject_memprobe.sh ./your_app

# 持久注入 PE（Windows）：
./hooks/scripts/inject_memprobe.ps1 -Target ./your_app.exe
```
