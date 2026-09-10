package framework

import (
	"fmt"
	"os"
	"path"
	"strings"
	"sync"

	"gopkg.in/yaml.v3"
)

// ACLFile maps acl.yaml.
//
// CUSTOMIZE: add business API routes; admin UI can rewrite this file.
type ACLFile struct {
	Version int        `yaml:"version" json:"version"`
	Routes  []ACLRoute `yaml:"routes" json:"routes"`
}

// ACLRoute is one method+path permission rule.
type ACLRoute struct {
	ID     string   `yaml:"id" json:"id"`
	Method string   `yaml:"method" json:"method"`
	Path   string   `yaml:"path" json:"path"`
	Roles  []string `yaml:"roles" json:"roles"`
	Public bool     `yaml:"public" json:"public"`
}

// ACLStore is a thread-safe ACL store backed by a YAML file.
type ACLStore struct {
	mu       sync.RWMutex
	path     string
	doc      ACLFile
	defaults []ACLRoute // module DefaultACL fragments; fill missing ids
}

func NewACLStore(path string) (*ACLStore, error) {
	s := &ACLStore{path: path}
	if err := s.Reload(); err != nil {
		return nil, err
	}
	return s, nil
}

// SetModuleDefaults stores module ACL fragments and merges any missing route ids.
// File (acl.yaml) remains authoritative for existing ids.
func (s *ACLStore) SetModuleDefaults(routes []ACLRoute) {
	s.mu.Lock()
	s.defaults = append([]ACLRoute(nil), routes...)
	s.mergeDefaultsLocked()
	s.mu.Unlock()
}

func (s *ACLStore) Reload() error {
	b, err := os.ReadFile(s.path)
	if err != nil {
		return fmt.Errorf("read acl: %w", err)
	}
	var doc ACLFile
	if err := yaml.Unmarshal(b, &doc); err != nil {
		return fmt.Errorf("parse acl: %w", err)
	}
	if doc.Version == 0 {
		doc.Version = 1
	}
	s.mu.Lock()
	s.doc = doc
	s.mergeDefaultsLocked()
	s.mu.Unlock()
	return nil
}

// mergeDefaultsLocked appends default routes whose id is not already present.
func (s *ACLStore) mergeDefaultsLocked() {
	if len(s.defaults) == 0 {
		return
	}
	have := make(map[string]struct{}, len(s.doc.Routes))
	for _, r := range s.doc.Routes {
		if r.ID != "" {
			have[r.ID] = struct{}{}
		}
	}
	for _, r := range s.defaults {
		if r.ID == "" {
			continue
		}
		if _, ok := have[r.ID]; ok {
			continue
		}
		s.doc.Routes = append(s.doc.Routes, r)
		have[r.ID] = struct{}{}
	}
}

func (s *ACLStore) Snapshot() ACLFile {
	s.mu.RLock()
	defer s.mu.RUnlock()
	out := ACLFile{Version: s.doc.Version, Routes: make([]ACLRoute, len(s.doc.Routes))}
	copy(out.Routes, s.doc.Routes)
	return out
}

func (s *ACLStore) Save(doc ACLFile) error {
	if doc.Version == 0 {
		doc.Version = 1
	}
	b, err := yaml.Marshal(&doc)
	if err != nil {
		return err
	}
	header := []byte("# API ACL — editable via /admin; CUSTOMIZE by business route\n")
	if err := os.WriteFile(s.path, append(header, b...), 0o644); err != nil {
		return err
	}
	s.mu.Lock()
	s.doc = doc
	s.mu.Unlock()
	return nil
}

// IsPublic reports whether method+path skips JWT.
func (s *ACLStore) IsPublic(method, reqPath string) bool {
	s.mu.RLock()
	defer s.mu.RUnlock()
	for _, r := range s.doc.Routes {
		if !r.Public {
			continue
		}
		if matchMethod(r.Method, method) && matchPath(r.Path, reqPath) {
			return true
		}
	}
	return false
}

// Allowed reports whether roles may access method+path.
func (s *ACLStore) Allowed(method, reqPath string, roles []string) bool {
	s.mu.RLock()
	defer s.mu.RUnlock()

	matched := false
	for _, r := range s.doc.Routes {
		if r.Public {
			continue
		}
		if !matchMethod(r.Method, method) || !matchPath(r.Path, reqPath) {
			continue
		}
		matched = true
		if rolePermitted(r.Roles, roles) {
			return true
		}
	}
	if !matched {
		return false
	}
	return false
}

func matchMethod(rule, method string) bool {
	rule = strings.ToUpper(strings.TrimSpace(rule))
	method = strings.ToUpper(strings.TrimSpace(method))
	return rule == "*" || rule == method
}

func matchPath(pattern, reqPath string) bool {
	pattern = path.Clean("/" + strings.TrimPrefix(pattern, "/"))
	reqPath = path.Clean("/" + strings.TrimPrefix(reqPath, "/"))
	if strings.HasSuffix(pattern, "/*") {
		prefix := strings.TrimSuffix(pattern, "/*")
		return reqPath == prefix || strings.HasPrefix(reqPath, prefix+"/")
	}
	if strings.HasSuffix(pattern, "*") {
		prefix := strings.TrimSuffix(pattern, "*")
		return strings.HasPrefix(reqPath, prefix)
	}
	return pattern == reqPath
}

func rolePermitted(need, have []string) bool {
	for _, n := range need {
		if n == "*" {
			return true
		}
	}
	for _, h := range have {
		if h == "admin" {
			return true
		}
		for _, n := range need {
			if h == n {
				return true
			}
		}
	}
	return false
}
