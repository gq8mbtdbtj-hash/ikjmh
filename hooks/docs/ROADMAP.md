# 后续可做（Roadmap）

## 高价值

| 项 | 状态 | 说明 |
|----|------|------|
| **hook_partial 真过滤** | ✅ | Win/ELF 按 `caller_allow` / `caller_path` 过滤 |
| **CI 矩阵** | ✅ | `.github/workflows/hooks-ci.yml`（Win + Linux ctest） |
| **托盘联动** | ✅ | `MemprobeModule`：stats / dump / start / stop / start_apm |
| **NPU 精确 ABI** | ✅ | Ascend `aclrtMalloc/Free`（+Host） |

## 中价值

| 项 | 状态 | 说明 |
|----|------|------|
| RELRO / 加固 GOT | ✅ 部分 | WriteSlot 失败计数 stderr 上报 |
| LoadLibrary 全路径 | ✅ | `LdrRegisterDllNotification` 首选；回退 LoadLibrary IAT |
| POSIX HTTPS APM | ⏳ | 现支持 http:// 与 FILE |
| 符号缓存 | ⏳ | backtrace LRU |
| 延迟加载 PE | ✅ | 仅 hook 已绑定的 Delay-Load 槽 |

## 文档与质量

| 项 | 状态 | 说明 |
|----|------|------|
| Doxygen | ✅ | `hooks/Doxyfile` + mainpage |
| 架构设计文档集 | ✅ | INDEX / ARCHITECTURE / API / DATAFLOW / PLATFORM / ANDROID / LEARNING |
| 白盒 | ✅ | hook_all + CALL_PREV + hook_partial |
| Fuzz / Benchmark | ⏳ | FILTER_* 畸形串、热路径计时 |
| Android CI 矩阵 | ⏳ | NDK 编包 + 模拟器/真机 whitebox |

## 明确不做

- 未授权注入、替代 ASAN/商业 APM、内核态 hook
