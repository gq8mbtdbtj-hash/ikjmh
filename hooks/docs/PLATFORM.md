# tray_hooks 平台实现备忘

**状态：** v1.1  
**配套：** [ARCHITECTURE.md](ARCHITECTURE.md) · [ANDROID.md](ANDROID.md)

本文记录各平台 **Backend 如何改写导入槽** 与 backtrace 选型，供实现与量产排查使用。

---

## 1. 统一模型

所有平台都遵循 **Caller 侧** 改写：

1. 找到「谁在调用」该符号（caller 模块的导入表 / GOT）；
2. 把槽位从「原函数」改成「proxy」；
3. 保存原地址供 `tray_hooks_get_prev` / `TRAY_HOOKS_CALL_PREV`；
4. AUTOMATIC 模式下拦截模块加载，对新模块 **重放** 活跃 stub。

**不做：** 改写 callee 代码段（inline hook / shadowhook）。

```text
  Caller module                         Callee (libc / …)
  ┌─────────────────┐                   ┌──────────────┐
  │ call [GOT/IAT]  │ ───────────────▶  │ real_sym     │
  │ slot ──▶ proxy  │                   └──────────────┘
  └────────┬────────┘
           │
           ▼
        Proxy → get_prev → real_sym
```

---

## 2. ELF 系（Android / Linux / QNX / 鸿蒙）

**实现文件：** `src/plat/elf_plt_patch.*` · `elf_plt_backend.cpp`  
**后端名：** `elf_plt(<os>[,auto])`

### 2.1 原理（同 xhook / ByteHook）

1. `libA.so` 调用 `malloc` 时读的是 **libA 自己的** GOT 槽；
2. Hook = 把该槽改为 `proxy`；原地址写入 `Patch.original`；
3. 不要改 `libc` 代码段。

### 2.2 关键步骤

| 步骤 | API / 结构 |
|------|------------|
| 枚举 so | `dl_iterate_phdr`（Android 5+）；老版本可读 `/proc/self/maps` |
| 找 DYNAMIC | `PT_DYNAMIC` |
| 找 rel | `DT_JMPREL` + `DT_PLTRELSZ` / `DT_PLTRELS` |
| 符号 | `DT_SYMTAB` / `DT_STRTAB`；匹配 JUMP_SLOT 重定位中的符号名 |
| 写 GOT | `mprotect(PROT_READ\|PROT_WRITE)` + 指针赋值 + ARM/ARM64 `clear_cache` |
| 新 so | AUTOMATIC：hook `dlopen`；Android/OHOS 另 hook `android_dlopen_ext` |

### 2.3 架构宏

| 宏条件 | JUMP_SLOT 类型 |
|--------|----------------|
| `__x86_64__` | `R_X86_64_JUMP_SLOT` |
| `__aarch64__` | `R_AARCH64_JUMP_SLOT` |
| `__arm__` | `R_ARM_JUMP_SLOT` |
| `__i386__` | `R_386_JMP_SLOT` |
| 其它 | 类型宏为 0：退化为「忽略类型，只比符号名」 |

### 2.4 RELRO / 加固

- FULL RELRO 下 GOT 只读；`mprotect` 可能失败。
- `PatchSymbol(..., fail_out)` 累计失败次数；stderr 打印 `(RELRO/hardening?)`。
- 量产不可假设 100% 槽位可写；应用失败计数做覆盖率监控。

### 2.5 鸿蒙（OHOS）

- 链接器与 namespace 接近 Android；优先验证 `dl_iterate_phdr`。
- 后端名：`elf_plt(ohos[,auto])`。
- 可先用 bytehook 源码移植评估，再替换 `CreateBackend()`。

### 2.6 QNX

- ELF + `dlopen`/`dlsym`；确认 `dl_iterate_phdr` 或等价枚举。
- 对 GOT 页 `mprotect` 可能受 procmgr 策略限制。

### 2.7 Android 量产建议

```cpp
// 在 plat 侧提供 BytehookBackend，或于 CreateBackend() 分支：
#include "bytehook.h"
// tray_hooks_hook_all  → bytehook_hook_all
// tray_hooks_get_prev  → 与 BYTEHOOK_CALL_PREV 语义对齐
```

对外 C ABI **不变**。落地清单见 [ANDROID.md](ANDROID.md)。

---

## 3. Windows（IAT）

**实现文件：** `src/plat/win_iat_patch.*` · `win_iat_backend.cpp`  
**后端名：** `win_iat` / `win_iat(auto)`

| 步骤 | 说明 |
|------|------|
| 枚举模块 | `CreateToolhelp32Snapshot` / PEB `InLoadOrderModuleList` |
| 找导入 | `IMAGE_DIRECTORY_ENTRY_IMPORT` → `IMAGE_THUNK_DATA` |
| Delay-Load | 仅 hook **已绑定** 的 Delay-Load 槽 |
| 写 IAT | `VirtualProtect` + 写 `proxy` |
| 新 DLL | 首选 `LdrRegisterDllNotification`；回退 IAT hook `LoadLibrary*` |
| 跳过自身 | 探针作为独立 DLL 时跳过自身模块，避免改到探针导入 |

与 Detours/MinHook：若仅需少量 API，IAT 更接近 PLT「caller 侧」模型；inline 改写属另一路线。

**DllMain 约束：** 勿在 loader lock 下 `LoadLibrary` 非依赖 DLL；APM 可用 `TRAY_MEMPROBE_APM_DEFER=1` 推迟。

---

## 4. Backtrace

**实现文件：** `src/backtrace/backtrace_all.cpp`

| 平台 | 抓栈 | 符号化 |
|------|------|--------|
| Windows | `CaptureStackBackTrace` | DbgHelp `SymFromAddr` |
| Android / OHOS | `_Unwind_Backtrace` | `dladdr` |
| 其它 POSIX | `backtrace(3)` | `dladdr` |

约定：

- `skip`：调用方跳过本 API / proxy 入口等无关帧（memprobe 常用 `skip=2`）。
- 符号可空（剥离 so）；保留 `module + offset` 供离线 `addr2line` / `llvm-symbolizer`。
- 在 **proxy 内** 调用 `tray_hooks_collector_record`，即可把 hook 命中与堆栈一并上报。

---

## 5. Memprobe 注入与平台差异

| 平台 | 启动 | Hook 安装 | 可选双保险 |
|------|------|-----------|------------|
| Linux/Android/OHOS/QNX | `constructor(101)` | GOT（`memprobe_install`） | `alloc_interpose` 符号导出 + `LD_PRELOAD` |
| Windows | `DllMain(PROCESS_ATTACH)` | IAT | 无 LD_PRELOAD 等价物 |

脚本：

- ELF：`hooks/scripts/inject_memprobe.sh`（`patchelf --add-needed`）
- PE：`hooks/scripts/inject_memprobe.ps1` / `inject_memprobe_pe.py`

详见 [../memprobe/README.md](../memprobe/README.md)。

---

## 6. 排查速查

| 症状 | 优先检查 |
|------|----------|
| `hooked … 0 slot(s)` | 符号名是否与导入一致；是否静态链接无 PLT；caller 过滤是否过严 |
| `fails=` 非 0 | RELRO / 加固 / `mprotect` 权限 |
| CALL_PREV 死递归 | proxy 内直接调了同名符号；或 `prev` 被写成 `new_func` |
| 晚加载未拦截 | 是否 AUTOMATIC；Android 是否走到 `android_dlopen_ext` |
| 无栈符号 | 是否 strip；改用 module+offset 离线符号化 |

---

## 7. 相关文档

- [ARCHITECTURE.md](ARCHITECTURE.md) — 分层与扩展点  
- [ANDROID.md](ANDROID.md) — Android 构建 / 注入 / 验收  
- [DATAFLOW.md](DATAFLOW.md) — 事件与 APM 流  
- [ROADMAP.md](ROADMAP.md) — 演进项  
