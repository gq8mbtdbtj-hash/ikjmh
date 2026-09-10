#include "tray_demo/net/http_client.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

#include <sstream>

namespace tray_demo {

std::string HttpClient::JoinUrl(const std::string& base, const std::string& path) {
  if (base.empty()) {
    return path;
  }
  if (path.empty()) {
    return base;
  }
  const bool base_slash = base[base.size() - 1] == '/';
  const bool path_slash = path[0] == '/';
  if (base_slash && path_slash) {
    return base + path.substr(1);
  }
  if (!base_slash && !path_slash) {
    return base + "/" + path;
  }
  return base + path;
}

#ifdef _WIN32

namespace {

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return std::wstring();
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
  std::wstring out(static_cast<std::size_t>(n > 0 ? n - 1 : 0), L'\0');
  if (n > 0) {
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], n);
  }
  return out;
}

std::string WideToUtf8(const std::wstring& w) {
  if (w.empty()) {
    return std::string();
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
  std::string out(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
  if (n > 0) {
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], n, NULL, NULL);
  }
  return out;
}

bool CrackUrl(const std::string& url, bool* https, std::wstring* host, INTERNET_PORT* port,
              std::wstring* path_query) {
  URL_COMPONENTSW uc;
  ZeroMemory(&uc, sizeof(uc));
  uc.dwStructSize = sizeof(uc);
  wchar_t host_buf[256];
  wchar_t path_buf[2048];
  uc.lpszHostName = host_buf;
  uc.dwHostNameLength = 256;
  uc.lpszUrlPath = path_buf;
  uc.dwUrlPathLength = 2048;
  const std::wstring wurl = Utf8ToWide(url);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) {
    return false;
  }
  *https = (uc.nScheme == INTERNET_SCHEME_HTTPS);
  *host = std::wstring(uc.lpszHostName, uc.dwHostNameLength);
  *port = uc.nPort;
  *path_query = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength);
  return true;
}

HttpResponse DoRequest(const std::string& method,
                       const std::string& url,
                       const std::string& body,
                       const std::string& content_type,
                       const std::string& bearer) {
  HttpResponse resp;
  resp.status_code = 0;

  bool https = false;
  std::wstring host, path;
  INTERNET_PORT port = 0;
  if (!CrackUrl(url, &https, &host, &port, &path)) {
    resp.error = "invalid url";
    return resp;
  }

  HINTERNET session = WinHttpOpen(L"tray_demo/1.0",
                                  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    resp.error = "WinHttpOpen failed";
    return resp;
  }

  HINTERNET conn = WinHttpConnect(session, host.c_str(), port, 0);
  if (!conn) {
    resp.error = "WinHttpConnect failed";
    WinHttpCloseHandle(session);
    return resp;
  }

  DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(
      conn, Utf8ToWide(method).c_str(), path.c_str(), NULL,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!req) {
    resp.error = "WinHttpOpenRequest failed";
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return resp;
  }

  std::wstring headers;
  if (!content_type.empty()) {
    headers += L"Content-Type: ";
    headers += Utf8ToWide(content_type);
    headers += L"\r\n";
  }
  if (!bearer.empty()) {
    headers += L"Authorization: Bearer ";
    headers += Utf8ToWide(bearer);
    headers += L"\r\n";
  }

  const BOOL sent = WinHttpSendRequest(
      req,
      headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
      headers.empty() ? 0 : static_cast<DWORD>(-1),
      body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)body.data(),
      static_cast<DWORD>(body.size()),
      static_cast<DWORD>(body.size()),
      0);
  if (!sent || !WinHttpReceiveResponse(req, NULL)) {
    resp.error = "send/receive failed";
    WinHttpCloseHandle(req);
    WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return resp;
  }

  DWORD status = 0;
  DWORD status_size = sizeof(status);
  WinHttpQueryHeaders(req,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                      WINHTTP_NO_HEADER_INDEX);
  resp.status_code = static_cast<int>(status);

  std::string result;
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(req, &avail)) {
      break;
    }
    if (avail == 0) {
      break;
    }
    std::string chunk(avail, '\0');
    DWORD read = 0;
    if (!WinHttpReadData(req, &chunk[0], avail, &read)) {
      break;
    }
    chunk.resize(read);
    result += chunk;
  }
  resp.body = result;

  WinHttpCloseHandle(req);
  WinHttpCloseHandle(conn);
  WinHttpCloseHandle(session);
  return resp;
}

}  // namespace

HttpResponse HttpClient::Get(const std::string& url, const std::string& bearer_token) {
  return DoRequest("GET", url, "", "", bearer_token);
}

HttpResponse HttpClient::PostJson(const std::string& url,
                                  const std::string& json_body,
                                  const std::string& bearer_token) {
  return DoRequest("POST", url, json_body, "application/json; charset=utf-8",
                   bearer_token);
}

#else

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

namespace {

bool ParseUrl(const std::string& url, bool* https, std::string* host, int* port,
              std::string* path) {
  std::string rest;
  int default_port = 80;
  if (url.compare(0, 8, "https://") == 0) {
    *https = true;
    rest = url.substr(8);
    default_port = 443;
  } else if (url.compare(0, 7, "http://") == 0) {
    *https = false;
    rest = url.substr(7);
    default_port = 80;
  } else {
    return false;
  }

  std::size_t slash = rest.find('/');
  std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
  *path = slash == std::string::npos ? "/" : rest.substr(slash);

  // strip userinfo if present
  const std::size_t at = hostport.find('@');
  if (at != std::string::npos) {
    hostport = hostport.substr(at + 1);
  }

  std::size_t colon = hostport.rfind(':');
  if (colon != std::string::npos && hostport.find(']') == std::string::npos) {
    *host = hostport.substr(0, colon);
    *port = std::atoi(hostport.substr(colon + 1).c_str());
    if (*port <= 0) {
      return false;
    }
  } else {
    *host = hostport;
    *port = default_port;
  }
  return !host->empty();
}

int ConnectTcp(const std::string& host, int port) {
  char port_str[16];
  std::snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = 0;
  if (getaddrinfo(host.c_str(), port_str, &hints, &res) != 0 || !res) {
    return -1;
  }

  int fd = -1;
  for (struct addrinfo* p = res; p; p = p->ai_next) {
    fd = static_cast<int>(::socket(p->ai_family, p->ai_socktype, p->ai_protocol));
    if (fd < 0) {
      continue;
    }
    if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
      break;
    }
    ::close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

struct IoChannel {
  int fd;
#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
  SSL* ssl;
  SSL_CTX* ctx;
#endif

  IoChannel()
      : fd(-1)
#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
        ,
        ssl(0),
        ctx(0)
#endif
  {
  }

  void Close() {
#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
    if (ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
      ssl = 0;
    }
    if (ctx) {
      SSL_CTX_free(ctx);
      ctx = 0;
    }
#endif
    if (fd >= 0) {
      ::close(fd);
      fd = -1;
    }
  }

  bool SendAll(const char* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
      if (ssl) {
        const int n = SSL_write(ssl, data + sent, static_cast<int>(len - sent));
        if (n <= 0) {
          return false;
        }
        sent += static_cast<std::size_t>(n);
        continue;
      }
#endif
      const ssize_t n = ::send(fd, data + sent, len - sent, 0);
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }
      if (n == 0) {
        return false;
      }
      sent += static_cast<std::size_t>(n);
    }
    return true;
  }

  bool RecvAll(std::string* out) {
    char buf[4096];
    for (;;) {
#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
      if (ssl) {
        const int n = SSL_read(ssl, buf, static_cast<int>(sizeof(buf)));
        if (n > 0) {
          out->append(buf, static_cast<std::size_t>(n));
          continue;
        }
        if (n == 0) {
          return true;
        }
        const int err = SSL_get_error(ssl, n);
        if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
          continue;
        }
        return false;
      }
#endif
      const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }
      if (n == 0) {
        return true;
      }
      out->append(buf, static_cast<std::size_t>(n));
    }
  }
};

#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL

bool SslHandshake(IoChannel* ch, const std::string& host, std::string* err) {
  static bool openssl_ready = false;
  if (!openssl_ready) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
#else
    OPENSSL_init_ssl(0, NULL);
#endif
    openssl_ready = true;
  }

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  ch->ctx = SSL_CTX_new(SSLv23_client_method());
#else
  ch->ctx = SSL_CTX_new(TLS_client_method());
#endif
  if (!ch->ctx) {
    *err = "SSL_CTX_new failed";
    return false;
  }
  SSL_CTX_set_default_verify_paths(ch->ctx);
  SSL_CTX_set_verify(ch->ctx, SSL_VERIFY_PEER, NULL);
#if OPENSSL_VERSION_NUMBER < 0x10100000L
  SSL_CTX_set_options(ch->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
#endif

  ch->ssl = SSL_new(ch->ctx);
  if (!ch->ssl) {
    *err = "SSL_new failed";
    return false;
  }
  if (SSL_set_fd(ch->ssl, ch->fd) != 1) {
    *err = "SSL_set_fd failed";
    return false;
  }
  if (SSL_set_tlsext_host_name(ch->ssl, host.c_str()) != 1) {
    *err = "SNI failed";
    return false;
  }
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
  if (SSL_set1_host(ch->ssl, host.c_str()) != 1) {
    *err = "SSL_set1_host failed";
    return false;
  }
#endif
  if (SSL_connect(ch->ssl) != 1) {
    unsigned long e = ERR_get_error();
    char buf[256];
    ERR_error_string_n(e, buf, sizeof(buf));
    *err = std::string("SSL_connect failed: ") + buf;
    return false;
  }
  const long v = SSL_get_verify_result(ch->ssl);
  if (v != X509_V_OK) {
    *err = std::string("TLS certificate verify failed: ") +
           X509_verify_cert_error_string(v);
    return false;
  }
  return true;
}

#endif  // OPENSSL

HttpResponse ParseHttpReply(const std::string& reply) {
  HttpResponse resp;
  resp.status_code = 0;
  const std::size_t hdr_end = reply.find("\r\n\r\n");
  if (hdr_end == std::string::npos) {
    resp.error = "bad http response";
    return resp;
  }
  const std::string headers = reply.substr(0, hdr_end);
  resp.body = reply.substr(hdr_end + 4);

  std::size_t sp1 = headers.find(' ');
  if (sp1 != std::string::npos) {
    std::size_t sp2 = headers.find(' ', sp1 + 1);
    const std::string code =
        sp2 == std::string::npos ? headers.substr(sp1 + 1)
                                 : headers.substr(sp1 + 1, sp2 - sp1 - 1);
    resp.status_code = std::atoi(code.c_str());
  }
  return resp;
}

HttpResponse DoRequestPosix(const std::string& method,
                            const std::string& url,
                            const std::string& body,
                            const std::string& content_type,
                            const std::string& bearer) {
  HttpResponse resp;
  resp.status_code = 0;

  bool https = false;
  std::string host;
  int port = 80;
  std::string path;
  if (!ParseUrl(url, &https, &host, &port, &path)) {
    resp.error = "invalid url (expect http:// or https://)";
    return resp;
  }

  if (https) {
#if !(defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL)
    resp.error =
        "HTTPS requires OpenSSL (rebuild with OpenSSL; cmake finds libssl)";
    return resp;
#endif
  }

  IoChannel ch;
  ch.fd = ConnectTcp(host, port);
  if (ch.fd < 0) {
    resp.error = "connect failed";
    return resp;
  }

  if (https) {
#if defined(TRAY_DEMO_HAS_OPENSSL) && TRAY_DEMO_HAS_OPENSSL
    std::string tls_err;
    if (!SslHandshake(&ch, host, &tls_err)) {
      resp.error = tls_err;
      ch.Close();
      return resp;
    }
#endif
  }

  std::ostringstream req;
  req << method << " " << path << " HTTP/1.0\r\n";
  req << "Host: " << host;
  if ((https && port != 443) || (!https && port != 80)) {
    req << ":" << port;
  }
  req << "\r\n";
  req << "User-Agent: tray_demo/1.0\r\n";
  req << "Connection: close\r\n";
  if (!content_type.empty()) {
    req << "Content-Type: " << content_type << "\r\n";
  }
  if (!bearer.empty()) {
    req << "Authorization: Bearer " << bearer << "\r\n";
  }
  if (!body.empty()) {
    req << "Content-Length: " << body.size() << "\r\n";
  }
  req << "\r\n";
  if (!body.empty()) {
    req << body;
  }
  const std::string raw = req.str();
  if (!ch.SendAll(raw.data(), raw.size())) {
    resp.error = "send failed";
    ch.Close();
    return resp;
  }

  std::string reply;
  if (!ch.RecvAll(&reply)) {
    resp.error = "recv failed";
    ch.Close();
    return resp;
  }
  ch.Close();
  return ParseHttpReply(reply);
}

}  // namespace

HttpResponse HttpClient::Get(const std::string& url, const std::string& bearer_token) {
  return DoRequestPosix("GET", url, "", "", bearer_token);
}

HttpResponse HttpClient::PostJson(const std::string& url,
                                  const std::string& json_body,
                                  const std::string& bearer_token) {
  return DoRequestPosix("POST", url, json_body, "application/json; charset=utf-8",
                        bearer_token);
}

#endif

}  // namespace tray_demo
