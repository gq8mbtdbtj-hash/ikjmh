# tray_hooks 文档索引

本目录是 **架构 / 设计 / 平台落地** 文档的入口。实现注释以 Doxygen 为主（`hooks/Doxyfile`）。

| 文档 | 读者 | 内容 |
|------|------|------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | 架构 / 接入方 | 分层、模块边界、扩展点、生命周期 |
| [API.md](API.md) | SDK 使用方 | C ABI 契约、调用顺序、错误码、线程模型 |
| [DATAFLOW.md](DATAFLOW.md) | 观测 / APM | Hook 命中 → 过滤 → sink / APM / memprobe 记账 |
| [PLATFORM.md](PLATFORM.md) | 平台实现 | ELF GOT / Win IAT / Backtrace 步骤备忘 |
| [ANDROID.md](ANDROID.md) | Android 落地 | NDK 构建、注入、量产建议、风险矩阵 |
| [ROADMAP.md](ROADMAP.md) | 维护者 | 已完成项与明确不做范围 |
| [mainpage.dox](mainpage.dox) | Doxygen 首页 | 模块总览（生成 HTML 后可见） |

配套：

| 路径 | 说明 |
|------|------|
| [../README.md](../README.md) | 快速开始、构建、测试 |
| [../memprobe/README.md](../memprobe/README.md) | 无编译注入探针用法与环境变量 |

生成 API 文档：

```bash
cd hooks && doxygen Doxyfile
# → hooks/docs/doxygen/html/index.html
```
