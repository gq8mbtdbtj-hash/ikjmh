package adminui

import (
	"net/http"

	"tray_demo/server/internal/framework"

	"github.com/gin-gonic/gin"
)

// Module ACL 管理后台（框架能力，独立 module 便于替换 UI）。
type Module struct{}

func (Module) Name() string { return "adminui" }

func (Module) Register(r *gin.Engine, d *framework.Deps) {
	r.GET("/admin", handlePage)
	r.GET("/admin/", handlePage)
	r.GET("/api/v1/admin/me", handleMe)
	r.GET("/api/v1/admin/extensions", func(c *gin.Context) { handleExtensions(c, d) })
	r.GET("/api/v1/admin/acl", func(c *gin.Context) { handleGetACL(c, d) })
	r.PUT("/api/v1/admin/acl", func(c *gin.Context) { handlePutACL(c, d) })
	r.POST("/api/v1/admin/acl/reload", func(c *gin.Context) { handleReloadACL(c, d) })
}

func handlePage(c *gin.Context) {
	c.Data(http.StatusOK, "text/html; charset=utf-8", []byte(adminHTML))
}

func handleMe(c *gin.Context) {
	cl := framework.ClaimsFrom(c)
	if cl == nil {
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		"username": cl.Username,
		"roles":    cl.Roles,
		"cip":      cl.ClientIP,
	})
}

func handleExtensions(c *gin.Context, d *framework.Deps) {
	items := []framework.AdminNavItem{}
	if d != nil && d.Admin != nil {
		items = d.Admin.Nav()
	}
	c.JSON(http.StatusOK, gin.H{"extensions": items})
}

func handleGetACL(c *gin.Context, d *framework.Deps) {
	c.JSON(http.StatusOK, d.ACL.Snapshot())
}

func handlePutACL(c *gin.Context, d *framework.Deps) {
	var doc framework.ACLFile
	if err := c.ShouldBindJSON(&doc); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "bad json"})
		return
	}
	if len(doc.Routes) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{"error": "routes empty"})
		return
	}
	if err := d.ACL.Save(doc); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true})
}

func handleReloadACL(c *gin.Context, d *framework.Deps) {
	if err := d.ACL.Reload(); err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
		return
	}
	c.JSON(http.StatusOK, gin.H{"ok": true, "acl": d.ACL.Snapshot()})
}
