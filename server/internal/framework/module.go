package framework

import "github.com/gin-gonic/gin"

// Deps 注入给业务 module 的框架依赖。
type Deps struct {
	Cfg   Config
	ACL   *ACLStore
	Admin *AdminRegistry
}

// Module 业务模块：只注册自己的路由，不依赖其它 modules。
//
// CUSTOMIZE: 新产品实现 Module 并在 app.Run 中注册。
type Module interface {
	Name() string
	Register(r *gin.Engine, d *Deps)
}

// ACLProvider 可选：提供默认 ACL 路由碎片（按 id 合并进 acl.yaml，不覆盖已有条目）。
type ACLProvider interface {
	DefaultACL() []ACLRoute
}

// NewEngine 创建带日志 / CORS / JWT+ACL 的 Gin 引擎。
func NewEngine(d *Deps) *gin.Engine {
	gin.SetMode(gin.ReleaseMode)
	r := gin.New()
	r.Use(gin.Logger(), gin.Recovery(), CORSMiddleware(d.Cfg.AllowOrigin), AuthAndACL(d.Cfg, d.ACL))
	return r
}

// Mount 依次注册 modules，并合并各 module 的 DefaultACL 碎片。
func Mount(r *gin.Engine, d *Deps, mods ...Module) {
	var defaults []ACLRoute
	for _, m := range mods {
		if m == nil {
			continue
		}
		m.Register(r, d)
		if p, ok := m.(ACLProvider); ok {
			defaults = append(defaults, p.DefaultACL()...)
		}
	}
	if d != nil && d.ACL != nil && len(defaults) > 0 {
		d.ACL.SetModuleDefaults(defaults)
	}
}
