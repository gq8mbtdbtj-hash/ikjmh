package framework

import (
	"net/http"
	"strings"

	"github.com/gin-gonic/gin"
)

const CtxClaimsKey = "claims"

// CORSMiddleware 按配置附加 CORS。
func CORSMiddleware(allowOrigin string) gin.HandlerFunc {
	origin := allowOrigin
	if origin == "" {
		origin = "*"
	}
	return func(c *gin.Context) {
		c.Header("Access-Control-Allow-Origin", origin)
		c.Header("Access-Control-Allow-Headers", "Authorization, Content-Type")
		c.Header("Access-Control-Allow-Methods", "GET, PUT, POST, OPTIONS")
		if c.Request.Method == http.MethodOptions {
			c.AbortWithStatus(http.StatusNoContent)
			return
		}
		c.Next()
	}
}

// AuthAndACL 非 public 路由校验 JWT + ACL。
func AuthAndACL(cfg Config, acl *ACLStore) gin.HandlerFunc {
	return func(c *gin.Context) {
		method := c.Request.Method
		reqPath := c.Request.URL.Path

		if acl.IsPublic(method, reqPath) {
			c.Next()
			return
		}

		h := c.GetHeader("Authorization")
		const prefix = "bearer "
		if len(h) < len(prefix) || !strings.EqualFold(h[:len(prefix)], prefix) {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "missing bearer token"})
			return
		}
		raw := strings.TrimSpace(h[len(prefix):])
		claims, err := ParseJWT(cfg.JWTSecret, raw)
		if err != nil {
			c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "invalid token"})
			return
		}

		if cfg.BindClientIP && claims.ClientIP != "" {
			ip := ClientIP(c.Request.RemoteAddr, c.GetHeader("X-Forwarded-For"))
			if ip != claims.ClientIP {
				c.AbortWithStatusJSON(http.StatusUnauthorized, gin.H{"error": "token ip mismatch"})
				return
			}
		}

		if !acl.Allowed(method, reqPath, claims.Roles) {
			c.AbortWithStatusJSON(http.StatusForbidden, gin.H{
				"error": "forbidden",
				"path":  reqPath,
				"roles": claims.Roles,
			})
			return
		}

		c.Set(CtxClaimsKey, claims)
		c.Header("X-Auth-User", claims.Username)
		c.Next()
	}
}

// ClaimsFrom 读取中间件写入的 JWT claims。
func ClaimsFrom(c *gin.Context) *TokenClaims {
	v, ok := c.Get(CtxClaimsKey)
	if !ok {
		return nil
	}
	cl, _ := v.(*TokenClaims)
	return cl
}
