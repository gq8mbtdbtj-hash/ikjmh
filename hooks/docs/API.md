# tray_hooks API 设计契约

**状态：** v1.0  
**语言：** C（头文件可被 C++ `extern "C"` 包含）  
**稳定性：** 对外符号以 `tray_hooks_*` / `tray_memprobe_*` 为准；`src/internal` 与 `src/plat` **不对外**。

---

## 1. 头文件一览

| 头文件 | Doxygen 分组 | 说明 |
|--------|--------------|------|
| `tray_hooks/hooks.h` | `tray_hooks_api` | init / hook_* / get_prev / unhook |
| `tray_hooks/backtrace.h` | `tray_hooks_collect` | 抓栈与格式化 |
| `tray_hooks/collector.h` | `tray_hooks_collect` | 事件 + filter + sink |
| `tray_hooks/apm.h` | `tray_hooks_collect` | NDJSON 落盘 / HTTP 批报 |
| `tray_hooks/memprobe.h` | `tray_memprobe_api` | 域统计 / dump / domain_alloc |
| `tray_hooks/memprobe_install.h` | `tray_memprobe_api` | 安装真 hook（通常自动） |

链接：业务链静态库 `tray_hooks`；注入场景加载共享库 `tray_memprobe`（已私有链接 hooks）。

---

## 2. 标准调用顺序

### 2.1 进程内 SDK

```c
#include "tray_hooks/hooks.h"
#include "tray_hooks/collector.h"
#include "tray_hooks/apm.h"

tray_hooks_init(TRAY_HOOKS_MODE_AUTOMATIC);
tray_hooks_collector_set_sink(my_sink, NULL);
/* 或降噪 */ tray_hooks_collector_apply_env_filter();
/* 或 APM */ tray_hooks_apm_start_from_env();

tray_hooks_stub_t* stub =
    tray_hooks_hook_all(NULL, "malloc", (void*)ProxyMalloc, NULL, NULL);

/* … 运行 … */

tray_hooks_unhook(stub);   /* 可选 */
tray_hooks_uninit();
```

### 2.2 Proxy 内

```c
static void* ProxyMalloc(size_t n) {
  tray_hooks_frame_t frames[32];
  int nf = tray_hooks_backtrace(frames, 32, 1);
  tray_hooks_collector_record("malloc", frames, nf, (uint64_t)n);

  typedef void* (*malloc_fn)(size_t);
  return TRAY_HOOKS_CALL_PREV(ProxyMalloc, malloc_fn, n);
}
```

**硬性规则：**

1. 必须先 `tray_hooks_init` 再 `hook_*`；未 init 时 `hook_*` 返回 `NULL`。
2. Proxy 内 **禁止** 直接调用同名 libc API（会死递归）；只用 `tray_hooks_get_prev` / `TRAY_HOOKS_CALL_PREV`。
3. `get_prev` 的 key 必须是 hook 时传入的 **同一个** `new_func` 地址。
4. `uninit` 之后不得再依赖已恢复的槽位逻辑。

### 2.3 Memprobe（自动）

加载 `libtray_memprobe.so` / `tray_memprobe.dll` 后，`constructor(101)` / `DllMain` 自动：

1. `tray_memprobe_start()`
2. `tray_memprobe_install_hooks()`
3. 若配置了 `TRAY_HOOKS_APM_FILE` / `URL` → filter + APM（Win 可用 `TRAY_MEMPROBE_APM_DEFER=1`，再调 `tray_memprobe_start_apm()`）

业务一般只需读 `tray_memprobe_get_stats` / `dump` / `domain_*`。

---

## 3. Hook API 细节

### 3.1 模式 `tray_hooks_mode_t`

| 枚举 | 语义 |
|------|------|
| `TRAY_HOOKS_MODE_AUTOMATIC` | 新模块加载后重放活跃 stub |
| `TRAY_HOOKS_MODE_MANUAL` | 仅改写当前已映射模块 |

`tray_hooks_init` **幂等**：已初始化则直接返回 `TRAY_HOOKS_OK`（不切换模式）。换模式须先 `uninit`。

### 3.2 状态码 `tray_hooks_status_t`

| 码 | 含义 |
|----|------|
| `TRAY_HOOKS_OK` | 成功 |
| `TRAY_HOOKS_ERR_INIT` | 初始化失败 |
| `TRAY_HOOKS_ERR_NOT_IMPL` | 功能未实现 |
| `TRAY_HOOKS_ERR_PARAM` | 参数非法 |
| `TRAY_HOOKS_ERR_PLATFORM` | 无后端 / 平台失败 |
| `TRAY_HOOKS_ERR_EXISTS` | 冲突（预留） |

`hook_*` 失败返回 **NULL**；成功时可选 `tray_hooks_hooked_t` 回调以 `TRAY_HOOKS_OK` 触发。

### 3.3 `hook_single` / `partial` / `all`

| API | 过滤 |
|-----|------|
| `tray_hooks_hook_single` | `caller_path` 子串；NULL=不限（视后端） |
| `tray_hooks_hook_partial` | `caller_allow(path, arg)` 非 0 才改写；**不可为 NULL** |
| `tray_hooks_hook_all` | 所有已映射调用者对该符号的导入 |

`callee_path`：

- **Windows：** 过滤导入自哪一个 DLL；NULL=任意（多数场景推荐）。
- **ELF：** 主要用于 `resolve_sym`；GOT 改写按符号名匹配 JUMP_SLOT。

### 3.4 后端名

`tray_hooks_backend_name()` 示例：

- `win_iat` / `win_iat(auto)`
- `elf_plt(linux)` / `elf_plt(android,auto)` / `elf_plt(ohos)` / `elf_plt(qnx,auto)`
- 未 init：`none`

### 3.5 导出宏

| 宏 | 说明 |
|----|------|
| `TRAY_HOOKS_API` | Win：`TRAY_HOOKS_EXPORTS` 时 dllexport；POSIX：default visibility |
| `TRAY_MEMPROBE_API` | memprobe 共享库导出（`TRAY_MEMPROBE_EXPORTS`） |

---

## 4. Collector / Filter / APM

### 4.1 事件 `tray_hooks_event_t`

字段：`timestamp_ms`、`tag`、`arg0`、`pid`、`tid`、`process`、`nframes`、`frames[64]`。

`tray_hooks_collector_record`：

- `frames == NULL` → 内部自动 `tray_hooks_backtrace`；
- 未过 filter → 不调 sink，计入 `tray_hooks_collector_dropped`；
- 过 filter → 调 sink；计入 `tray_hooks_collector_total`（无 sink 也计 total）。

### 4.2 过滤

结构体 `tray_hooks_filter_t` 或环境变量（`tray_hooks_collector_apply_env_filter`）：

| 环境变量 | 字段 |
|----------|------|
| `TRAY_HOOKS_FILTER_TAGS` | `allow_tags` |
| `TRAY_HOOKS_FILTER_DENY_TAGS` | `deny_tags`（优先于白名单） |
| `TRAY_HOOKS_FILTER_TIDS` | `allow_tids` |
| `TRAY_HOOKS_FILTER_PROCESS` | `allow_process` |
| `TRAY_HOOKS_FILTER_MODULES` | `allow_modules`（栈帧 module 子串） |
| `TRAY_HOOKS_FILTER_MIN_ARG0` | `min_arg0` |
| `TRAY_HOOKS_FILTER_SAMPLE` | `sample_n` |

### 4.3 APM `tray_hooks_apm_*`

配置结构：`tray_hooks_apm_config_t`。

| 环境变量 | 含义 | 默认 |
|----------|------|------|
| `TRAY_HOOKS_APM_FILE` | NDJSON 追加路径 | 无则不启文件 |
| `TRAY_HOOKS_APM_URL` | HTTP POST 批次（`application/x-ndjson`） | 无 |
| `TRAY_HOOKS_APM_INTERVAL_MS` | 刷新间隔 | 2000 |
| `TRAY_HOOKS_APM_BATCH` | 每批最大事件数 | 64 |
| `TRAY_HOOKS_APM_STACKS` | `0`=不上报 frames | 开 |

`tray_hooks_apm_start` / `tray_hooks_apm_start_from_env` 会 **注册为 collector sink**（覆盖先前 `set_sink`）。  
`tray_hooks_apm_emit_raw`：追加自定义 JSON 行（如 memprobe 周期统计）。

POSIX URL 当前以 **HTTP** 为主；HTTPS 见 [ROADMAP.md](ROADMAP.md)。

---

## 5. Memprobe API

| API | 说明 |
|-----|------|
| `tray_memprobe_start` / `stop` | 读环境变量、开关记账 |
| `tray_memprobe_install_hooks` | `tray_hooks_init(AUTOMATIC)` + 安装域符号 |
| `tray_memprobe_get_stats` | 刷新 CPU/RSS 后拷贝快照 |
| `tray_memprobe_dump` | Top live + 域汇总 |
| `tray_memprobe_domain_alloc` / `free` | SDK 包装层主动上报 |
| `tray_memprobe_sample_cpu` | 刷新 CPU 采样 |
| `tray_memprobe_start_apm` | DllMain 外延迟启动 APM |
| `tray_memprobe_ping` | PE add-needed 占位导出 |

域 `tray_mem_domain_t`：`TRAY_MEM_HEAP` / `MMAP` / `GPU` / `NPU` / `NEON` / `CUSTOM`。

主要环境变量见 [../memprobe/README.md](../memprobe/README.md)。

---

## 6. 线程安全摘要

| API | 线程安全 |
|-----|----------|
| `init` / `uninit` / `hook_*` / `get_prev` | 是（内部锁） |
| `collector_record` / `set_sink` / `set_filter` | 是 |
| `apm_*` | 是（缓冲锁 + 工作线程） |
| `memprobe_get_stats` / `domain_*` | 是（实现侧同步） |
| 用户 `sink` 回调 | 调用方保证；勿在回调里 `uninit` |

热路径：proxy 内少分配、少日志；采栈用合理 `skip`。

---

## 7. 版本与兼容

- C++ 标准：**C++11**（与仓库一致）。
- ABI：以头文件结构体布局为准；新增字段应追加 API，避免破坏现有二进制。
- `tray_memprobe_stats_t` 顶层 heap 兼容字段等于 `domains[TRAY_MEM_HEAP]`。

---

## 8. 示例与测试入口

| 目标 / 文件 | 演示 |
|-------------|------|
| `examples/sample_collect.cpp` | 基础 hook + collector |
| `examples/hook_whitebox.cpp` | 真 hook / `TRAY_HOOKS_CALL_PREV` / partial / 晚加载 |
| `examples/memprobe_smoke.cpp` | memprobe 统计冒烟 |
| `tests/test_collector_unit.cpp` | 过滤 |
| `tests/test_apm_unit.cpp` | APM 文件 |

完整架构见 [ARCHITECTURE.md](ARCHITECTURE.md)；数据流见 [DATAFLOW.md](DATAFLOW.md)。
