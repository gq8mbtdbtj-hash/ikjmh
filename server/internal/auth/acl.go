package auth

import "tray_demo/server/internal/framework"

// DefaultACL auth 公开入口碎片（B2）。
func (Module) DefaultACL() []framework.ACLRoute {
	return []framework.ACLRoute{
		{ID: "health", Method: "GET", Path: "/api/v1/health", Public: true},
		{ID: "login", Method: "POST", Path: "/api/v1/login", Public: true},
	}
}
