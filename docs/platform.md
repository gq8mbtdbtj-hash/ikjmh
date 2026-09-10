# 平台矩阵

| 平台 | 托盘 / 菜单 / 面板 | 登录 UI | HTTP |
|------|-------------------|---------|------|
| **Windows** | 完整（NotifyIcon / HMENU / 原生面板 + 可选 WebView2） | `WinAuthUi` | WinHTTP（http/https） |
| **macOS** | **桩** `platform/mac` | 桩 | socket + **OpenSSL HTTPS**（若找到 libssl） |
| **Linux** | **桩** `platform/linux` | 桩 | 同上 |
| **Null** | 测试用空实现 | `NullAuthUi` | 同上（非 Win） |

## POSIX HTTP / HTTPS

- `http://`：原生 TCP + HTTP/1.0
- `https://`：OpenSSL（SNI + 系统 CA 校验）；CMake `find_package(OpenSSL)`
  - 开启：`TRAY_DEMO_USE_OPENSSL=ON`（默认）
  - 未安装 OpenSSL 时仅支持 `http://`，HTTPS 返回明确错误

依赖示例：

```bash
# Debian/Ubuntu
sudo apt install libssl-dev
# macOS (Homebrew)
brew install openssl
cmake -S . -B build -DOPENSSL_ROOT_DIR=$(brew --prefix openssl)
```

配置路径：

- Win：`%APPDATA%/tray_demo/settings.ini`
- POSIX：`$XDG_CONFIG_HOME/tray_demo/settings.ini` 或 `~/.config/tray_demo/settings.ini`

入口：`src/app/main.cpp` 按 `_WIN32` / `__APPLE__` / `__linux__` 分支。  
框架登录：`AuthModule`（`auth.login` / `auth.logout`）。
