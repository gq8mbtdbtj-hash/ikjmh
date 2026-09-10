package adminui

import "tray_demo/server/internal/framework"

// DefaultACL 管理后台碎片（B2）。
func (Module) DefaultACL() []framework.ACLRoute {
	return []framework.ACLRoute{
		{ID: "admin_ui", Method: "GET", Path: "/admin", Public: true},
		{ID: "admin_ui_slash", Method: "GET", Path: "/admin/", Public: true},
		{ID: "admin_api", Method: "*", Path: "/api/v1/admin/*", Roles: []string{"admin"}},
	}
}
