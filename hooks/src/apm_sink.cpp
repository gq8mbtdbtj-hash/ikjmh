/**
 * @file apm_sink.cpp
 * @brief Collector sink：缓冲 → NDJSON 文件 / HTTP(S) POST，持续观测。
 */

#include "tray_hooks/apm.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#if defined(TRAY_HOOKS_HAVE_OPENSSL) && TRAY_HOOKS_HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif
#endif

namespace {

bool TlsInsecureEnv() {
  const char* v = std::getenv("TRAY_HOOKS_APM_TLS_INSECURE");
  return v && (*v == '1' || *v == 'y' || *v == 'Y' || *v == 't' || *v == 'T');
}

std::mutex g_mu;
bool g_running = false;
tray_hooks_apm_config_t g_cfg = {};
std::string g_file;
std::string g_url;
int g_interval_ms = 2000;
int g_batch_max = 64;
int g_include_stacks = 1;
std::vector<std::string> g_buf;
std::thread g_thread;
std::atomic<bool> g_stop{false};

std::string EscapeJson(const char* s) {
  std::string o;
  if (!s) {
    return o;
  }
  for (const char* p = s; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == '"' || c == '\\') {
      o.push_back('\\');
      o.push_back(static_cast<char>(c));
    } else if (c < 0x20) {
      char tmp[8];
      std::snprintf(tmp, sizeof(tmp), "\\u%04x", c);
      o += tmp;
    } else {
      o.push_back(static_cast<char>(c));
    }
  }
  return o;
}

std::string EventToJson(const tray_hooks_event_t* ev, int stacks) {
  char head[512];
  std::snprintf(
      head, sizeof(head),
      "{\"ts\":%lld,\"tag\":\"%s\",\"arg0\":%llu,\"pid\":%u,\"tid\":%u,"
      "\"process\":\"%s\"",
      static_cast<long long>(ev->timestamp_ms), EscapeJson(ev->tag).c_str(),
      static_cast<unsigned long long>(ev->arg0), ev->pid, ev->tid,
      EscapeJson(ev->process).c_str());
  std::string j = head;
  if (stacks && ev->nframes > 0) {
    j += ",\"frames\":[";
    for (int i = 0; i < ev->nframes; ++i) {
      if (i) {
        j += ",";
      }
      char fr[384];
      std::snprintf(fr, sizeof(fr),
                    "{\"pc\":\"%p\",\"sym\":\"%s\",\"mod\":\"%s\",\"off\":%lu}",
                    ev->frames[i].pc, EscapeJson(ev->frames[i].symbol).c_str(),
                    EscapeJson(ev->frames[i].module).c_str(),
                    static_cast<unsigned long>(ev->frames[i].offset));
      j += fr;
    }
    j += "]";
  }
  j += "}";
  return j;
}

void AppendFile(const std::vector<std::string>& lines) {
  if (g_file.empty() || lines.empty()) {
    return;
  }
  FILE* fp = std::fopen(g_file.c_str(), "a");
  if (!fp) {
    return;
  }
  for (size_t i = 0; i < lines.size(); ++i) {
    std::fputs(lines[i].c_str(), fp);
    std::fputc('\n', fp);
  }
  std::fclose(fp);
}

#if defined(_WIN32)

bool HttpPostNdjson(const std::string& url, const std::string& body) {
  if (url.empty() || body.empty()) {
    return false;
  }
  // 粗解析 http(s)://host:port/path
  std::string u = url;
  bool https = false;
  if (u.find("https://") == 0) {
    https = true;
    u = u.substr(8);
  } else if (u.find("http://") == 0) {
    u = u.substr(7);
  }
  std::string host;
  std::string path = "/";
  INTERNET_PORT port = https ? 443 : 80;
  size_t slash = u.find('/');
  std::string hostport = slash == std::string::npos ? u : u.substr(0, slash);
  if (slash != std::string::npos) {
    path = u.substr(slash);
  }
  size_t colon = hostport.find(':');
  if (colon != std::string::npos) {
    host = hostport.substr(0, colon);
    port = static_cast<INTERNET_PORT>(std::atoi(hostport.substr(colon + 1).c_str()));
  } else {
    host = hostport;
  }

  std::wstring whost(host.begin(), host.end());
  std::wstring wpath(path.begin(), path.end());

  HINTERNET ses = WinHttpOpen(L"tray_hooks_apm/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) {
    return false;
  }
  HINTERNET con = WinHttpConnect(ses, whost.c_str(), port, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return false;
  }
  DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(con, L"POST", wpath.c_str(), NULL,
                                     WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                     flags);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return false;
  }
  if (https && TlsInsecureEnv()) {
    DWORD sec = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &sec, sizeof(sec));
  }
  const wchar_t* headers = L"Content-Type: application/x-ndjson\r\n";
  BOOL ok = WinHttpSendRequest(req, headers, (DWORD)-1L, (LPVOID)body.data(),
                               (DWORD)body.size(), (DWORD)body.size(), 0);
  if (ok) {
    ok = WinHttpReceiveResponse(req, NULL);
  }
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return ok == TRUE;
}

#else

bool ParseHttpUrl(const std::string& url, bool* https, std::string* host,
                  std::string* path, int* port) {
  std::string u = url;
  *https = false;
  if (u.find("https://") == 0) {
    *https = true;
    u = u.substr(8);
  } else if (u.find("http://") == 0) {
    u = u.substr(7);
  } else {
    return false;
  }
  *path = "/";
  *port = *https ? 443 : 80;
  size_t slash = u.find('/');
  std::string hostport = slash == std::string::npos ? u : u.substr(0, slash);
  if (slash != std::string::npos) {
    *path = u.substr(slash);
  }
  size_t colon = hostport.find(':');
  if (colon != std::string::npos) {
    *host = hostport.substr(0, colon);
    *port = std::atoi(hostport.substr(colon + 1).c_str());
  } else {
    *host = hostport;
  }
  return !host->empty();
}

int TcpConnect(const std::string& host, int port) {
  struct addrinfo hints;
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;
  struct addrinfo* res = 0;
  char port_s[16];
  std::snprintf(port_s, sizeof(port_s), "%d", port);
  if (getaddrinfo(host.c_str(), port_s, &hints, &res) != 0 || !res) {
    return -1;
  }
  int fd = -1;
  for (struct addrinfo* p = res; p; p = p->ai_next) {
    fd = static_cast<int>(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
    if (fd < 0) {
      continue;
    }
    if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  return fd;
}

bool SendAll(int fd, const char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(fd, data + sent, len - sent, 0);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

#if defined(TRAY_HOOKS_HAVE_OPENSSL) && TRAY_HOOKS_HAVE_OPENSSL

bool SslSendAll(SSL* ssl, const char* data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int n = SSL_write(ssl, data + sent, static_cast<int>(len - sent));
    if (n <= 0) {
      return false;
    }
    sent += static_cast<size_t>(n);
  }
  return true;
}

bool HttpsPostNdjson(const std::string& host, int port, const std::string& path,
                     const std::string& body) {
  int fd = TcpConnect(host, port);
  if (fd < 0) {
    return false;
  }

  SSL_library_init();
  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();

  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    close(fd);
    return false;
  }
  if (TlsInsecureEnv()) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
  } else {
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
  }

  SSL* ssl = SSL_new(ctx);
  if (!ssl) {
    SSL_CTX_free(ctx);
    close(fd);
    return false;
  }
  SSL_set_tlsext_host_name(ssl, host.c_str());
  SSL_set_fd(ssl, fd);
  if (SSL_connect(ssl) != 1) {
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);
    return false;
  }
  if (!TlsInsecureEnv()) {
    long vr = SSL_get_verify_result(ssl);
    if (vr != X509_V_OK) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
      SSL_CTX_free(ctx);
      close(fd);
      return false;
    }
  }

  char hdr[512];
  std::snprintf(hdr, sizeof(hdr),
                "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: "
                "application/x-ndjson\r\nContent-Length: %zu\r\nConnection: "
                "close\r\n\r\n",
                path.c_str(), host.c_str(), body.size());
  std::string req = std::string(hdr) + body;
  bool ok = SslSendAll(ssl, req.data(), req.size());
  if (ok) {
    char tmp[256];
    (void)SSL_read(ssl, tmp, sizeof(tmp));
  }
  SSL_shutdown(ssl);
  SSL_free(ssl);
  SSL_CTX_free(ctx);
  close(fd);
  return ok;
}

#endif  // TRAY_HOOKS_HAVE_OPENSSL

bool HttpPostNdjson(const std::string& url, const std::string& body) {
  if (url.empty() || body.empty()) {
    return false;
  }
  bool https = false;
  std::string host;
  std::string path = "/";
  int port = 80;
  if (!ParseHttpUrl(url, &https, &host, &path, &port)) {
    return false;
  }

  if (https) {
#if defined(TRAY_HOOKS_HAVE_OPENSSL) && TRAY_HOOKS_HAVE_OPENSSL
    return HttpsPostNdjson(host, port, path, body);
#else
    std::fprintf(stderr,
                 "[tray_hooks_apm] HTTPS requires OpenSSL (rebuild with OpenSSL "
                 "found); use TRAY_HOOKS_APM_FILE or http://\n");
    return false;
#endif
  }

  int fd = TcpConnect(host, port);
  if (fd < 0) {
    return false;
  }

  char hdr[512];
  std::snprintf(hdr, sizeof(hdr),
                "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: "
                "application/x-ndjson\r\nContent-Length: %zu\r\nConnection: "
                "close\r\n\r\n",
                path.c_str(), host.c_str(), body.size());
  std::string req = std::string(hdr) + body;
  bool ok = SendAll(fd, req.data(), req.size());
  close(fd);
  return ok;
}

#endif

void FlushLocked() {
  if (g_buf.empty()) {
    return;
  }
  std::vector<std::string> batch;
  batch.swap(g_buf);
  AppendFile(batch);
  if (!g_url.empty()) {
    std::string body;
    for (size_t i = 0; i < batch.size(); ++i) {
      if (i) {
        body.push_back('\n');
      }
      body += batch[i];
    }
    if (!HttpPostNdjson(g_url, body)) {
      std::fprintf(stderr, "[tray_hooks_apm] POST failed (%zu events)\n",
                   batch.size());
    }
  }
}

void Worker() {
  while (!g_stop.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(g_interval_ms));
    std::lock_guard<std::mutex> lock(g_mu);
    FlushLocked();
  }
  std::lock_guard<std::mutex> lock(g_mu);
  FlushLocked();
}

void OnEvent(const tray_hooks_event_t* ev, void* /*user*/) {
  if (!ev) {
    return;
  }
  std::string line = EventToJson(ev, g_include_stacks);
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_running) {
    return;
  }
  g_buf.push_back(line);
  if (static_cast<int>(g_buf.size()) >= g_batch_max) {
    FlushLocked();
  }
}

}  // namespace

extern "C" int tray_hooks_apm_start(const tray_hooks_apm_config_t* cfg) {
  if (!cfg) {
    return -1;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  if (g_running) {
    return 0;
  }
  g_file = cfg->file_path ? cfg->file_path : "";
  g_url = cfg->url ? cfg->url : "";
  if (g_file.empty() && g_url.empty()) {
    return -1;
  }
  g_interval_ms = cfg->interval_ms > 0 ? cfg->interval_ms : 2000;
  g_batch_max = cfg->batch_max > 0 ? cfg->batch_max : 64;
  g_include_stacks = cfg->include_stacks;
  g_buf.clear();
  g_stop = false;
  g_running = true;
  tray_hooks_collector_set_sink(OnEvent, 0);
  g_thread = std::thread(Worker);
  std::fprintf(stderr,
               "[tray_hooks_apm] started file=%s url=%s interval=%dms\n",
               g_file.empty() ? "-" : g_file.c_str(),
               g_url.empty() ? "-" : g_url.c_str(), g_interval_ms);
  return 0;
}

extern "C" int tray_hooks_apm_start_from_env(void) {
  tray_hooks_collector_apply_env_filter();
  tray_hooks_apm_config_t cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  cfg.file_path = std::getenv("TRAY_HOOKS_APM_FILE");
  cfg.url = std::getenv("TRAY_HOOKS_APM_URL");
  if ((!cfg.file_path || !*cfg.file_path) && (!cfg.url || !*cfg.url)) {
    return 0;
  }
  const char* iv = std::getenv("TRAY_HOOKS_APM_INTERVAL_MS");
  cfg.interval_ms = iv && *iv ? std::atoi(iv) : 2000;
  const char* bn = std::getenv("TRAY_HOOKS_APM_BATCH");
  cfg.batch_max = bn && *bn ? std::atoi(bn) : 64;
  const char* st = std::getenv("TRAY_HOOKS_APM_STACKS");
  cfg.include_stacks = !(st && (*st == '0' || *st == 'n' || *st == 'N'));
  return tray_hooks_apm_start(&cfg);
}

extern "C" void tray_hooks_apm_stop(void) {
  {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_running) {
      return;
    }
    g_stop = true;
  }
  if (g_thread.joinable()) {
    g_thread.join();
  }
  std::lock_guard<std::mutex> lock(g_mu);
  g_running = false;
  tray_hooks_collector_set_sink(0, 0);
}

extern "C" void tray_hooks_apm_flush(void) {
  std::lock_guard<std::mutex> lock(g_mu);
  FlushLocked();
}

extern "C" void tray_hooks_apm_emit_raw(const char* json_line) {
  if (!json_line || !*json_line) {
    return;
  }
  std::lock_guard<std::mutex> lock(g_mu);
  if (!g_running) {
    return;
  }
  g_buf.push_back(json_line);
  if (static_cast<int>(g_buf.size()) >= g_batch_max) {
    FlushLocked();
  }
}
