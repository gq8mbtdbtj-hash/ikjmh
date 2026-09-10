/**
 * @file win_webview_panel.cpp
 * @brief WebView2 面板实现。
 *
 * CUSTOMIZE: Navigate URL / token localStorage 键 / postMessage 格式。
 */

#include "tray_demo/platform/win/win_webview_panel.hpp"
#include "tray_demo/platform/win/win_panel_placement.hpp"

#ifdef _WIN32

#include <cstdio>
#include <cstring>
#include <sstream>

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace tray_demo {
namespace win {
namespace {

const wchar_t kWebViewClass[] = L"tray_demo.WebViewPanelWnd";

}  // namespace

WinWebViewPanel::WinWebViewPanel()
    : hwnd_(NULL),
      visible_(false),
      fullscreen_(false),
      ready_(false),
      creating_(false),
      missing_runtime_(false),
      pending_show_(false),
      pending_auth_(false),
      pending_clear_auth_(false),
      normal_w_(960),
      normal_h_(720),
      token_key_("tray_demo_token") {}

WinWebViewPanel::~WinWebViewPanel() { Destroy(); }

std::wstring WinWebViewPanel::Utf8ToWide(const std::string& utf8) const {
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

std::string WinWebViewPanel::EscapeJsString(const std::string& s) const {
  std::string out;
  out.reserve(s.size() + 8);
  for (std::size_t i = 0; i < s.size(); ++i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '\'':
        out += "\\'";
        break;
      case '\"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '<':
        out += "\\u003c";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          sprintf_s(buf, "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
        break;
    }
  }
  return out;
}

bool WinWebViewPanel::CreateHost() {
  if (hwnd_) {
    return true;
  }

  WNDCLASSEXW wc;
  std::memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &WinWebViewPanel::WndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = kWebViewClass;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      kWebViewClass,
      L"tray_demo WebView",
      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, normal_w_, normal_h_,
      NULL, NULL, GetModuleHandleW(NULL), this);
  if (!hwnd_) {
    last_error_ = "CreateWindowEx failed for WebView host";
    return false;
  }
  EnsureEnvironment();
  return true;
}

void WinWebViewPanel::Destroy() {
  if (controller_) {
    controller_->Close();
    controller_.Reset();
  }
  webview_.Reset();
  env_.Reset();
  ready_ = false;
  creating_ = false;
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
  visible_ = false;
}

void WinWebViewPanel::EnsureEnvironment() {
  if (ready_ || creating_ || !hwnd_) {
    return;
  }
  creating_ = true;
  missing_runtime_ = false;
  last_error_.clear();

  // CUSTOMIZE: 用户数据目录可改为 %APPDATA%/tray_demo/webview
  HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result) || !env) {
              creating_ = false;
              missing_runtime_ = true;
              last_error_ =
                  "WebView2 Runtime missing or environment create failed. "
                  "Install Evergreen Runtime from Microsoft.";
              if (hwnd_) {
                MessageBoxW(
                    hwnd_,
                    L"未检测到 WebView2 Runtime。\n"
                    L"请安装 Microsoft Edge WebView2 Evergreen Runtime。\n"
                    L"将自动回退到原生面板。",
                    L"tray_demo",
                    MB_ICONWARNING | MB_OK);
              }
              return S_OK;
            }
            env_ = env;
            env_->CreateCoreWebView2Controller(
                hwnd_,
                Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                    [this](HRESULT result2, ICoreWebView2Controller* controller) -> HRESULT {
                      creating_ = false;
                      if (FAILED(result2) || !controller) {
                        missing_runtime_ = true;
                        last_error_ = "CreateCoreWebView2Controller failed";
                        return S_OK;
                      }
                      controller_ = controller;
                      controller_->get_CoreWebView2(&webview_);
                      ready_ = (webview_ != nullptr);
                      ResizeWebView();
                      if (webview_) {
                        // 允许前端接收 chrome.webview.postMessage
                        webview_->add_NavigationCompleted(
                            Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                [this](ICoreWebView2*,
                                       ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT {
                                  ApplyPendingAuth();
                                  return S_OK;
                                })
                                .Get(),
                            nullptr);
                      }
                      NavigateIfReady();
                      if (pending_show_) {
                        pending_show_ = false;
                        ShowWindow(hwnd_, SW_SHOW);
                        SetForegroundWindow(hwnd_);
                        visible_ = true;
                        ApplyWindowPlacement();
                        ResizeWebView();
                      }
                      ApplyPendingAuth();
                      return S_OK;
                    })
                    .Get());
            return S_OK;
          })
          .Get());

  if (FAILED(hr)) {
    creating_ = false;
    missing_runtime_ = true;
    last_error_ = "CreateCoreWebView2EnvironmentWithOptions HRESULT failed";
  }
}

void WinWebViewPanel::Configure(const std::string& url, const std::string& token_key) {
  url_ = url;
  if (!token_key.empty()) {
    token_key_ = token_key;
  }
  NavigateIfReady();
}

void WinWebViewPanel::NavigateIfReady() {
  if (!ready_ || !webview_ || url_.empty()) {
    return;
  }
  const std::wstring wurl = Utf8ToWide(url_);
  webview_->Navigate(wurl.c_str());
}

void WinWebViewPanel::InjectAuth(const std::string& token, const std::string& username) {
  token_ = token;
  username_ = username;
  pending_auth_ = true;
  pending_clear_auth_ = false;
  ApplyPendingAuth();
}

void WinWebViewPanel::ClearAuth() {
  token_.clear();
  username_.clear();
  pending_clear_auth_ = true;
  pending_auth_ = false;
  ApplyPendingAuth();
}

void WinWebViewPanel::ApplyPendingAuth() {
  if (!ready_ || !webview_) {
    return;
  }
  if (pending_clear_auth_) {
    RunInjectScript(true);
    pending_clear_auth_ = false;
    return;
  }
  if (pending_auth_) {
    RunInjectScript(false);
    pending_auth_ = false;
  }
}

void WinWebViewPanel::RunInjectScript(bool clear) {
  if (!webview_) {
    return;
  }
  // =========================================================================
  // CUSTOMIZE: 改这里 — 与前端约定的 localStorage 键与 message JSON
  // =========================================================================
  std::ostringstream js;
  if (clear) {
    js << "(function(){try{"
       << "localStorage.removeItem('" << EscapeJsString(token_key_) << "');"
       << "localStorage.removeItem('" << EscapeJsString(token_key_) << "_user');"
       << "if(window.chrome&&chrome.webview){"
       << "chrome.webview.postMessage({type:'auth_cleared'});"
       << "}"
       << "}catch(e){}})();";
  } else {
    js << "(function(){try{"
       << "localStorage.setItem('" << EscapeJsString(token_key_) << "','"
       << EscapeJsString(token_) << "');"
       << "localStorage.setItem('" << EscapeJsString(token_key_) << "_user','"
       << EscapeJsString(username_) << "');"
       << "if(window.chrome&&chrome.webview){"
       << "chrome.webview.postMessage({type:'auth',token:'" << EscapeJsString(token_)
       << "',username:'" << EscapeJsString(username_) << "'});"
       << "}"
       << "}catch(e){}})();";
  }
  const std::wstring wjs = Utf8ToWide(js.str());
  webview_->ExecuteScript(wjs.c_str(), nullptr);

  // PostWebMessageAsJson（部分页面更易监听）
  if (webview_) {
    ComPtr<ICoreWebView2_2> wv2;
    if (SUCCEEDED(webview_.As(&wv2)) && wv2) {
      // keep using ExecuteScript primarily; also try PostWebMessage via ICoreWebView2
    }
    std::ostringstream msg;
    if (clear) {
      msg << "{\"type\":\"auth_cleared\"}";
    } else {
      msg << "{\"type\":\"auth\",\"token\":\"" << EscapeJsString(token_)
          << "\",\"username\":\"" << EscapeJsString(username_) << "\"}";
    }
    const std::wstring wmsg = Utf8ToWide(msg.str());
    webview_->PostWebMessageAsJson(wmsg.c_str());
  }
}

bool WinWebViewPanel::IsReady() const { return ready_; }

bool WinWebViewPanel::FailedMissingRuntime() const { return missing_runtime_; }

std::string WinWebViewPanel::LastError() const { return last_error_; }

void WinWebViewPanel::Show() {
  if (!hwnd_ && !CreateHost()) {
    return;
  }
  EnsureEnvironment();
  ApplyWindowPlacement();
  if (!ready_) {
    pending_show_ = true;
    // 仍显示宿主窗，避免无反馈
    ShowWindow(hwnd_, SW_SHOW);
    visible_ = true;
    return;
  }
  NavigateIfReady();
  ApplyPendingAuth();
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  visible_ = true;
  ResizeWebView();
}

void WinWebViewPanel::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
  pending_show_ = false;
}

bool WinWebViewPanel::IsVisible() const { return visible_; }

bool WinWebViewPanel::IsFullscreen() const { return fullscreen_; }

void WinWebViewPanel::SetFullscreen(bool enabled) {
  fullscreen_ = enabled;
  if (!hwnd_) {
    return;
  }
  ApplyWindowPlacement();
  ResizeWebView();
}

void WinWebViewPanel::ApplyWindowPlacement() {
  if (!hwnd_) {
    return;
  }
  if (fullscreen_) {
    MONITORINFO mi;
    std::memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    HMONITOR mon = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
    if (!GetMonitorInfoW(mon, &mi)) {
      SystemParametersInfoW(SPI_GETWORKAREA, 0, &mi.rcWork, 0);
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_TOOLWINDOW);
    SetWindowPos(hwnd_, HWND_TOP, mi.rcWork.left, mi.rcWork.top,
                 mi.rcWork.right - mi.rcWork.left, mi.rcWork.bottom - mi.rcWork.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);
  } else {
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, normal_w_, normal_h_,
                 SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    PositionNearTray();
  }
}

void WinWebViewPanel::SetNotifyIconAnchor(HWND notify_hwnd, UINT notify_id) {
  tray_anchor_.hwnd = notify_hwnd;
  tray_anchor_.id = notify_id;
  tray_anchor_.valid = (notify_hwnd != NULL);
}

void WinWebViewPanel::PositionNearTray() {
  PositionPanelNearTray(hwnd_, tray_anchor_);
}

void WinWebViewPanel::ResizeWebView() {
  if (!controller_ || !hwnd_) {
    return;
  }
  RECT bounds;
  GetClientRect(hwnd_, &bounds);
  controller_->put_Bounds(bounds);
  controller_->put_IsVisible(visible_ ? TRUE : FALSE);
}

void WinWebViewPanel::RenderNavigation(const PageNavigator& navigator) {
  (void)navigator;
  ResizeWebView();
}

void WinWebViewPanel::InvalidateCurrentPage() {
  ApplyPendingAuth();
  ResizeWebView();
}

LRESULT CALLBACK WinWebViewPanel::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    WinWebViewPanel* self = reinterpret_cast<WinWebViewPanel*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  WinWebViewPanel* self =
      reinterpret_cast<WinWebViewPanel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  if (msg == WM_SIZE && self) {
    self->ResizeWebView();
    return 0;
  }
  if (msg == WM_CLOSE && self) {
    self->Hide();
    return 0;
  }
  if (msg == WM_DESTROY && self) {
    self->hwnd_ = NULL;
    self->visible_ = false;
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace win
}  // namespace tray_demo

#endif  // _WIN32
