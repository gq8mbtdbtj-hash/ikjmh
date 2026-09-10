#pragma once

/**
 * @file http_client.hpp
 * @brief 简易 HTTP 客户端（Win: WinHTTP；POSIX: socket + 可选 OpenSSL HTTPS）。
 *
 * @customize 登录 / 拉配置 JSON / 下载均走此类；基址来自 AppSettings::http_base_url。
 * POSIX HTTPS 需链接 OpenSSL（CMake: TRAY_DEMO_USE_OPENSSL / find_package OpenSSL）。
 */

#include <string>

namespace tray_demo {

struct HttpResponse {
  int status_code;   ///< HTTP 状态，0 表示传输失败
  std::string body;
  std::string error; ///< 可读错误
};

class HttpClient {
public:
  /**
   * @brief GET
   * @param url 完整 URL
   * @param bearer_token 可空；非空则加 Authorization: Bearer
   */
  static HttpResponse Get(const std::string& url, const std::string& bearer_token);

  /**
   * @brief POST JSON
   */
  static HttpResponse PostJson(const std::string& url,
                               const std::string& json_body,
                               const std::string& bearer_token);

  /// @brief 拼接 base + path（处理斜杠）
  static std::string JoinUrl(const std::string& base, const std::string& path);
};

}  // namespace tray_demo
