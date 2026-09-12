/**
 * @file test_apm_https.cpp
 * @brief HTTPS APM：本地自签 TLS 服务收 POST。
 *
 * 依赖：python3 + openssl CLI；设 TRAY_HOOKS_APM_TLS_INSECURE=1。
 * POSIX 未链接 OpenSSL 时跳过（exit 0）。
 */

#include "tray_hooks/apm.h"
#include "tray_hooks/collector.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#if !(defined(TRAY_HOOKS_HAVE_OPENSSL) && TRAY_HOOKS_HAVE_OPENSSL)
#define TRAY_HOOKS_HTTPS_TEST_SKIP 1
#endif
#endif

namespace {

int g_fail = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_fail;                                                                \
    }                                                                          \
  } while (0)

std::string TmpDir() {
#if defined(_WIN32)
  char dir[MAX_PATH];
  GetTempPathA(MAX_PATH, dir);
  return std::string(dir);
#else
  return "/tmp/";
#endif
}

#if defined(_WIN32)
void SetEnv(const char* k, const char* v) {
  SetEnvironmentVariableA(k, v ? v : "");
}
#else
void SetEnv(const char* k, const char* v) {
  if (v) {
    setenv(k, v, 1);
  } else {
    unsetenv(k);
  }
}
#endif

int RunCmd(const std::string& cmd) { return std::system(cmd.c_str()); }

bool FileContains(const std::string& path, const char* needle) {
  FILE* fp = std::fopen(path.c_str(), "rb");
  if (!fp) {
    return false;
  }
  char buf[8192];
  size_t n = std::fread(buf, 1, sizeof(buf) - 1, fp);
  buf[n] = '\0';
  std::fclose(fp);
  return n > 0 && std::strstr(buf, needle) != 0;
}

}  // namespace

int main() {
  std::printf("=== test_apm_https ===\n");
#if defined(TRAY_HOOKS_HTTPS_TEST_SKIP)
  std::printf("SKIP: OpenSSL not linked in this build\n");
  return 0;
#else
  const std::string base = TmpDir() + "tray_apm_https_";
  const std::string cert = base + "cert.pem";
  const std::string key = base + "key.pem";
  const std::string body_out = base + "body.ndjson";
  const std::string srv_py = base + "srv.py";
  std::remove(body_out.c_str());

  {
    std::string gen =
        "openssl req -x509 -newkey rsa:2048 -keyout \"" + key + "\" -out \"" +
        cert +
        "\" -days 1 -nodes -subj \"/CN=localhost\" >/dev/null 2>&1";
    if (RunCmd(gen) != 0) {
      std::printf("SKIP: openssl CLI unavailable\n");
      return 0;
    }
  }

  {
    FILE* fp = std::fopen(srv_py.c_str(), "w");
    CHECK(fp != 0);
    if (fp) {
      std::fputs(
          "import ssl, sys, http.server\n"
          "port=int(sys.argv[1]); out=sys.argv[2]; cert=sys.argv[3]; "
          "key=sys.argv[4]\n"
          "class H(http.server.BaseHTTPRequestHandler):\n"
          "  def do_POST(self):\n"
          "    n=int(self.headers.get('Content-Length',0))\n"
          "    open(out,'wb').write(self.rfile.read(n))\n"
          "    self.send_response(204); self.end_headers()\n"
          "  def log_message(self,*a): pass\n"
          "httpd=http.server.HTTPServer(('127.0.0.1',port),H)\n"
          "ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)\n"
          "ctx.load_cert_chain(cert,key)\n"
          "httpd.socket=ctx.wrap_socket(httpd.socket, server_side=True)\n"
          "httpd.handle_request()\n",
          fp);
      std::fclose(fp);
    }
  }

  const int port = 18443;
#if defined(_WIN32)
  std::string start = "start /B python \"" + srv_py + "\" " +
                      std::to_string(port) + " \"" + body_out + "\" \"" +
                      cert + "\" \"" + key + "\"";
#else
  std::string start = "python3 \"" + srv_py + "\" " + std::to_string(port) +
                      " \"" + body_out + "\" \"" + cert + "\" \"" + key +
                      "\" >/dev/null 2>&1 &";
#endif
  if (RunCmd(start) != 0) {
    std::printf("SKIP: failed to spawn python TLS server\n");
    return 0;
  }
#if defined(_WIN32)
  Sleep(800);
#else
  usleep(800 * 1000);
#endif

  SetEnv("TRAY_HOOKS_APM_TLS_INSECURE", "1");
  tray_hooks_apm_config_t cfg;
  std::memset(&cfg, 0, sizeof(cfg));
  std::string url = "https://127.0.0.1:" + std::to_string(port) + "/ingest";
  cfg.url = url.c_str();
  cfg.interval_ms = 5000;
  cfg.batch_max = 4;
  cfg.include_stacks = 0;
  CHECK(tray_hooks_apm_start(&cfg) == 0);
  tray_hooks_collector_record("https_ut", NULL, 0, 42);
  tray_hooks_apm_emit_raw("{\"type\":\"https_unit\",\"ok\":1}");
  tray_hooks_apm_flush();
  tray_hooks_apm_stop();

#if defined(_WIN32)
  Sleep(500);
#else
  usleep(500 * 1000);
#endif

  CHECK(FileContains(body_out, "https_ut") ||
        FileContains(body_out, "https_unit"));
  if (FileContains(body_out, "https_ut") ||
      FileContains(body_out, "https_unit")) {
    std::printf("  HTTPS POST ok -> %s\n", body_out.c_str());
  }

  SetEnv("TRAY_HOOKS_APM_TLS_INSECURE", NULL);

  if (g_fail) {
    std::fprintf(stderr, "APM HTTPS FAILED: %d\n", g_fail);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
#endif
}
