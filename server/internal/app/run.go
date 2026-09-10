package app

import (
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"

	"tray_demo/server/internal/auth"
	"tray_demo/server/internal/framework"
	"tray_demo/server/internal/modules/adminui"
	"tray_demo/server/internal/modules/catalog"
)

// Run 装载配置、挂载 modules 并监听。
//
// CUSTOMIZE: 增删业务时只改 Mount 列表。
func Run(args []string) error {
	fs := flag.NewFlagSet("tray_demo_server", flag.ContinueOnError)
	configPath := fs.String("config", "server.yaml", "path to YAML config (primary settings source)")
	if err := fs.Parse(args); err != nil {
		return err
	}

	fileCfg, err := framework.LoadFileConfig(*configPath)
	if err != nil {
		return err
	}
	cfg := fileCfg.ToRuntime()

	if err := os.MkdirAll(filepath.Join(cfg.DataDir, "files"), 0o755); err != nil {
		return err
	}

	acl, err := framework.NewACLStore(cfg.ACLFile)
	if err != nil {
		return err
	}

	deps := &framework.Deps{
		Cfg:   cfg,
		ACL:   acl,
		Admin: &framework.AdminRegistry{},
	}
	r := framework.NewEngine(deps)

	// 框架级 auth + 业务 modules（互不 import）
	framework.Mount(r, deps,
		auth.Module{},
		catalog.Module{},
		adminui.Module{},
	)

	log.Printf("tray_demo server on %s (auth=%s acl=%s data=%s)",
		cfg.Addr, cfg.AuthMode, cfg.ACLFile, cfg.DataDir)
	log.Printf("admin UI: http://127.0.0.1%s/admin/  (admin/admin)", cfg.Addr)
	log.Printf("modules: auth, catalog, adminui")
	if err := r.Run(cfg.Addr); err != nil {
		return fmt.Errorf("listen: %w", err)
	}
	return nil
}
