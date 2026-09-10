package framework

import "sync"

// AdminNavItem 管理后台导航扩展（业务 module 注册，adminui 展示）。
//
// Path 可为 /admin/... 页面或 /api/v1/admin/... API（由 Kind 区分）。
type AdminNavItem struct {
	ID    string `json:"id"`
	Title string `json:"title"`
	Path  string `json:"path"`
	Kind  string `json:"kind"` // "page" | "api"
}

// AdminRegistry 收集业务模块挂到 /admin 的次级入口。
// ACL 编辑本身仍由 adminui 提供；本注册表只做导航/扩展发现。
type AdminRegistry struct {
	mu    sync.Mutex
	items []AdminNavItem
}

// RegisterNav 追加一条后台导航（幂等按 ID 覆盖）。
func (a *AdminRegistry) RegisterNav(item AdminNavItem) {
	if a == nil || item.ID == "" || item.Path == "" {
		return
	}
	if item.Kind == "" {
		item.Kind = "api"
	}
	a.mu.Lock()
	defer a.mu.Unlock()
	for i := range a.items {
		if a.items[i].ID == item.ID {
			a.items[i] = item
			return
		}
	}
	a.items = append(a.items, item)
}

// Nav 返回当前扩展列表副本。
func (a *AdminRegistry) Nav() []AdminNavItem {
	if a == nil {
		return nil
	}
	a.mu.Lock()
	defer a.mu.Unlock()
	out := make([]AdminNavItem, len(a.items))
	copy(out, a.items)
	return out
}
