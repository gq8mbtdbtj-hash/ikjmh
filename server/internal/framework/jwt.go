package framework

import (
	"fmt"
	"time"

	"github.com/golang-jwt/jwt/v5"
)

// TokenClaims 鏄鍙戠粰瀹㈡埛绔殑 JWT claims銆?
//
// 鍚?LDAP/mock 韬唤銆佽鑹层€佸彲閫夌粦瀹?IP锛涙湇鍔＄鏍￠獙绛惧悕涓庢潈闄愩€?
type TokenClaims struct {
	Username string   `json:"username"`
	Roles    []string `json:"roles"`
	ClientIP string   `json:"cip,omitempty"`
	jwt.RegisteredClaims
}

func IssueJWT(secret []byte, username string, roles []string, clientIP string, ttlHours int) (string, int, error) {
	if ttlHours <= 0 {
		ttlHours = 12
	}
	expiresIn := ttlHours * 3600
	now := time.Now()
	claims := TokenClaims{
		Username: username,
		Roles:    roles,
		ClientIP: clientIP,
		RegisteredClaims: jwt.RegisteredClaims{
			Subject:   username,
			IssuedAt:  jwt.NewNumericDate(now),
			ExpiresAt: jwt.NewNumericDate(now.Add(time.Duration(ttlHours) * time.Hour)),
			Issuer:    "tray_demo",
		},
	}
	t := jwt.NewWithClaims(jwt.SigningMethodHS256, claims)
	signed, err := t.SignedString(secret)
	if err != nil {
		return "", 0, err
	}
	return signed, expiresIn, nil
}

func ParseJWT(secret []byte, tokenStr string) (*TokenClaims, error) {
	tok, err := jwt.ParseWithClaims(tokenStr, &TokenClaims{}, func(t *jwt.Token) (any, error) {
		if t.Method != jwt.SigningMethodHS256 {
			return nil, fmt.Errorf("unexpected signing method")
		}
		return secret, nil
	})
	if err != nil {
		return nil, err
	}
	claims, ok := tok.Claims.(*TokenClaims)
	if !ok || !tok.Valid {
		return nil, fmt.Errorf("invalid token")
	}
	return claims, nil
}
