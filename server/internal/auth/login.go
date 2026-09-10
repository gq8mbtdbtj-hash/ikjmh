package auth

import (
	"fmt"
	"log"
	"net/http"
	"strings"
	"time"

	"tray_demo/server/internal/framework"

	"github.com/gin-gonic/gin"
	ldap "github.com/go-ldap/ldap/v3"
)

// Module 提供 health + login（框架级公开入口，仍按 acl public 放行）。
type Module struct{}

func (Module) Name() string { return "auth" }

func (Module) Register(r *gin.Engine, d *framework.Deps) {
	r.GET("/api/v1/health", func(c *gin.Context) {
		c.JSON(http.StatusOK, gin.H{
			"ok":        true,
			"auth_mode": d.Cfg.AuthMode,
			"time":      time.Now().UTC().Format(time.RFC3339),
		})
	})
	r.POST("/api/v1/login", func(c *gin.Context) { handleLogin(c, d) })
}

type loginReq struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

func handleLogin(c *gin.Context, d *framework.Deps) {
	var req loginReq
	if err := c.ShouldBindJSON(&req); err != nil {
		c.JSON(http.StatusBadRequest, gin.H{"error": "bad json"})
		return
	}
	req.Username = strings.TrimSpace(req.Username)
	if req.Username == "" || req.Password == "" {
		c.JSON(http.StatusBadRequest, gin.H{"error": "username/password required"})
		return
	}
	if err := Authenticate(d.Cfg, req.Username, req.Password); err != nil {
		log.Printf("login failed user=%s: %v", req.Username, err)
		c.JSON(http.StatusUnauthorized, gin.H{"error": "unauthorized"})
		return
	}
	roles := d.Cfg.ResolveRoles(req.Username)
	ip := framework.ClientIP(c.Request.RemoteAddr, c.GetHeader("X-Forwarded-For"))
	cip := ""
	if d.Cfg.BindClientIP {
		cip = ip
	}
	tok, expiresIn, err := framework.IssueJWT(d.Cfg.JWTSecret, req.Username, roles, cip, d.Cfg.TokenTTL)
	if err != nil {
		c.JSON(http.StatusInternalServerError, gin.H{"error": "token error"})
		return
	}
	c.JSON(http.StatusOK, gin.H{
		// B4 stable login contract — see docs/api-contract.md
		"token":      tok,
		"username":   req.Username,
		"roles":      roles,
		"expires_in": expiresIn,
		"client_ip":  cip,
	})
}

// Authenticate mock 或 LDAP Bind。
//
// CUSTOMIZE: LDAP search-then-bind 在此扩展。
func Authenticate(cfg framework.Config, user, pass string) error {
	switch cfg.AuthMode {
	case "mock":
		for _, u := range cfg.MockUsers {
			if u.Username == user && u.Password == pass {
				return nil
			}
		}
		return fmt.Errorf("mock: invalid credentials")
	case "ldap":
		return ldapBind(cfg, user, pass)
	default:
		return fmt.Errorf("unknown auth_mode %q", cfg.AuthMode)
	}
}

func ldapBind(cfg framework.Config, user, pass string) error {
	conn, err := ldap.DialURL(cfg.LDAPURL)
	if err != nil {
		return fmt.Errorf("ldap dial: %w", err)
	}
	defer conn.Close()

	dn := user
	if strings.Contains(cfg.LDAPUserFmt, "%s") {
		dn = strings.Replace(cfg.LDAPUserFmt, "%s", user, 1)
	}
	if err := conn.Bind(dn, pass); err != nil {
		return fmt.Errorf("ldap bind: %w", err)
	}
	return nil
}
