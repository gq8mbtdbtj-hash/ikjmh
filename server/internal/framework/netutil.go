package framework

import "strings"

// ClientIP 从 RemoteAddr / X-Forwarded-For 提取客户端 IP。
func ClientIP(remoteAddr, xff string) string {
	if xff != "" {
		parts := strings.Split(xff, ",")
		ip := strings.TrimSpace(parts[0])
		if ip != "" {
			return StripPort(ip)
		}
	}
	return StripPort(remoteAddr)
}

// StripPort 去掉 host:port 中的端口。
func StripPort(addr string) string {
	if strings.HasPrefix(addr, "[") {
		if i := strings.Index(addr, "]"); i >= 0 {
			return addr[1:i]
		}
	}
	if i := strings.LastIndex(addr, ":"); i >= 0 {
		if strings.Count(addr, ":") == 1 {
			return addr[:i]
		}
	}
	return addr
}
