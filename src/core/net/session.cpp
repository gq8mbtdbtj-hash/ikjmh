#include "tray_demo/net/session.hpp"

#include <cstdlib>
#include <sstream>

namespace tray_demo {

namespace {

// 极简从 {"token":"..."} 抠 token（demo 不引入 JSON 库）
std::string ExtractJsonStringField(const std::string& json, const std::string& key) {
  const std::string pat = "\"" + key + "\"";
  std::size_t p = json.find(pat);
  if (p == std::string::npos) {
    return std::string();
  }
  p = json.find(':', p + pat.size());
  if (p == std::string::npos) {
    return std::string();
  }
  p = json.find('"', p + 1);
  if (p == std::string::npos) {
    return std::string();
  }
  const std::size_t start = p + 1;
  std::size_t end = start;
  while (end < json.size()) {
    if (json[end] == '\\' && end + 1 < json.size()) {
      end += 2;
      continue;
    }
    if (json[end] == '"') {
      break;
    }
    ++end;
  }
  if (end >= json.size()) {
    return std::string();
  }
  return json.substr(start, end - start);
}

// 解析 JSON 数字字段（如 expires_in）；失败返回 0
int ExtractJsonIntField(const std::string& json, const std::string& key) {
  const std::string pat = "\"" + key + "\"";
  std::size_t p = json.find(pat);
  if (p == std::string::npos) {
    return 0;
  }
  p = json.find(':', p + pat.size());
  if (p == std::string::npos) {
    return 0;
  }
  ++p;
  while (p < json.size() && (json[p] == ' ' || json[p] == '\t')) {
    ++p;
  }
  if (p >= json.size()) {
    return 0;
  }
  char* end = 0;
  const long v = std::strtol(json.c_str() + static_cast<std::ptrdiff_t>(p), &end, 10);
  if (end == json.c_str() + static_cast<std::ptrdiff_t>(p)) {
    return 0;
  }
  if (v < 0 || v > 2147483647L) {
    return 0;
  }
  return static_cast<int>(v);
}

// 抠 "roles":[...] 原始数组片段（可选缓存）
std::string ExtractJsonArrayField(const std::string& json, const std::string& key) {
  const std::string pat = "\"" + key + "\"";
  std::size_t p = json.find(pat);
  if (p == std::string::npos) {
    return std::string();
  }
  p = json.find('[', p + pat.size());
  if (p == std::string::npos) {
    return std::string();
  }
  int depth = 0;
  for (std::size_t i = p; i < json.size(); ++i) {
    if (json[i] == '[') {
      ++depth;
    } else if (json[i] == ']') {
      --depth;
      if (depth == 0) {
        return json.substr(p, i - p + 1);
      }
    }
  }
  return std::string();
}

std::string EscapeJson(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (c == '\\' || c == '"') {
      o.push_back('\\');
    }
    o.push_back(c);
  }
  return o;
}

}  // namespace

SessionService::SessionService(AppSettings* settings)
    : settings_(settings), last_expires_in_(0) {
  status_text_ = "未登录";
}

void SessionService::SyncStatusFromSettings() {
  if (IsLoggedIn()) {
    status_text_ = "已登录: " + settings_->username;
  } else {
    status_text_ = "未登录";
  }
}

bool SessionService::IsLoggedIn() const {
  return settings_ && !settings_->auth_token.empty();
}

bool SessionService::Login(const std::string& user, const std::string& password,
                           std::string* err) {
  if (!settings_) {
    if (err) {
      *err = "no settings";
    }
    return false;
  }
  const std::string url =
      HttpClient::JoinUrl(settings_->http_base_url, "/api/v1/login");
  std::ostringstream body;
  body << "{\"username\":\"" << EscapeJson(user) << "\",\"password\":\""
       << EscapeJson(password) << "\"}";
  const HttpResponse resp = HttpClient::PostJson(url, body.str(), "");
  if (resp.status_code != 200) {
    if (err) {
      *err = resp.error.empty() ? ("HTTP " + std::to_string(static_cast<long long>(resp.status_code)) +
                                   " " + resp.body)
                                : resp.error;
    }
    status_text_ = "登录失败";
    return false;
  }
  // B4: 稳定字段 token / username / roles / expires_in（见 docs/api-contract.md）
  const std::string token = ExtractJsonStringField(resp.body, "token");
  if (token.empty()) {
    if (err) {
      *err = "response missing token";
    }
    status_text_ = "登录失败";
    return false;
  }
  std::string name = ExtractJsonStringField(resp.body, "username");
  if (name.empty()) {
    name = user;
  }
  settings_->username = name;
  settings_->auth_token = token;
  last_roles_json_ = ExtractJsonArrayField(resp.body, "roles");
  last_expires_in_ = ExtractJsonIntField(resp.body, "expires_in");
  status_text_ = "已登录: " + name;
  return true;
}

void SessionService::Logout() {
  if (settings_) {
    settings_->auth_token.clear();
  }
  last_config_json_.clear();
  last_roles_json_.clear();
  last_expires_in_ = 0;
  status_text_ = "未登录";
}

bool SessionService::FetchConfigJson(std::string* err) {
  if (!settings_ || settings_->auth_token.empty()) {
    if (err) {
      *err = "not logged in";
    }
    return false;
  }
  const std::string url =
      HttpClient::JoinUrl(settings_->http_base_url, "/api/v1/config.json");
  const HttpResponse resp = HttpClient::Get(url, settings_->auth_token);
  if (resp.status_code != 200) {
    if (err) {
      *err = resp.error.empty() ? ("HTTP " + std::to_string(static_cast<long long>(resp.status_code)))
                                : resp.error;
    }
    return false;
  }
  last_config_json_ = resp.body;
  status_text_ = "已同步配置 (" + settings_->username + ")";
  return true;
}

}  // namespace tray_demo
