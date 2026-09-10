package catalog

import (
	"encoding/json"
	"io"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"time"

	"tray_demo/server/internal/framework"

	"github.com/gin-gonic/gin"
)

// Module 目录配置 / 更新清单 / 文件下载（demo 业务）。
//
// CUSTOMIZE: 新产品可另建 modules/xxx，勿改 framework。
type Module struct{}

func (Module) Name() string { return "catalog" }

func (Module) Register(r *gin.Engine, d *framework.Deps) {
	EnsureSeedData(d.Cfg.DataDir)
	r.GET("/api/v1/config.json", func(c *gin.Context) { handleConfig(c, d) })
	r.PUT("/api/v1/config.json", func(c *gin.Context) { handleConfig(c, d) })
	r.POST("/api/v1/config.json", func(c *gin.Context) { handleConfig(c, d) })
	r.GET("/api/v1/update/manifest", handleUpdateManifest)
	r.GET("/api/v1/files/:name", func(c *gin.Context) { handleFiles(c, d) })

	// B3: 业务向 admin 注册次级 API（需 admin 角色；见 acl.yaml /api/v1/admin/*）
	if d.Admin != nil {
		d.Admin.RegisterNav(framework.AdminNavItem{
			ID:    "catalog-status",
			Title: "Catalog 状态",
			Path:  "/api/v1/admin/catalog/status",
			Kind:  "api",
		})
	}
	r.GET("/api/v1/admin/catalog/status", func(c *gin.Context) { handleAdminStatus(c, d) })
}

func handleAdminStatus(c *gin.Context, d *framework.Deps) {
	cfgPath := filepath.Join(d.Cfg.DataDir, "config.json")
	st, err := os.Stat(cfgPath)
	info := gin.H{
		"module":   "catalog",
		"data_dir": d.Cfg.DataDir,
	}
	if err != nil {
		info["config_json"] = gin.H{"present": false, "error": err.Error()}
	} else {
		info["config_json"] = gin.H{
			"present":  true,
			"size":     st.Size(),
			"modified": st.ModTime().UTC().Format(time.RFC3339),
		}
	}
	filesDir := filepath.Join(d.Cfg.DataDir, "files")
	entries, _ := os.ReadDir(filesDir)
	names := make([]string, 0, len(entries))
	for _, e := range entries {
		if !e.IsDir() {
			names = append(names, e.Name())
		}
	}
	info["files"] = names
	c.JSON(http.StatusOK, info)
}

func handleConfig(c *gin.Context, d *framework.Deps) {
	path := filepath.Join(d.Cfg.DataDir, "config.json")
	switch c.Request.Method {
	case http.MethodGet:
		b, err := os.ReadFile(path)
		if err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
			return
		}
		c.Data(http.StatusOK, "application/json; charset=utf-8", b)
	case http.MethodPut, http.MethodPost:
		b, err := io.ReadAll(io.LimitReader(c.Request.Body, 2<<20))
		if err != nil {
			c.JSON(http.StatusBadRequest, gin.H{"error": err.Error()})
			return
		}
		if !json.Valid(b) {
			c.JSON(http.StatusBadRequest, gin.H{"error": "body must be json"})
			return
		}
		if err := os.WriteFile(path, b, 0o644); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{"error": err.Error()})
			return
		}
		user := c.GetHeader("X-Auth-User")
		log.Printf("config.json updated by %s (%d bytes)", user, len(b))
		c.JSON(http.StatusOK, gin.H{"ok": true, "bytes": len(b)})
	default:
		c.JSON(http.StatusMethodNotAllowed, gin.H{"error": "method not allowed"})
	}
}

func handleUpdateManifest(c *gin.Context) {
	c.JSON(http.StatusOK, gin.H{
		"schema_version": 1,
		"latest_version": "0.2.0",
		"channel":        "stable",
		"notes_url":      "/api/v1/files/sample.txt",
		"mandatory":      false,
		"packages": []gin.H{
			{
				"os": "windows", "arch": "x86_64",
				"url": "/api/v1/files/sample.txt", "sha256": "demo",
				"size_bytes": 12, "format": "raw",
			},
		},
	})
}

func handleFiles(c *gin.Context, d *framework.Deps) {
	name := filepath.Base(c.Param("name"))
	if name == "." || name == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "bad file"})
		return
	}
	path := filepath.Join(d.Cfg.DataDir, "files", name)
	c.FileAttachment(path, name)
}

// EnsureSeedData 写入默认 catalog / sample（若不存在）。
func EnsureSeedData(dataDir string) {
	_ = os.MkdirAll(filepath.Join(dataDir, "files"), 0o755)
	cfgPath := filepath.Join(dataDir, "config.json")
	if _, err := os.Stat(cfgPath); err != nil {
		doc := map[string]any{
			"schema_version": 1,
			"generated_at":   time.Now().UTC().Format(time.RFC3339),
			"app": map[string]any{
				"min_client_version": "0.1.0",
				"message":            "hello from tray_demo server",
			},
			"groups": []map[string]any{
				{
					"id": "toolchains", "title": "Toolchains",
					"items": []map[string]any{
						{
							"id": "demo-tool", "name": "Demo Tool",
							"installed_version": "1.0.0", "available_version": "1.1.0",
							"status": "update_available",
							"action": map[string]string{"type": "url", "value": "https://example.com"},
						},
					},
				},
			},
		}
		b, _ := json.MarshalIndent(doc, "", "  ")
		_ = os.WriteFile(cfgPath, b, 0o644)
	}
	sample := filepath.Join(dataDir, "files", "sample.txt")
	if _, err := os.Stat(sample); err != nil {
		_ = os.WriteFile(sample, []byte("tray_demo sample download\n"), 0o644)
	}
}
