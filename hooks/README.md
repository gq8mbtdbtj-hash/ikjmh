# tray_hooks — 跨平台 Hook 采集 & Backtrace

参考 [xhook](https://github.com/iqiyi/xHook) / [bhook (ByteHook)](https://github.com/bytedance/bhook) 的 **caller 侧 PLT/GOT 改写**思路，提供统一采集 API，覆盖：

| 平台 | Hook 后端 | Backtrace |
|------|-----------|-----------|
| **Android** | 自研 ELF PLT/GOT（量产可对接 bytehook） | `_Unwind_Backtrace` / libunwind |
| **Linux** | 自研 ELF PLT/GOT（`dl_iterate_phdr`） | `backtrace` / unwind |
| **QNX** | ELF PLT/GOT（QNX ELF + `dl*`） | unwind / 手写 FP |
| **鸿蒙 (OHOS)** | ELF PLT/GOT（注意 linker namespace） | unwind |
| **Windows** | 自研 **IAT**（PE 导入表） | `CaptureStackBackTrace` |

> 当前交付：**统一 C ABI + collector/APM + 可工作 backtrace + 真 GOT/IAT 后端 + memprobe 注入**。  
> Android 量产可在 `CreateBackend()` 换成 bytehook，业务代码无需改动。

## 文档（架构 / 设计）

| 文档 | 内容 |
|------|------|
| [docs/INDEX.md](docs/INDEX.md) | 文档索引 |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | 分层架构、模块边界、扩展点 |
| [docs/API.md](docs/API.md) | C ABI 契约与调用顺序 |
| [docs/DATAFLOW.md](docs/DATAFLOW.md) | 事件 / 过滤 / APM / 记账数据流 |
| [docs/PLATFORM.md](docs/PLATFORM.md) | ELF GOT / Win IAT / Backtrace 实现备忘 |
| [docs/ANDROID.md](docs/ANDROID.md) | Android 落地、注入、量产与验收 |
| [docs/LEARNING.md](docs/LEARNING.md) | 相关学习资料与推荐路径 |
| [docs/ROADMAP.md](docs/ROADMAP.md) | 演进与明确不做 |
| [memprobe/README.md](memprobe/README.md) | 无编译注入探针 |

## 架构（简图）

```text
          ┌─────────────────────────────┐
          │  App / 埋点 / APM / 注入     │
          └─────────────┬───────────────┘
                        │  tray_hooks_* / tray_memprobe_*
          ┌─────────────▼───────────────┐
          │  collector + apm + memprobe │
          └─────────────┬───────────────┘
               ┌────────┴────────┐
               ▼                 ▼
        hook backend        backtrace
   elf_plt | win_iat     posix | win
```

**与 bhook 对齐：** hook **调用者** GOT/IAT；`hook_single` / `partial` / `all`；proxy 内 `TRAY_HOOKS_CALL_PREV`。

## 快速使用

```c
#include "tray_hooks/hooks.h"
#include "tray_hooks/collector.h"
#include "tray_hooks/backtrace.h"

static void* ProxyMalloc(size_t n) {
  tray_hooks_frame_t frames[32];
  int nframes = tray_hooks_backtrace(frames, 32, 1);
  tray_hooks_collector_record("malloc", frames, nframes, (uint64_t)n);
  typedef void* (*malloc_fn)(size_t);
  return TRAY_HOOKS_CALL_PREV(ProxyMalloc, malloc_fn, n);
}

void setup(void) {
  tray_hooks_init(TRAY_HOOKS_MODE_AUTOMATIC);
  tray_hooks_collector_set_sink(my_sink, NULL);
  tray_hooks_hook_all(NULL, "malloc", (void*)ProxyMalloc, NULL, NULL);
}
```

## 构建

```bash
# 在仓库根目录
cmake -S . -B build -DTRAY_HOOKS_BUILD=ON
cmake --build build --target tray_hooks tray_memprobe -j

# 或仅 hooks 子树
cmake -S hooks -B build_hooks
cmake --build build_hooks -j
```

Android NDK：

```bash
export ANDROID_NDK_HOME=/path/to/ndk
./hooks/scripts/build_android_ndk.sh           # 默认 arm64-v8a
./hooks/scripts/build_android_ndk.sh x86_64    # 模拟器 ABI
```

详见 [docs/ANDROID.md](docs/ANDROID.md)（含 `android_ndk_smoke` 与 adb 运行说明）。

## 测试与 Doxygen

```bash
cmake --build build --target hooks_unit_tests hooks_apm_tests hook_whitebox memprobe_smoke
ctest --test-dir build -R "hooks_|hook_whitebox" --output-on-failure

cd hooks && doxygen Doxyfile
# → hooks/docs/doxygen/html/index.html
```

白盒 `hook_whitebox`：自建 `tray_wb_lib`，验证真 IAT/PLT、`TRAY_HOOKS_CALL_PREV`、partial、晚加载。

## 对接生产后端

| 目标 | 建议 |
|------|------|
| Android / Linux | **已自研** `elf_plt_patch`；量产可换 bytehook |
| Windows | **已自研** `win_iat_patch` |
| 无编译采集 | 注入 `tray_memprobe`（见 [memprobe/README.md](memprobe/README.md)） |
| GPU/NPU/NEON | 精确 ABI hook / 环境变量 / `tray_memprobe_domain_*` |
| 持续观测 | `TRAY_HOOKS_APM_FILE` / `TRAY_HOOKS_APM_URL` |
| 降噪 | `TRAY_HOOKS_FILTER_*` |

## 合规说明

面向 **自有进程内** 诊断 / APM / 崩溃分析。请勿用于未授权注入第三方进程或绕过安全机制。

## tray_memprobe 快速注入

```bash
cmake --build build --target tray_memprobe memprobe_smoke
LD_PRELOAD=$PWD/build/hooks/libtray_memprobe.so \
  TRAY_MEMPROBE_LOG=/tmp/memprobe.txt \
  ./build/hooks/memprobe_smoke

./hooks/scripts/inject_memprobe.sh ./your_app
# Windows: ./hooks/scripts/inject_memprobe.ps1 -Target ./your_app.exe
```
