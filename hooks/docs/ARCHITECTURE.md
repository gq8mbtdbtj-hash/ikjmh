# tray_hooks 架构设计

**状态：** 设计说明 v1.0  
**范围：** `hooks/` 目录内的跨平台 Hook 采集、Backtrace、Collector、APM、Memprobe  
**非目标：** 未授权第三方进程注入、内核态 hook、替代商业 APM / ASAN

---

## 1. 问题与目标

### 1.1 问题

在 **不改业务源码**（或仅链入轻量 SDK）的前提下，需要：

1. 拦截进程内对关键符号的调用（如 `malloc` / `mmap` / GPU 分配 API）；
2. 在拦截点采集调用栈与附加参数；
3. 按域记账内存用量，并可选持续上报（文件 / HTTP）；
4. 同一套 C ABI 覆盖 **Windows / Linux / Android / 鸿蒙 / QNX**。

### 1.2 设计目标

| ID | 目标 |
|----|------|
| A1 | **Caller 侧**改写（GOT / IAT），不改 callee 代码段（非 inline hook） |
| A2 | 稳定 **C ABI**；平台差异收敛在 Backend 实现 |
| A3 | 支持 **AUTOMATIC**：新加载模块自动补 hook |
| A4 | 采集路径可降噪（filter）并可接 APM sink |
| A5 | Memprobe 可 **无编译注入**（LD_PRELOAD / patchelf / PE add-needed / APK so） |
| A6 | Android 量产可 **替换后端为 bytehook** 而不改业务调用方 |

### 1.3 非目标

- 内核态 / SELinux 旁路 / 未授权注入
- 完整符号服务器或在线符号化平台
- 保证 FULL RELRO / 加固包 100% 可写 GOT（失败需可观测）

---

## 2. 逻辑分层

```text
┌─────────────────────────────────────────────────────────────┐
│  接入层                                                      │
│  · 业务主动调用 tray_hooks_* / tray_memprobe_*               │
│  · 无编译：LD_PRELOAD / PE 注入 / APK System.loadLibrary     │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│  门面层（稳定 C ABI）                                        │
│  hooks.h · collector.h · backtrace.h · apm.h · memprobe.h    │
└───────┬───────────────────────────────┬─────────────────────┘
        │                               │
        ▼                               ▼
┌───────────────────┐         ┌───────────────────────────────┐
│  Hook 引擎         │         │  采集与观测                    │
│  hooks_api.cpp     │         │  collector → apm_sink         │
│  Backend 抽象      │         │  backtrace_all                │
│  Stub / proxy→prev │         │  memprobe tracker / install   │
└─────────┬─────────┘         └───────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────┐
│  平台后端（互斥编译）                                         │
│  Win:  win_iat_backend + win_iat_patch                       │
│  ELF:  elf_plt_backend + elf_plt_patch                       │
│  （可选量产）CreateBackend() → bytehook 适配器                │
└─────────────────────────────────────────────────────────────┘
```

**依赖方向：** 接入层 → 门面 → 引擎 / 采集 → 平台后端。  
平台后端 **不得** 依赖 memprobe；memprobe **依赖** tray_hooks。

---

## 3. 核心概念

### 3.1 Caller 侧 Hook（与 bytehook / xhook 对齐）

业务 so/dll `libA` 调用 `malloc` 时，实际读取的是 **libA 自己的** PLT GOT / IAT 槽，而不是 `libc` 的代码。

```text
  libA.so                    libc.so
  ┌──────────┐               ┌──────────┐
  │ call *GOT│──────────────▶│ malloc   │
  │ GOT[slot]│  ←── 改写为 proxy
  └──────────┘
       │
       ▼
   ProxyMalloc → tray_hooks_get_prev → 原 malloc
                 + collector / OnAlloc
```

| 平台 | 改写对象 | 枚举模块 | 晚加载 |
|------|----------|----------|--------|
| Windows | PE IAT（含部分 Delay-Load 已绑定槽） | Toolhelp / PEB | `LdrRegisterDllNotification`（首选）或 LoadLibrary* |
| ELF 系 | `.rela.plt` / `.rel.plt` 的 JUMP_SLOT | `dl_iterate_phdr` | hook `dlopen` / `android_dlopen_ext` |

**不做：** 改写 callee 指令（inline / shadowhook 范畴）。

### 3.2 Stub 与 CALL_PREV

一次 `hook_*` 产生一个运行时 `Stub`：

| 字段 | 含义 |
|------|------|
| `sym_name` | 目标符号 |
| `new_func` | 代理（proxy） |
| `prev_func` | 改写前原地址 |
| `scope` | 0=single / 1=partial / 2=all |
| `caller_path` / `caller_allow` | 调用者过滤 |

门面维护 `proxy → prev` 映射；proxy 内必须通过：

```c
TRAY_HOOKS_CALL_PREV(ProxyMalloc, malloc_fn, n);
/* 或 */ tray_hooks_get_prev((void*)ProxyMalloc);
```

直接再调 `malloc` 会 **死递归**。

### 3.3 MANUAL vs AUTOMATIC

| 模式 | 行为 |
|------|------|
| `TRAY_HOOKS_MODE_MANUAL` | 仅改写 **当前已映射** 模块 |
| `TRAY_HOOKS_MODE_AUTOMATIC` | 额外 hook 加载器；新 so/dll 映射后 **重放** 活跃 stub |

Memprobe 默认 `AUTOMATIC`，以覆盖晚加载的 CUDA/Vulkan/业务插件。

---

## 4. 模块职责

| 模块 | 路径 | 职责 | 不负责 |
|------|------|------|--------|
| **Hook 门面** | `hooks_api.cpp` + `hooks.h` | 幂等 init、Stub 安装、proxy→prev、线程锁 | PE/ELF 解析 |
| **Backend** | `internal/backend.hpp` | 虚接口；`CreateBackend()` 工厂 | 对外 ABI |
| **ELF 后端** | `plat/elf_plt_*` | GOT 改写、dlopen 重放 | IAT |
| **Win 后端** | `plat/win_iat_*` | IAT 改写、DLL 通知 | GOT |
| **Backtrace** | `backtrace_all.cpp` | 抓栈 + 粗符号化 | 离线 addr2line 服务 |
| **Collector** | `collector.cpp` | 事件组装、filter、调 sink | 持久化格式 |
| **APM** | `apm_sink.cpp` | NDJSON 落盘 / HTTP 批报 | 服务端存储 |
| **Memprobe** | `src/memprobe/*` | 注入启动、域记账、heap/mmap/GPU/NPU hook | 通用任意符号 DSL |

---

## 5. 目录与构建拓扑

```text
hooks/
  include/tray_hooks/     # 对外头文件（C ABI）
  src/
    hooks_api.cpp         # 门面
    collector.cpp
    apm_sink.cpp
    backtrace/
    internal/backend.hpp  # 仅内部
    plat/                 # 平台互斥源文件
    memprobe/             # 共享库 tray_memprobe
  examples/               # sample / whitebox / smoke
  tests/                  # 单元 + victim SO/DLL
  docs/                   # 本设计文档集
  memprobe/README.md      # 注入用法
```

CMake：

- `tray_hooks`：**静态库**（PIC），含平台后端源文件之一。
- `tray_memprobe`：**共享库**，链 `tray_hooks`；ELF 另编 `alloc_interpose.cpp`（符号插桩双保险）。

根工程开关：`TRAY_DEMO_BUILD_HOOKS`；子选项见 `hooks/CMakeLists.txt`。

---

## 6. 关键生命周期

```text
进程启动
   │
   ├─【链入 SDK】业务调 tray_hooks_init / hook_*
   │
   └─【注入 memprobe】
         constructor(101) / DllMain(ATTACH)
              ├─ tray_memprobe_start()          # 读 DOMAINS 等
              ├─ tray_memprobe_install_hooks()  # init AUTOMATIC + hook 符号
              └─ StartObservability()           # filter + APM（可 DEFER）
                    │
                    ▼
              业务运行：proxy → OnAlloc/record → sink
                    │
              退出：dump / apm_flush / uninit / Restore 槽位
```

**幂等：** `tray_hooks_init` 可重复调用；已 init 直接返回 OK。  
**恢复：** `unhook` / `uninit` 将 GOT/IAT 写回 `original`。

---

## 7. 扩展点（CUSTOMIZE）

| 扩展 | 做法 | ABI 影响 |
|------|------|----------|
| Android 量产接 bytehook | 实现 `Backend` 适配器，改 `CreateBackend()` | 无 |
| 新平台 | 新 `*_backend.cpp` + patch，条件编译进 `tray_hooks` | 无 |
| 新内存域 | `tray_mem_domain_t` + install 符号表；或 `domain_alloc/free` | 可能增枚举 |
| 自定义 sink | `tray_hooks_collector_set_sink` | 无 |
| 过滤策略 | `set_filter` / `apply_env_filter` | 无 |

**禁止** 在门面层散落 `#ifdef __ANDROID__` 业务逻辑；平台细节留在 `plat/`。

---

## 8. 线程与重入

| 组件 | 约定 |
|------|------|
| `hooks_api` | 全局 `mutex` 保护 backend 与 proxy 表 |
| Proxy | 热路径；`get_prev` 持锁时间应短；勿在 lock 内调业务重逻辑 |
| Collector sink | **勿长时间持锁**；可异步投递 |
| Memprobe OnAlloc | TLS/标志做 **重入保护**（采栈可能再次分配） |
| Win DllMain | 避免再 `LoadLibrary` 非依赖模块；APM 可 `TRAY_MEMPROBE_APM_DEFER=1` |

---

## 9. 质量与验证

| 资产 | 验证点 |
|------|--------|
| `hooks_unit_tests` | collector 过滤 / API 冒烟 |
| `hooks_apm_tests` | APM 文件写出 |
| `hook_whitebox` | 真 IAT/PLT、CALL_PREV、partial、晚加载 |
| `memprobe_smoke` | 分配统计 |
| CI | `.github/workflows/hooks-ci.yml`（Linux + Windows） |

Android 真机矩阵见 [ANDROID.md](ANDROID.md)。

---

## 10. 安全与合规边界

- 仅用于 **自有进程** 诊断 / APM / 崩溃分析。
- 不提供绕过加固、提权、跨进程未授权注入的指南或实现。
- APM HTTP 默认可用于内网采集；生产应叠加鉴权与 TLS（POSIX HTTPS 见 ROADMAP）。

---

## 11. 相关文档

- [API.md](API.md) — ABI 与调用契约  
- [DATAFLOW.md](DATAFLOW.md) — 事件与记账数据流  
- [PLATFORM.md](PLATFORM.md) — 平台实现步骤  
- [ANDROID.md](ANDROID.md) — Android 落地  
- [ROADMAP.md](ROADMAP.md) — 演进与明确不做  
