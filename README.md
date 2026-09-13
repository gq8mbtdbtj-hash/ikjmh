# tray_demo

C++11 托盘 Demo + Go（LDAP）配置/更新简易服务。

## 客户端能力

- 系统托盘；左键面板（状态区 + **模糊搜索列表** + **分类下拉**）；右键菜单
- 可选 **面板全屏**；可选 **WebView2 远程站点面板**（与原生可切换）
- **登录…** 弹窗（账号走服务端 LDAP / mock）
- 本地配置 `%APPDATA%/tray_demo/settings.ini`（默认 `http_base_url`）
- 单实例；拉取配置 JSON

## WebView2 面板（可选）

默认 `panel_ui=native`。切换到远程前端：

1. 编辑 `%APPDATA%/tray_demo/settings.ini`：
   - `panel_ui=webview`
   - `panel_webview_url=https://your-frontend.example/`
   - `panel_webview_token_key=tray_demo_token`
2. 或右键菜单 **面板：切换到 WebView**
3. 目标机需安装 [WebView2 Evergreen Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)

登录后托盘会把 Bearer 写入页面 `localStorage[token_key]`，并 `chrome.webview.postMessage({type:'auth',...})`。无 Runtime 时提示并回退 native。

## 一键体验

### 1. 启动 Go 服务

```bat
cd server
go run .
```

默认 mock：`demo`/`demo`（viewer）；管理后台 `admin`/`admin` → http://127.0.0.1:8080/admin/  
鉴权为 JWT + `acl.yaml` 路由权限（见 [server/README.md](server/README.md)）。

### 2. 启动托盘

```bat
scripts\build_and_run.bat
```

或 `build\tray_demo.exe`

右键 → **登录…**（AuthModule）→ demo/demo → **拉取配置 JSON** → 左键看面板。

## 架构（业务解耦）

- **框架** `tray_demo_core`：托盘 / 菜单 / 会话 / **AuthModule** / 面板宿主 / `IAppModule` / `IListPanelModel`
- **平台**：Win 完整；**mac / Linux 桩**；其它用 Null。见 [docs/platform.md](docs/platform.md)
- **HTTP**：Win=WinHTTP；POSIX=socket，HTTPS 需 OpenSSL（见 [docs/platform.md](docs/platform.md)）
- **业务** `tray_demo_module_demo` / `about`（无登录菜单；登录属 AuthModule）
- 新产品：实现 `IAppModule`，`main` 里 `RegisterModule`（建议先注册 `AuthModule`）
- **Go**：`framework.Module` + 可选 `ACLProvider` / `Deps.Admin.RegisterNav`
- **验收**：关掉 demo/about 仍可编出 Auth+退出壳；`internal/framework` 不依赖 `modules`

## 配置

| 项 | 位置 |
|----|------|
| 默认 HTTP 地址 | `AppSettings::DefaultSettings()` / `settings.ini` |
| 面板模式 | `panel_ui=native\|webview` |
| WebView URL / token 键 | `panel_webview_url` / `panel_webview_token_key` |
| 业务菜单与动作 | `modules/demo` → `DemoModule` |
| 登录窗 UI | `login_dialog.cpp` |
| 原生 / WebView 面板宿主 | `WinPanelRenderer` / `WinWebViewPanel` |

IDE 搜索 **`CUSTOMIZE:`**。

## Go 服务 API

见 [server/README.md](server/README.md)。

| 方法 | 路径 | 鉴权 |
|------|------|------|
| POST | `/api/v1/login` | LDAP/mock |
| GET/PUT | `/api/v1/config.json` | Bearer |
| GET | `/api/v1/update/manifest` | Bearer |
| GET | `/api/v1/files/{name}` | Bearer |

## CLion

`CMakePresets.json` + 根目录 `compile_commands.json`（构建后自动同步）。

## Doxygen

```bat
doxygen Doxyfile
```

## tray_hooks（跨平台 Hook / Backtrace / Memprobe）

见 [hooks/README.md](hooks/README.md)、[hooks/memprobe/README.md](hooks/memprobe/README.md)。

```bat
cmake -S . -B build -DTRAY_DEMO_BUILD_HOOKS=ON
cmake --build build --target tray_hooks tray_hooks_sample
```

Linux 内存探针：

```bash
cmake --build build --target tray_memprobe
LD_PRELOAD=./build/hooks/libtray_memprobe.so ./build/hooks/memprobe_smoke
./hooks/scripts/inject_memprobe.sh ./app   # patchelf --add-needed
```

## 小票账本（微信小程序）

独立目录 [`miniprogram/`](miniprogram/)。用微信开发者工具打开该目录即可预览：拍小票入账、手动记账、明细与月统计。详见 [miniprogram/README.md](miniprogram/README.md)。
