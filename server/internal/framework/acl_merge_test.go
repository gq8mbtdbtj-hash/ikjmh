package framework

import (
	"os"
	"path/filepath"
	"testing"
)

func TestACLMergeDefaults(t *testing.T) {
	dir := t.TempDir()
	path := filepath.Join(dir, "acl.yaml")
	base := []byte("version: 1\nroutes:\n  - id: health\n    method: GET\n    path: /api/v1/health\n    public: true\n")
	if err := os.WriteFile(path, base, 0o644); err != nil {
		t.Fatal(err)
	}
	store, err := NewACLStore(path)
	if err != nil {
		t.Fatal(err)
	}
	store.SetModuleDefaults([]ACLRoute{
		{ID: "health", Method: "GET", Path: "/api/v1/health", Public: true}, // existing: keep file
		{ID: "login", Method: "POST", Path: "/api/v1/login", Public: true},
		{ID: "config_read", Method: "GET", Path: "/api/v1/config.json", Roles: []string{"viewer"}},
	})
	snap := store.Snapshot()
	if len(snap.Routes) != 3 {
		t.Fatalf("want 3 routes after merge, got %d", len(snap.Routes))
	}
	ids := map[string]bool{}
	for _, r := range snap.Routes {
		ids[r.ID] = true
	}
	for _, id := range []string{"health", "login", "config_read"} {
		if !ids[id] {
			t.Fatalf("missing id %s", id)
		}
	}
	if !store.IsPublic("POST", "/api/v1/login") {
		t.Fatal("login should be public after merge")
	}
}
