#include "tray_demo/config/settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace tray_demo {

AppSettings AppSettings::DefaultSettings() {
  AppSettings s;
  // =========================================================================
  // CUSTOMIZE: 改这里 — 默认 HTTP 服务根地址
  // =========================================================================
  s.http_base_url = "http://127.0.0.1:8080";
  s.config_json_path = "/api/v1/config.json";
  s.update_manifest_path = "/api/v1/update/manifest";
  // CUSTOMIZE: 面板全屏为可选功能；设 panel_allow_fullscreen=false 可隐藏菜单项
  s.panel_allow_fullscreen = true;
  s.panel_fullscreen = false;
  // CUSTOMIZE: panel_ui=webview 启用远程站点面板；改 panel_webview_url
  s.panel_ui = "native";
  s.panel_webview_url = "https://example.com";
  s.panel_webview_token_key = "tray_demo_token";
  return s;
}

namespace {

#ifndef _WIN32
void EnsureParentDir(const std::string& file_path) {
  const std::size_t slash = file_path.find_last_of('/');
  if (slash == std::string::npos || slash == 0) {
    return;
  }
  const std::string dir = file_path.substr(0, slash);
  // mkdir -p 简化：逐级创建
  std::string cur;
  for (std::size_t i = 0; i < dir.size(); ++i) {
    cur.push_back(dir[i]);
    if (dir[i] == '/' || i + 1 == dir.size()) {
      if (cur.size() > 1) {
        ::mkdir(cur.c_str(), 0755);
      }
    }
  }
}
#endif

}  // namespace

std::string SettingsStore::DefaultFilePath() {
#ifdef _WIN32
  wchar_t appdata[MAX_PATH] = {0};
  if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
    return "settings.ini";
  }
  std::wstring dir = std::wstring(appdata) + L"\\tray_demo";
  CreateDirectoryW(dir.c_str(), NULL);
  const std::wstring file = dir + L"\\settings.ini";
  const int n = WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, NULL, 0, NULL, NULL);
  std::string out(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
  if (n > 0) {
    WideCharToMultiByte(CP_UTF8, 0, file.c_str(), -1, &out[0], n, NULL, NULL);
  }
  return out;
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  std::string base;
  if (xdg && *xdg) {
    base = std::string(xdg) + "/tray_demo";
  } else {
    const char* home = std::getenv("HOME");
    if (!home) {
      return "settings.ini";
    }
    base = std::string(home) + "/.config/tray_demo";
  }
  const std::string file = base + "/settings.ini";
  EnsureParentDir(file);
  return file;
#endif
}

namespace {

std::string Trim(const std::string& s) {
  std::size_t b = 0;
  while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) {
    ++b;
  }
  std::size_t e = s.size();
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) {
    --e;
  }
  return s.substr(b, e - b);
}

bool ParseBool(const std::string& val, bool def) {
  if (val == "1" || val == "true" || val == "True" || val == "yes" || val == "on") {
    return true;
  }
  if (val == "0" || val == "false" || val == "False" || val == "no" || val == "off") {
    return false;
  }
  return def;
}

void ApplyKey(AppSettings* s, const std::string& key, const std::string& val) {
  if (key == "http_base_url") {
    s->http_base_url = val;
  } else if (key == "username") {
    s->username = val;
  } else if (key == "auth_token") {
    s->auth_token = val;
  } else if (key == "config_json_path") {
    s->config_json_path = val;
  } else if (key == "update_manifest_path") {
    s->update_manifest_path = val;
  } else if (key == "panel_allow_fullscreen") {
    s->panel_allow_fullscreen = ParseBool(val, s->panel_allow_fullscreen);
  } else if (key == "panel_fullscreen") {
    s->panel_fullscreen = ParseBool(val, s->panel_fullscreen);
  } else if (key == "panel_ui") {
    s->panel_ui = val;
  } else if (key == "panel_webview_url") {
    s->panel_webview_url = val;
  } else if (key == "panel_webview_token_key") {
    s->panel_webview_token_key = val;
  }
}

}  // namespace

bool SettingsStore::Load(const std::string& path, AppSettings* out) {
  if (!out) {
    return false;
  }
  *out = AppSettings::DefaultSettings();
  std::ifstream in(path.c_str());
  if (!in) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }
    const std::size_t eq = line.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    ApplyKey(out, Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
  }
  return true;
}

bool SettingsStore::Save(const std::string& path, const AppSettings& in) {
#ifndef _WIN32
  EnsureParentDir(path);
#endif
  std::ofstream out(path.c_str(), std::ios::trunc);
  if (!out) {
    return false;
  }
  out << "# tray_demo settings — CUSTOMIZE http_base_url / panel_ui / panel_webview_url\n";
  out << "http_base_url=" << in.http_base_url << "\n";
  out << "username=" << in.username << "\n";
  out << "auth_token=" << in.auth_token << "\n";
  out << "config_json_path=" << in.config_json_path << "\n";
  out << "update_manifest_path=" << in.update_manifest_path << "\n";
  out << "panel_allow_fullscreen=" << (in.panel_allow_fullscreen ? "1" : "0") << "\n";
  out << "panel_fullscreen=" << (in.panel_fullscreen ? "1" : "0") << "\n";
  out << "panel_ui=" << in.panel_ui << "\n";
  out << "panel_webview_url=" << in.panel_webview_url << "\n";
  out << "panel_webview_token_key=" << in.panel_webview_token_key << "\n";
  return true;
}

}  // namespace tray_demo
