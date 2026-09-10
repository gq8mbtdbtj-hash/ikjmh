# 产品规格 — 跨平台托盘版本中心

**状态：** 草案 v0.4  
**日期：** 2026-09-08  
**代号：** `tray_demo`  
**参考交互：** JetBrains Toolbox（托盘常驻、左键面板、版本列表、自更新）  
**语言：** C++11  
**线程：** POSIX pthread API（Windows 使用 pthreads-w32 / pthreads4w）

---

## 1. 问题

需要一款体积小、始终可用的桌面工具，满足：

- 驻留系统托盘 / 菜单栏（无常驻主窗口）。
- 根据远程 JSON 配置展示工具 / 产品版本。
- 通过紧凑面板与右键菜单完成刷新、打开条目、退出等操作。
- **支持自身安全更新**（签名 / 哈希校验的发布通道）。
- 包体小、冷启动快、JSON 解析快。

## 2. 目标

| ID | 目标 |
|----|------|
| G1 | 仅托盘应用：无常驻主窗口 |
| G2 | 右键 → 原生上下文菜单 |
| G3 | 左键 → 托盘旁浮动面板（类 Toolbox） |
| G4 | 优先 Windows / macOS；Linux 延后 |
| G5 | 优先 **C++11**；仅在明显更优时使用 Rust |
| G5a | **线程模型统一为 POSIX pthread**；Windows 用 **pthreads-w32**（pthreads4w）复用同一套接口 |
| G6 | 包体积小；快速读 JSON；HTTP 拉取配置 JSON |
| G7 | 面板根据 JSON 渲染版本条目 |
| G8 | 托盘应用**自更新**（检查 / 下载 / 校验 / 应用 / 重启） |

## 3. 非目标（v1）

- 面向面板*目录工具*的完整安装 / 下载流水线（条目仅保留 URL / 路径启动）。
- Electron / CEF / 内嵌 Chromium UI（Windows **可选**系统 WebView2 加载远程站，不打包浏览器）。
- 账号登录 / SSO（可后续再做）。
- 插件市场。
- 增量 / 差分更新（v1 可用整包替换；差分为 P2）。
- v1 不支持 Linux。

## 4. 角色与主流程

**角色：** 希望快速查看可用或已安装工具版本的开发 / 运维用户。

### F1 — 冷启动

1. 可选「登录时启动」，或用户手动启动一次。
2. 出现托盘图标；无窗口。
3. 若有上次成功缓存则立即加载；可选后台拉取远程配置。

### F2 — 左键面板

1. 用户左键点击托盘图标。
2. 在托盘 / 光标附近打开非抢焦点浮动面板。
3. 面板展示：分组 → 条目（名称、当前版本、可用版本、状态）。
4. 点击条目 → 打开 URL 或启动配置路径。
5. 点击外部 / Esc / 再次点击托盘 → 关闭面板。

### F3 — 右键菜单

最少包含：

- 刷新
- 检查更新… / 立即更新（有新版本时）
- 打开面板
- 打开配置目录
- 关于（本托盘应用版本）
- 退出

### F4 — 配置刷新

1. 从配置的 HTTPS 端点拉取 JSON（离线可用 file:// 或本地路径）。
2. 校验 schema；失败则保留缓存并在面板显示错误。
3. 持久化缓存；在可用时保存 ETag / Last-Modified。

### F5 — 自更新（本托盘应用）

1. 客户端定期（及手动「检查更新」时）获取**更新清单**（可嵌在配置 JSON 的 `app.update`，或独立 URL）。
2. 用 semver 比较 `latest_version` 与运行中的 `CLIENT_VERSION`。
3. 若有新版本：面板横幅 + 菜单「更新到 x.y.z」。
4. 用户确认（或设置 `auto_update=true` 且策略允许时静默）。
5. HTTPS 下载 → 校验 **SHA-256**（必须）及已配置时的**签名** → 暂存到临时区。
6. 通过平台更新助手应用 → 退出 → 替换二进制 → 重新启动。
7. 失败：保留旧安装；显示错误；不得留下半应用状态。

## 5. 功能需求

| ID | 需求 | 优先级 |
|----|------|--------|
| FR1 | 单一托盘 / 状态栏图标 | P0 |
| FR2 | 原生右键菜单 | P0 |
| FR3 | 左键切换浮动面板 | P0 |
| FR4 | 面板列出 JSON 条目 | P0 |
| FR5 | 本地磁盘 JSON 缓存 | P0 |
| FR6 | HTTPS 拉取远程 JSON | P0 |
| FR7 | 可配置端点与轮询间隔 | P0 |
| FR8 | 按条目启动本地路径或打开 URL | P1 |
| FR9 | 登录时启动 | P1 |
| FR10 | 深 / 浅色跟随系统 | P1 |
| FR11 | 检查更新清单；展示「有更新」UI | P0 |
| FR12 | 下载 + 哈希校验 + 应用自更新 + 重启（Win/macOS） | P0 |
| FR13 | 可选自动更新设置（稳定前默认关闭） | P1 |
| FR14 | 应用失败时回滚 / 保留上一可用版本 | P1 |
| FR15 | Linux 托盘 + 面板 | P2 |
| FR16 | 差分自更新 | P2 |
| FR17 | 后台工作使用 **pthread** API；Windows 链接 pthreads-w32 | P0 |

## 6. UX 规格（面板）

参考 JetBrains Toolbox，简化版：

```
┌─────────────────────────────────┐
│  Version Hub            ↻  ⋯   │
│  有可用更新: 0.2.0       [↑]   │  ← 有新版本时的自更新横幅
├─────────────────────────────────┤
│  分组: Toolchains               │
│    ● Rust        1.80 → 1.81    │
│    ● Node        22.5  (最新)   │
│  分组: IDEs                     │
│    ○ CLion       未安装         │
├─────────────────────────────────┤
│  上次同步: 2 分钟前 · OK · v0.1.0│
└─────────────────────────────────┘
```

约束：

- 面板关闭时无任务栏 / Dock 应用图标（Windows：仅托盘；macOS：LSUIElement / accessory）。
- 面板宽约 320–420px；高度随内容，可滚动。
- 键盘：Esc 关闭；↑↓ 选择；Enter 激活（P1）。
- 自更新进度可用面板内细进度条或非模态状态；避免第二个主窗口。

## 7. 数据模型（配置 JSON）

示意 schema（规范性细节见 RFC）：

```json
{
  "schema_version": 1,
  "generated_at": "2026-09-08T12:00:00Z",
  "app": {
    "min_client_version": "0.1.0",
    "message": "可选横幅文案",
    "update": {
      "latest_version": "0.2.0",
      "channel": "stable",
      "notes_url": "https://example.com/releases/0.2.0",
      "mandatory": false,
      "packages": [
        {
          "os": "windows",
          "arch": "x86_64",
          "url": "https://example.com/releases/versionhub-0.2.0-win-x64.zip",
          "sha256": "…",
          "size_bytes": 4500000,
          "format": "zip"
        },
        {
          "os": "macos",
          "arch": "universal",
          "url": "https://example.com/releases/VersionHub-0.2.0.dmg",
          "sha256": "…",
          "size_bytes": 5200000,
          "format": "dmg"
        }
      ]
    }
  },
  "groups": [
    {
      "id": "toolchains",
      "title": "Toolchains",
      "items": [
        {
          "id": "rust",
          "name": "Rust",
          "installed_version": "1.80.0",
          "available_version": "1.81.0",
          "status": "update_available",
          "action": { "type": "url", "value": "https://example.com/rust" }
        }
      ]
    }
  ]
}
```

说明：

- 服务端可省略 `installed_version`；客户端可与本地覆盖文件合并。
- 客户端必须容忍未知字段（向前兼容）。
- `app.update` **仅驱动本应用自更新**；面板 `groups` 是目录条目，不是本二进制的安装器。
- 若 `mandatory: true` 且客户端低于 `latest_version`（或低于 `min_client_version`）：显示阻塞式横幅；仍允许退出。

## 8. 非功能需求

| ID | 需求 | 指标 |
|----|------|------|
| NFR1 | 安装包 / zip 体积（Win x64，strip 后） | 理想 ≤ 8 MB；硬顶 15 MB |
| NFR2 | 空闲 RSS | ≤ 40 MB |
| NFR3 | 冷启动到托盘就绪 | 典型 SSD ≤ 500 ms |
| NFR4 | 解析 256 KB JSON | ≤ 10 ms（RapidJSON；C++11） |
| NFR5 | 配置拉取超时 | 5–10 s，不阻塞 UI |
| NFR6 | TLS | 系统信任库；远程仅 HTTPS |
| NFR7 | 离线 | 缓存可完整展示 UI；跳过自更新 |
| NFR8 | 更新包校验 | 应用前 SHA-256 必须匹配 |
| NFR9 | 更新应用原子性 | 失败后上一版本仍可运行 |
| NFR10 | 更新下载 | 后台；可取消；不冻结 UI |
| NFR11 | 线程 API | 可移植代码仅用 `pthread_*`（`core/` 内不用 `std::thread` / 裸 `CreateThread`） |

## 9. 平台

| 平台 | 托盘 | 面板 | 自更新应用方式 | 优先级 |
|------|------|------|----------------|--------|
| Windows 10/11 | `Shell_NotifyIcon` | 托盘旁工具窗口 | 暂存 zip + 助手替换 + 重启 | P0 |
| macOS 12+ | `NSStatusItem` | 非激活 `NSPanel` | 替换 `.app`（或 pkg）+ 重启；公证产物 | P0 |
| Linux | StatusNotifier / AppIndicator | 延后 | 延后 | P2 |

## 10. 成功标准（v1）

- Win + macOS 二进制体积在上限内。
- 两个 P0 系统上左 / 右键行为符合本 Spec。
- 远程 JSON 往返 + 缓存回退验证通过。
- 自更新路径：检测 → 下载 → 校验 → 应用 → 重启，在 Win + macOS 测试环成功。
- 交付物不含 Chromium / Node 运行时。

## 11. 待决问题

1. 远程 JSON 服务归属与鉴权（无 / token / mTLS）？
2. 「已安装版本」由服务端上报、客户端探测，还是用户编辑？
3. 条目动作是否允许 shell 命令（安全风险），或仅 URL + 绝对路径？
4. 品牌 / 最终产品显示名。
5. 更新包格式：Win 用 zip 还是 MSI；macOS 用 dmg 还是 zip-of-.app？
6. 除 OS Authenticode / Apple 公证外，是否还要应用级 ed25519 签名？
7. 首发默认 `auto_update` 开还是关？
8. pthreads-w32 打包：静态链接还是 `pthreadVC2.dll`；是否优先 **pthreads4w**（Apache-2.0）而非旧版 LGPL 包？
