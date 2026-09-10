package framework

import (
	"fmt"
	"os"
	"strings"

	"gopkg.in/yaml.v3"
)

// FileConfig maps server.yaml (primary config).
//
// CUSTOMIZE: keep tags in sync with server.yaml.
type FileConfig struct {
	Listen   string              `yaml:"listen"`
	AuthMode string              `yaml:"auth_mode"`
	DataDir  string              `yaml:"data_dir"`
	ACLFile  string              `yaml:"acl_file"`
	LDAP     LDAPConfig          `yaml:"ldap"`
	Mock     MockConfig          `yaml:"mock"`
	RoleMap  map[string][]string `yaml:"role_map"`
	CORS     CORSConfig          `yaml:"cors"`
	Token    TokenConfig         `yaml:"token"`
}

// LDAPConfig LDAP connection and bind DN template.
type LDAPConfig struct {
	URL          string `yaml:"url"`
	BaseDN       string `yaml:"base_dn"`
	UserDNFormat string `yaml:"user_dn_format"`
}

// MockConfig 涓烘棤 LDAP 鏃剁殑鏈湴璐﹀彿琛ㄣ€?
type MockConfig struct {
	Users []MockUser `yaml:"users"`
}

// MockUser 鏄紨绀鸿处鍙凤紝鍙甫 roles銆?
type MockUser struct {
	Username string   `yaml:"username"`
	Password string   `yaml:"password"`
	Roles    []string `yaml:"roles"`
}

// CORSConfig 鎺у埗璺ㄥ煙鍝嶅簲澶淬€?
type CORSConfig struct {
	AllowOrigin string `yaml:"allow_origin"`
}

// TokenConfig 鎺у埗 JWT 浼氳瘽銆?
type TokenConfig struct {
	TTLHours     int    `yaml:"ttl_hours"`
	JWTSecret    string `yaml:"jwt_secret"`
	BindClientIP bool   `yaml:"bind_client_ip"`
}

// Config 鏄繘绋嬪唴杩愯鍙傛暟銆?
type Config struct {
	Addr         string
	AuthMode     string
	LDAPURL      string
	LDAPBaseDN   string
	LDAPUserFmt  string
	DataDir      string
	ACLFile      string
	AllowOrigin  string
	TokenTTL     int
	JWTSecret    []byte
	BindClientIP bool
	MockUsers    []MockUser
	RoleMap      map[string][]string
}

func defaultFileConfig() FileConfig {
	return FileConfig{
		Listen:   ":8080",
		AuthMode: "mock",
		DataDir:  "./data",
		ACLFile:  "./acl.yaml",
		LDAP: LDAPConfig{
			URL:          "ldap://127.0.0.1:389",
			BaseDN:       "dc=example,dc=com",
			UserDNFormat: "uid=%s,ou=people,dc=example,dc=com",
		},
		Mock: MockConfig{
			Users: []MockUser{
				{Username: "demo", Password: "demo", Roles: []string{"viewer"}},
				{Username: "admin", Password: "admin", Roles: []string{"admin"}},
			},
		},
		RoleMap: map[string][]string{"default": {"viewer"}},
		CORS:    CORSConfig{AllowOrigin: "*"},
		Token: TokenConfig{
			TTLHours:     12,
			JWTSecret:    "tray_demo_dev_secret_change_me",
			BindClientIP: true,
		},
	}
}

func LoadFileConfig(path string) (FileConfig, error) {
	cfg := defaultFileConfig()
	b, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return cfg, fmt.Errorf("config file not found: %s (copy server.yaml or pass -config)", path)
		}
		return cfg, err
	}
	if err := yaml.Unmarshal(b, &cfg); err != nil {
		return cfg, fmt.Errorf("parse %s: %w", path, err)
	}
	normalizeFileConfig(&cfg)
	applyEnvOverrides(&cfg)
	return cfg, nil
}

func normalizeFileConfig(c *FileConfig) {
	c.AuthMode = strings.ToLower(strings.TrimSpace(c.AuthMode))
	if c.Listen == "" {
		c.Listen = ":8080"
	}
	if c.DataDir == "" {
		c.DataDir = "./data"
	}
	if c.ACLFile == "" {
		c.ACLFile = "./acl.yaml"
	}
	if c.Token.TTLHours <= 0 {
		c.Token.TTLHours = 12
	}
	if c.Token.JWTSecret == "" {
		c.Token.JWTSecret = "tray_demo_dev_secret_change_me"
	}
	if c.CORS.AllowOrigin == "" {
		c.CORS.AllowOrigin = "*"
	}
	if c.LDAP.UserDNFormat == "" {
		c.LDAP.UserDNFormat = "uid=%s,ou=people,dc=example,dc=com"
	}
	if c.RoleMap == nil {
		c.RoleMap = map[string][]string{"default": {"viewer"}}
	}
}

func applyEnvOverrides(c *FileConfig) {
	if v := os.Getenv("TRAY_DEMO_LISTEN"); v != "" {
		c.Listen = v
	}
	if v := os.Getenv("TRAY_DEMO_AUTH_MODE"); v != "" {
		c.AuthMode = strings.ToLower(v)
	}
	if v := os.Getenv("TRAY_DEMO_DATA_DIR"); v != "" {
		c.DataDir = v
	}
	if v := os.Getenv("TRAY_DEMO_ACL_FILE"); v != "" {
		c.ACLFile = v
	}
	if v := os.Getenv("TRAY_DEMO_JWT_SECRET"); v != "" {
		c.Token.JWTSecret = v
	}
	if v := os.Getenv("TRAY_DEMO_LDAP_URL"); v != "" {
		c.LDAP.URL = v
	}
	if v := os.Getenv("TRAY_DEMO_LDAP_BASE_DN"); v != "" {
		c.LDAP.BaseDN = v
	}
	if v := os.Getenv("TRAY_DEMO_LDAP_USER_DN_FORMAT"); v != "" {
		c.LDAP.UserDNFormat = v
	}
}

func (c FileConfig) ToRuntime() Config {
	return Config{
		Addr:         c.Listen,
		AuthMode:     c.AuthMode,
		LDAPURL:      c.LDAP.URL,
		LDAPBaseDN:   c.LDAP.BaseDN,
		LDAPUserFmt:  c.LDAP.UserDNFormat,
		DataDir:      c.DataDir,
		ACLFile:      c.ACLFile,
		AllowOrigin:  c.CORS.AllowOrigin,
		TokenTTL:     c.Token.TTLHours,
		JWTSecret:    []byte(c.Token.JWTSecret),
		BindClientIP: c.Token.BindClientIP,
		MockUsers:    c.Mock.Users,
		RoleMap:      c.RoleMap,
	}
}

// ResolveRoles returns roles for username (mock roles or role_map / default).
func (c *Config) ResolveRoles(username string) []string {
	for _, u := range c.MockUsers {
		if u.Username == username && len(u.Roles) > 0 {
			return append([]string{}, u.Roles...)
		}
	}
	if c.RoleMap != nil {
		if r, ok := c.RoleMap[username]; ok && len(r) > 0 {
			return append([]string{}, r...)
		}
		if r, ok := c.RoleMap["default"]; ok && len(r) > 0 {
			return append([]string{}, r...)
		}
	}
	return []string{"viewer"}
}
