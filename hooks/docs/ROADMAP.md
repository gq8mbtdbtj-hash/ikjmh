# 后续可做（Roadmap）

在现有 **真 hook / memprobe / APM / 过滤 / PE 注入 / 白盒测试** 之上，按价值排序：

## 高价值

| 项 | 说明 |
|----|------|
| **hook_partial 真过滤** | 按 `caller_allow` 只改指定模块 IAT/GOT，降噪与权限隔离 |
| **CI 矩阵** | Win + Linux + Android NDK 跑 `ctest`（unit + whitebox + smoke） |
| **托盘联动** | tray 主程序一键 start/stop/dump/导出 NDJSON |
| **NPU 精确 ABI** | 按厂商 runtime 补 size 级 hook（对标 GPU 精确路径） |

## 中价值

| 项 | 说明 |
|----|------|
| RELRO / 加固 GOT | 写入失败分类上报；可选 `/proc/self/mem` 等后备（需合规评估） |
| LoadLibrary 全路径 | 再 hook `LdrLoadDll` / `android_dlopen_ext` 边角 |
| POSIX HTTPS APM | 接 libcurl 或 BoringSSL，与 WinHTTP 对齐 |
| 符号缓存 | backtrace 符号化结果 LRU，降热路径开销 |
| 延迟加载 PE | 支持 `/DELAYLOAD` 导入表改写 |

## 文档与质量

| 项 | 说明 |
|----|------|
| 本仓库 Doxygen | `hooks/Doxyfile` → `hooks/docs/doxygen/html/` |
| 白盒用例扩展 | 多符号、多模块、AUTOMATIC 晚加载 DLL 场景 |
| Fuzz 过滤字符串 | 畸形 `FILTER_*` 环境变量 |
| Benchmark | hook 热路径 ns/call、采样开销曲线 |

## 明确不做 / 慎做

- 未授权注入第三方进程
- 完整替代 ASAN / 商业 APM（本库是可注入诊断底座）
- 内核态 / 驱动级 hook
