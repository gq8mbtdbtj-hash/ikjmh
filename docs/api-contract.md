# API 契约（B4）— 客户端 / 服务端稳定字段

登录与鉴权字段约定；**新增业务 API 不得改这些键名**。

## POST `/api/v1/login`

请求：

```json
{ "username": "demo", "password": "demo" }
```

成功 `200`：

| 字段 | 类型 | 说明 | C++ 客户端 |
|------|------|------|------------|
| `token` | string | JWT Bearer | **必读** → `settings.auth_token` |
| `username` | string | 登录名 | 优先于请求体写入 `settings.username` |
| `roles` | string[] | 角色 | 可选缓存 |
| `expires_in` | number | 秒 | 可选 |
| `client_ip` | string | 绑定 IP（可空） | 可选 |

失败：`401` / `400`，body 含 `error`。

## 鉴权头

```
Authorization: Bearer <token>
```

除 `acl.yaml` 中 `public: true` 外全部需要。

## 业务 API

业务 JSON 字段由各 `framework.Module` 自定；横切层只保证 JWT + ACL。
