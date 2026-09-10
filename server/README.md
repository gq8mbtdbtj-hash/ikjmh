# tray_demo 统一服务（Gin + JWT + ACL，framework / modules 解耦）

**一个进程**：登录、业务 API、自更新、文件下载、ACL 管理后台。  
配置优先：`server.yaml` + `acl.yaml`。

```bash
cd server
go mod tidy
go run .                    # 或 go run . -config server.yaml
```

管理后台：<http://127.0.0.1:8080/admin/>（`admin` / `admin`）

## 包结构（业务解耦）

```
server/
  main.go                      # 薄入口
  server.yaml / acl.yaml       # 配置（主）
  internal/framework/          # JWT、ACL、中间件、Module 接口（不依赖 modules）
  internal/auth/               # health + login
  internal/modules/catalog/    # config / update / files（demo 业务）
  internal/modules/adminui/    # /admin + /api/v1/admin/*
  internal/app/                # 装配 Run()：Mount 各 Module
```

新增业务：实现 `framework.Module`（可选 `ACLProvider.DefaultACL`），在 [`internal/app/run.go`](internal/app/run.go) 的 `Mount(...)` 加一行。  
`acl.yaml` 同 id **优先**；缺失 id 由 module 碎片合并。后台次级入口：`d.Admin.RegisterNav`。

登录字段契约见 [docs/api-contract.md](../docs/api-contract.md)。

## 鉴权

登录 → JWT（`username` / `roles` / 可选 `cip`）→ 除 `public` 外全部 Bearer + ACL。

| 用户 | 密码 | 角色 |
|------|------|------|
| demo | demo | viewer |
| ops | ops | ops |
| admin | admin | admin |

## API

| 方法 | 路径 | 模块 |
|------|------|------|
| GET | `/api/v1/health` | auth |
| POST | `/api/v1/login` | auth |
| GET/PUT | `/api/v1/config.json` | catalog |
| GET | `/api/v1/update/manifest` | catalog |
| GET | `/api/v1/files/:name` | catalog |
| GET | `/admin/` | adminui |
| GET | `/api/v1/admin/extensions` | adminui（业务导航） |
| GET/PUT | `/api/v1/admin/acl` | adminui |
| GET | `/api/v1/admin/catalog/status` | catalog（扩展示例） |

## CUSTOMIZE

| 位置 | 用途 |
|------|------|
| `server.yaml` | 端口、LDAP、JWT、mock roles |
| `acl.yaml` | 按业务 API 分权 |
| `internal/modules/*` | 业务路由 |
| `internal/app/run.go` | 注册 Module |
| `internal/framework` | 仅改横切能力 |
