# Hook 采集相关学习资料

**状态：** v1.0  
**目的：** 按学习路径整理与本仓库 `tray_hooks` / `tray_memprobe` 相关的外部资料，便于新人上手与量产排障。  
**配套设计文档：** [ARCHITECTURE.md](ARCHITECTURE.md) · [PLATFORM.md](PLATFORM.md) · [ANDROID.md](ANDROID.md)

> 合规提醒：下列资料仅用于 **自有进程内** 诊断 / APM / 崩溃分析学习。请勿用于未授权注入或绕过安全机制。

---

## 0. 建议学习顺序（约 1–2 周可通读）

| 阶段 | 目标 | 优先材料 | 对照本仓库 |
|------|------|----------|------------|
| 1. 概念 | 分清 PLT/GOT、caller 侧 vs inline | §1 ELF 基础 + §2 ByteHook 原理 | [ARCHITECTURE.md](ARCHITECTURE.md) §3 |
| 2. API 对齐 | 理解 `hook_all` / `CALL_PREV` / AUTOMATIC | §2 ByteHook 手册 | [API.md](API.md) |
| 3. Android 落地 | NDK、namespace、晚加载 | §3 Android linker + §4 注入 | [ANDROID.md](ANDROID.md) |
| 4. 采栈与观测 | unwind、符号化、降噪 | §5 Backtrace + §6 APM | [DATAFLOW.md](DATAFLOW.md) |
| 5. Windows 对照 | IAT ≈ GOT | §7 Windows IAT | [PLATFORM.md](PLATFORM.md) §3 |
| 6. 加固边界 | RELRO、失败可观测 | §8 加固与限制 | [PLATFORM.md](PLATFORM.md) §2.4 |

---

## 1. ELF / PLT / GOT 基础（必读）

理解「为什么改 **调用者** 的 GOT，而不是改 `libc` 代码」。

| 资料 | 类型 | 说明 |
|------|------|------|
| [System V ABI — ELF](https://refspecs.linuxfoundation.org/elf/elf.pdf) | 规范 | `PT_DYNAMIC`、`DT_JMPREL`、`R_*_JUMP_SLOT` 权威定义 |
| [ELF Handling For Thread-Local Storage](https://www.akkadia.org/drepper/tls.pdf)（可选） | 论文 | TLS 与重定位背景；本仓库暂不依赖 |
| [Linkers and Loaders（Levine）](https://www.iecc.com/linker/) | 书 | 动态链接与 PLT 机制总览 |
| [A Whirlwind Tutorial on Creating Really Teensy ELF Executables](https://www.muppetlabs.com/~breadbox/software/tiny/teensy.html)（可选） | 教程 | 加深对 ELF 结构的直觉 |
| `man 5 elf` / `man 3 dl_iterate_phdr` | man | 本仓库 ELF 枚举入口 |

**对照本仓库：** `src/plat/elf_plt_patch.*` 遍历 `dl_iterate_phdr` → `DT_JMPREL` → 写 JUMP_SLOT。

---

## 2. Android PLT Hook 生态（核心对齐）

本仓库 API 形态对齐 **ByteHook / xHook**：caller 侧 GOT，非 inline。

### 2.1 ByteHook（bhook）— 量产首选参考

| 资料 | 说明 |
|------|------|
| [bytedance/bhook](https://github.com/bytedance/bhook) | 官方仓库；Android PLT hook |
| [ByteHook 文档目录](https://github.com/bytedance/bhook/tree/main/doc) | 英文/中文文档入口 |
| [native_manual.zh-CN.md](https://github.com/bytedance/bhook/blob/main/doc/native_manual.zh-CN.md) | **中文手册**：caller/callee、`hook_single/partial/all`、`CALL_PREV`、`POP_STACK`、`dlopen` 自动补丁 |
| [README — Hook API](https://github.com/bytedance/bhook/blob/main/README.md) | `bytehook_init` / `hook_all` / AUTOMATIC 语义速查 |

**与本仓库概念对照：**

| ByteHook | tray_hooks |
|----------|------------|
| `bytehook_init(AUTOMATIC/MANUAL)` | `tray_hooks_init(TRAY_HOOKS_MODE_*)` |
| `bytehook_hook_all/partial/single` | `tray_hooks_hook_all/partial/single` |
| `BYTEHOOK_CALL_PREV` | `TRAY_HOOKS_CALL_PREV` / `tray_hooks_get_prev` |
| 内部 hook `dlopen` / `android_dlopen_ext` | ELF 后端 AUTOMATIC 同思路 |
| `CreateBackend()` → ByteHook 适配器 | 见 [ANDROID.md](ANDROID.md) §6 |

### 2.2 xHook — 早期经典实现

| 资料 | 说明 |
|------|------|
| [iqiyi/xHook](https://github.com/iqiyi/xHook) | 早期开源 Android PLT hook；思路与 ByteHook 同源 |
| 仓库内 README / 示例 | 适合对照 GOT 改写步骤做源码阅读 |

### 2.3 Inline Hook（对照，非本仓库路线）

| 资料 | 说明 |
|------|------|
| [bytedance/android-inline-hook (ShadowHook)](https://github.com/bytedance/android-inline-hook) | **改 callee 指令**；与本仓库 caller 侧模型不同，仅作边界对照 |
| [frida](https://frida.re/docs/home/)（可选） | 动态插桩工具链；适合理解「拦截」产品形态，非本库实现路径 |

---

## 3. Android 动态链接与命名空间

量产失败多出在 **linker namespace / 晚加载 / RELRO**，而非 hook API 本身。

| 资料 | 说明 |
|------|------|
| [Android Linker Namespace](https://source.android.com/docs/core/runtime/linker-namespaces) | App / vendor / system 命名空间隔离 |
| [Android NDK — Native Libraries](https://developer.android.com/ndk/guides/concepts) | JNI、so 加载基础 |
| [android_dlopen_ext](https://developer.android.com/ndk/reference/group/libdl) | 带 extinfo 的加载；本仓库 AUTOMATIC 会尝试 hook |
| [Wrap.sh](https://developer.android.com/ndk/guides/wrap-script) | debuggable 包用 `LD_PRELOAD` 注入探针 |
| [CMake Android toolchain](https://developer.android.com/ndk/guides/cmake) | NDK 交叉编译；见本仓库 README / ANDROID.md |

**对照本仓库：** `elf_plt_backend` 在 AUTOMATIC 下 hook `dlopen` / `android_dlopen_ext` 并重放 stub。

---

## 4. 注入与无编译采集

| 资料 | 说明 |
|------|------|
| `man 8 ld.so`（`LD_PRELOAD`） | Linux/Android 预加载 |
| [patchelf](https://github.com/NixOS/patchelf) | ELF 追加 `NEEDED`；对应 `hooks/scripts/inject_memprobe.sh` |
| [Android wrap.sh](https://developer.android.com/ndk/guides/wrap-script) | APK 内启动包装 |
| PE 导入表 / Delay-Load（MSDN） | Windows 侧持久依赖；对应 `inject_memprobe.ps1` |

**对照本仓库：** [memprobe/README.md](../memprobe/README.md)。

---

## 5. Backtrace / Unwind / 符号化

| 资料 | 说明 |
|------|------|
| [libunwind](https://www.nongnu.org/libunwind/) | 跨平台 unwind |
| LLVM libunwind / `_Unwind_Backtrace` | Android/OHOS 常用路径；见 `backtrace_all.cpp` |
| `man 3 backtrace` | glibc 抓栈 |
| [Windows CaptureStackBackTrace](https://learn.microsoft.com/en-us/windows/win32/api/utilapiset/nf-utilapiset-capturestackbacktrace) | Win 抓栈 |
| [DbgHelp SymFromAddr](https://learn.microsoft.com/en-us/windows/win32/api/dbghelp/nf-dbghelp-symfromaddr) | Win 符号化 |
| `llvm-symbolizer` / `addr2line` / `ndk-stack` | 离线符号化（生产常保留 module+offset） |

**对照本仓库：** [PLATFORM.md](PLATFORM.md) §4 · `src/backtrace/backtrace_all.cpp`。

---

## 6. 采集、过滤与 APM 实践

| 主题 | 资料 / 关键词 | 对照本仓库 |
|------|----------------|------------|
| 分配采样 | jemalloc / tcmalloc profiling 思路；「按 size / 采样率降噪」 | `TRAY_MEMPROBE_SAMPLE` / `MIN_SIZE`、`FILTER_*` |
| 持续观测格式 | NDJSON、OpenTelemetry 日志导出（概念） | `TRAY_HOOKS_APM_FILE` / `URL` |
| 内存泄漏排查 | Android Studio Memory Profiler、`malloc_debug` | memprobe dump Top live |

深入数据流：[DATAFLOW.md](DATAFLOW.md)。

---

## 7. Windows IAT（跨平台对照）

| 资料 | 说明 |
|------|------|
| [PE Format — Import Directory](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#import-directory-table) | IAT 结构；对应 ELF GOT |
| [Delay-Loaded DLLs](https://learn.microsoft.com/en-us/cpp/build/reference/linker-support-for-delay-loaded-dlls) | 仅已绑定槽可 hook |
| [LdrRegisterDllNotification](https://learn.microsoft.com/en-us/windows/win32/devnotes/ldrregisterdllnotification) | 晚加载 DLL 通知（本仓库 AUTOMATIC 首选） |
| Microsoft Detours / MinHook（可选） | 多为 **inline**；与本仓库 IAT 路线不同，仅作对照 |

**对照本仓库：** `src/plat/win_iat_*` · [PLATFORM.md](PLATFORM.md) §3。

---

## 8. 加固、RELRO 与失败边界

| 资料 | 说明 |
|------|------|
| [RELRO — HowTo](https://www.redhat.com/en/blog/hardening-elf-binaries-using-relocation-read-only-relro) | Partial / Full RELRO；GOT 只读导致写槽失败 |
| Android 加固 / 壳（厂商文档，按需） | 映射异常、解密后安装 hook |
| SELinux / 应用沙箱（Android 安全文档） | 明确 **不做** 未授权跨进程 |

**对照本仓库：** `WriteSlot` 失败计数、stderr `fails=`；见 ROADMAP「明确不做」。

---

## 9. 本仓库内建「活教材」

读完外部资料后，用仓库内目标验证理解：

| 目标 / 路径 | 学什么 |
|-------------|--------|
| `examples/sample_collect.cpp` | 最小 hook + collector |
| `examples/hook_whitebox.cpp` | 真 PLT/IAT、`CALL_PREV`、partial、晚加载 |
| `examples/memprobe_smoke.cpp` | 注入探针统计冒烟 |
| `tests/test_collector_unit.cpp` | 过滤语义 |
| `tests/test_apm_unit.cpp` | APM 文件写出 |
| `hooks/docs/*` | 设计决策与落地清单 |
| `cd hooks && doxygen Doxyfile` | 带注释的 API 浏览 |

---

## 10. 速查：概念 → 仓库落点

| 概念 | 外部关键词 | 仓库落点 |
|------|------------|----------|
| Caller 侧 hook | PLT/GOT、ByteHook | `elf_plt_patch` / `win_iat_patch` |
| 调原函数 | `CALL_PREV` | `tray_hooks_get_prev` |
| 新 so 自动补丁 | `android_dlopen_ext` | AUTOMATIC 模式 |
| 降噪 | tag/tid/module/sample | `collector` + `TRAY_HOOKS_FILTER_*` |
| 持续观测 | NDJSON / HTTP batch | `apm.h` |
| 无编译注入 | LD_PRELOAD / patchelf / wrap.sh | `tray_memprobe` |
| 量产换后端 | ByteHook | `CreateBackend()` |

---

## 11. 维护说明

- 外部链接可能变更；以各项目官方 README / `doc/` 为准。
- 新增资料时：注明 **与本仓库哪一层对齐**，避免与 inline-hook 路线混淆。
- 更新本文件后，同步 [INDEX.md](INDEX.md) 与 `Doxyfile` 的 `INPUT`（若新增独立 md）。
