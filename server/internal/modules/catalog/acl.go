package catalog

import "tray_demo/server/internal/framework"

// DefaultACL catalog 业务路由碎片（B2；acl.yaml 已有同 id 时不覆盖）。
func (Module) DefaultACL() []framework.ACLRoute {
	return []framework.ACLRoute{
		{ID: "config_read", Method: "GET", Path: "/api/v1/config.json", Roles: []string{"admin", "ops", "viewer"}},
		{ID: "config_write", Method: "PUT", Path: "/api/v1/config.json", Roles: []string{"admin", "ops"}},
		{ID: "config_write_post", Method: "POST", Path: "/api/v1/config.json", Roles: []string{"admin", "ops"}},
		{ID: "update_manifest", Method: "GET", Path: "/api/v1/update/manifest", Roles: []string{"admin", "ops", "viewer"}},
		{ID: "files_get", Method: "GET", Path: "/api/v1/files/*", Roles: []string{"admin", "ops", "viewer"}},
		{ID: "admin_catalog_status", Method: "GET", Path: "/api/v1/admin/catalog/status", Roles: []string{"admin"}},
	}
}
