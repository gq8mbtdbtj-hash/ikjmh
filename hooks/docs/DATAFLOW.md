# 采集与观测数据流

**状态：** v1.0  
**范围：** Hook 命中之后的事件、过滤、APM、Memprobe 记账如何串联。

---

## 1. 总览

```text
                    ┌─────────────┐
  业务调用符号 ───▶ │ GOT / IAT   │───▶ Proxy
                    └─────────────┘       │
                                          ├─① tray_hooks_backtrace（可选）
                                          ├─② tray_hooks_collector_record
                                          ├─③ memprobe OnAlloc / OnFree
                                          └─④ TRAY_HOOKS_CALL_PREV → 原实现
```

Memprobe 的 proxy（`src/memprobe/memprobe_install.cpp`）约定：

- **分配：** `TRAY_HOOKS_CALL_PREV` →（可选）`tray_hooks_collector_record` → `OnAlloc`
- **释放：** `OnFree` → `TRAY_HOOKS_CALL_PREV`（先记账再释放，避免野指针）

`TRAY_MEMPROBE_COLLECT=1` 时打开 ②；域开关由 `TRAY_MEMPROBE_DOMAINS` 控制 ③。

---

## 2. Collector 路径

```text
tray_hooks_collector_record(tag, frames?, nframes, arg0)
        │
        ▼
   组装 tray_hooks_event_t
   （timestamp_ms / pid / tid / process / frames）
        │
        ▼
   filter 判定 ──否──▶ collector_dropped++
        │是
        ▼
   collector_total++
        │
        ▼
   sink(ev, user)  ──可空──▶ 仅计数
        │
        ├─ 用户 tray_hooks_collector_set_sink
        └─ APM（tray_hooks_apm_start* 注册）
```

### 2.1 过滤环境变量

| 变量 | 作用 |
|------|------|
| `TRAY_HOOKS_FILTER_TAGS` | tag 白名单（逗号分隔） |
| `TRAY_HOOKS_FILTER_DENY_TAGS` | tag 黑名单（优先） |
| `TRAY_HOOKS_FILTER_TIDS` | 线程 ID 白名单 |
| `TRAY_HOOKS_FILTER_PROCESS` | 进程名子串 |
| `TRAY_HOOKS_FILTER_MODULES` | 栈帧 module 子串（至少一帧命中） |
| `TRAY_HOOKS_FILTER_MIN_ARG0` | arg0 下限（如最小分配字节） |
| `TRAY_HOOKS_FILTER_SAMPLE` | 每 N 次放行 1 次 |

由 `tray_hooks_collector_apply_env_filter()` 装载；memprobe/APM 启动路径会调用。

### 2.2 与 Memprobe 记账的关系

| 通道 | 存储 | 用途 |
|------|------|------|
| Collector 事件 | 瞬时 → sink | 分配点堆栈、APM 流水 |
| Memprobe tracker | 按指针的 live map + 域计数 | dump Top、peak、RSS/CPU |

二者独立；同一 proxy 可同时写两条通道。

---

## 3. APM 路径

```text
sink(ev)
  │
  ▼
Event → JSON → 内存 batch 缓冲
  │
  ├─ 定时线程（TRAY_HOOKS_APM_INTERVAL_MS）
  └─ tray_hooks_apm_flush / stop
        │
        ├─ 追加写入 TRAY_HOOKS_APM_FILE（每行一个 JSON）
        └─ HTTP POST TRAY_HOOKS_APM_URL
           Content-Type: application/x-ndjson
```

附加：

- `tray_hooks_apm_emit_raw`：写入自定义行（如 `{"type":"memprobe.stats",…}`）。
- Memprobe 在配置了 FILE/URL 时启动 stats 工作线程，按同一 interval 周期 emit。

### 3.1 事件 JSON 形状（示意）

```json
{
  "ts": 1710000000000,
  "tag": "malloc",
  "arg0": 4096,
  "pid": 1234,
  "tid": 5678,
  "process": "myapp",
  "frames": [
    {"pc": "0x…", "module": "libx.so", "symbol": "foo", "offset": 123}
  ]
}
```

`TRAY_HOOKS_APM_STACKS=0` 时省略 stacks/frames 文本。

---

## 4. Memprobe 域数据流

```text
TRAY_MEMPROBE_DOMAINS=heap,mmap,gpu,…
        │
        ▼
tray_memprobe_install_hooks
  ├─ heap:  malloc/calloc/realloc/free（+ ELF 符号插桩可选）
  ├─ mmap:  mmap/munmap 或 VirtualAlloc/Free
  ├─ gpu:   CUDA/OpenCL/Vulkan 精确 ABI（+ TRAY_MEMPROBE_GPU_SYMS 占位）
  ├─ npu:   Ascend aclrtMalloc/Free 等（+ NPU_SYMS）
  └─ neon/custom: NEON_SYMS 或 domain_alloc/free
        │
        ▼
OnAlloc(dom, ptr, size)
  ├─ 域未开启 / 重入 → 忽略
  ├─ SAMPLE / MIN_SIZE → 决定是否采栈
  └─ live_blocks / live_bytes / peak / sampled_allocs
        │
        ▼
get_stats / dump / APM stats 行
```

### 4.1 采样与控制变量

| 变量 | 含义 |
|------|------|
| `TRAY_MEMPROBE_SAMPLE` | 每 N 次分配采栈（1=全采） |
| `TRAY_MEMPROBE_MIN_SIZE` | 小于该字节不采栈 |
| `TRAY_MEMPROBE_LOG` | dump 默认路径 |
| `TRAY_MEMPROBE_DISABLE=1` | 装库但不记账 |
| `TRAY_MEMPROBE_DUMP_ATEXIT=0` | 退出不自动 dump |
| `TRAY_MEMPROBE_COLLECT=1` | 分配事件进 collector |
| `TRAY_MEMPROBE_APM_DEFER=1` | Win：DllMain 内不启 APM；业务调 `tray_memprobe_start_apm` |

---

## 5. 晚加载模块（AUTOMATIC）

```text
dlopen / android_dlopen_ext / LoadLibrary / LdrRegisterDllNotification
        │
        ▼
Backend 重放活跃 Stub
        │
        ▼
再次 PatchSymbol / PatchIAT（幂等；已是 proxy 的槽可跳过）
```

因此晚加载的 CUDA/Vulkan/业务插件仍可挂上 hook（需 `TRAY_HOOKS_MODE_AUTOMATIC`；memprobe 默认已开）。

---

## 6. 失败与可观测性

| 现象 | 信号 |
|------|------|
| GOT/IAT 无法写（RELRO/加固） | stderr：`fails=` / WriteSlot 失败计数 |
| hook 0 槽 | stderr：`hooked 'sym' in 0 GOT slot(s)`；查符号名、是否静态链无 PLT |
| CALL_PREV 空 / 死递归 | `tray_hooks_get_prev` 未登记或 prev==proxy；见 `hooks_api.cpp` 防覆盖逻辑 |
| APM 无输出 | 未设 FILE/URL；或全被 filter 丢掉（查 `dropped`） |
| 采栈无符号 | 剥离 so；保留 module+offset 做离线符号化 |

---

## 7. 推荐组合（降噪 + 持续观测）

```bash
export TRAY_MEMPROBE_DOMAINS=heap,mmap
export TRAY_MEMPROBE_COLLECT=1
export TRAY_MEMPROBE_MIN_SIZE=4096
export TRAY_HOOKS_FILTER_MIN_ARG0=4096
export TRAY_HOOKS_FILTER_MODULES=myapp,libbiz
export TRAY_HOOKS_APM_FILE=/tmp/tray.ndjson
export TRAY_HOOKS_APM_STACKS=1
export TRAY_HOOKS_APM_INTERVAL_MS=2000
```

设计背景见 [ARCHITECTURE.md](ARCHITECTURE.md)；Android 注入见 [ANDROID.md](ANDROID.md)。
（过滤变量亦可用 `TRAY_HOOKS_FILTER_*`，与 `tray_hooks_collector_apply_env_filter` 一致。）
