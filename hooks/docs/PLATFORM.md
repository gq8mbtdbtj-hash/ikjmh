# tray_hooks 平台实现备忘

## ELF 系（Android / Linux / QNX / 鸿蒙）

### 原理（同 xhook / ByteHook）

1. **Caller GOT**：`libA.so` 调用 `malloc` 时，实际读的是 `libA` 的 GOT 槽。  
2. Hook = 把该槽改为 `proxy`；原地址存入 `prev`。  
3. **不要**改 `libc` 代码段（那是 inline hook / shadowhook 范畴）。

### 关键步骤

| 步骤 | API / 结构 |
|------|------------|
| 枚举 so | `dl_iterate_phdr`（Android 5+）；老版本读 `/proc/self/maps` |
| 找 rel | `DT_JMPREL` + `DT_PLTRELSZ` / `DT_PLTRELS` |
| 符号 | `DT_SYMTAB` / `DT_STRTAB` / `DT_HASH` 或 `DT_GNU_HASH` |
| 写 GOT | `mprotect(PROT_READ\|PROT_WRITE)` + 指针赋值 + 可选 `clear_cache` |
| 新 so | hook `android_dlopen_ext` / `dlopen`（AUTOMATIC） |

### 鸿蒙

- 链接器与 namespace 与 Android 接近，优先验证 `dl_iterate_phdr` 行为。  
- 可先用 **bytehook** 源码移植评估，再替换本目录 `ElfPltBackend::hook`。

### QNX

- ELF + `dlopen`/`dlsym` 可用；确认 `dl_iterate_phdr` 或等价枚举。  
- 权限：对 GOT 页 `mprotect` 可能受 procmgr 策略限制。

### Android 量产建议

```cpp
// plat/elf_plt_backend.cpp 内
#include "bytehook.h"
// tray_hooks_hook_all → bytehook_hook_all
// tray_hooks_get_prev → BYTEHOOK_CALL_PREV 语义
```

---

## Windows（IAT）

| 步骤 | 说明 |
|------|------|
| 枚举模块 | `CreateToolhelp32Snapshot` / PEB `InLoadOrderModuleList` |
| 找导入 | `IMAGE_DIRECTORY_ENTRY_IMPORT` → `IMAGE_THUNK_DATA` |
| 写 IAT | `VirtualProtect` + 写 `proxy` |
| 新 DLL | hook `LoadLibraryW` / `LdrLoadDll`（AUTOMATIC） |

与 Detours/MinHook：若仅需少量 API，IAT 更接近 PLT「caller 侧」模型；inline 改写属于另一路线。

---

## Backtrace

| 平台 | 实现 |
|------|------|
| Windows | `CaptureStackBackTrace` + DbgHelp |
| Linux/Android/OHOS/QNX | `backtrace` + `dladdr`；可升级 libunwind |

在 **proxy 内**调用 `tray_hooks_collector_record`，即可把 hook 命中与堆栈一并上报。
