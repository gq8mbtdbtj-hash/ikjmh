#include "tray_demo/platform/win/login_dialog.hpp"

#ifdef _WIN32

#include <string>

namespace tray_demo {
namespace win {

namespace {

enum {
  kIdUser = 1001,
  kIdPass = 1002,
  kIdOk = 1003,
  kIdCancel = 1004,
  kIdHint = 1005
};

struct LoginDlgState {
  SessionService* session;
  bool ok;
  LoginDlgState() : session(0), ok(false) {}
};

std::string GetWindowTextUtf8(HWND hwnd) {
  const int n = GetWindowTextLengthW(hwnd);
  if (n <= 0) {
    return std::string();
  }
  std::wstring w(static_cast<std::size_t>(n) + 1, L'\0');
  GetWindowTextW(hwnd, &w[0], n + 1);
  w.resize(static_cast<std::size_t>(n));
  const int bytes =
      WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, NULL, 0, NULL, NULL);
  std::string out(static_cast<std::size_t>(bytes > 0 ? bytes - 1 : 0), '\0');
  if (bytes > 0) {
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &out[0], bytes, NULL, NULL);
  }
  return out;
}

LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  LoginDlgState* st =
      reinterpret_cast<LoginDlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  if (msg == WM_CREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    st = reinterpret_cast<LoginDlgState*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));

    // =====================================================================
    // CUSTOMIZE: 改这里 — 登录窗口完整界面示例（控件位置/文案/尺寸）
    // 坐标为客户区像素；可改字体、加 Logo、记住密码复选框等。
    // =====================================================================
    const HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

    HWND h = CreateWindowW(L"STATIC", L"服务器账号（LDAP）",
                           WS_CHILD | WS_VISIBLE, 20, 18, 260, 18, hwnd, NULL,
                           GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    h = CreateWindowW(L"STATIC", L"用户名", WS_CHILD | WS_VISIBLE, 20, 48, 80, 18,
                      hwnd, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 44, 220, 24,
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdUser)),
                        GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    if (st && st->session && st->session->settings()) {
      const std::string& u = st->session->settings()->username;
      if (!u.empty()) {
        const int wn = MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, NULL, 0);
        std::wstring wu(static_cast<std::size_t>(wn > 0 ? wn - 1 : 0), L'\0');
        if (wn > 0) {
          MultiByteToWideChar(CP_UTF8, 0, u.c_str(), -1, &wu[0], wn);
          SetWindowTextW(h, wu.c_str());
        }
      }
    }

    h = CreateWindowW(L"STATIC", L"密码", WS_CHILD | WS_VISIBLE, 20, 84, 80, 18,
                      hwnd, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
                        100, 80, 220, 24, hwnd,
                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdPass)),
                        GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    h = CreateWindowW(L"BUTTON", L"登录", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      100, 130, 100, 28, hwnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdOk)),
                      GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    h = CreateWindowW(L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE, 220, 130, 100, 28,
                      hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdCancel)),
                      GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    h = CreateWindowW(L"STATIC", L"校验由服务端 LDAP 完成", WS_CHILD | WS_VISIBLE,
                      20, 170, 300, 18, hwnd,
                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdHint)),
                      GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    return 0;
  }

  if (msg == WM_COMMAND) {
    const int id = LOWORD(wparam);
    if (id == kIdCancel) {
      DestroyWindow(hwnd);
      return 0;
    }
    if (id == kIdOk && st && st->session) {
      const std::string user = GetWindowTextUtf8(GetDlgItem(hwnd, kIdUser));
      const std::string pass = GetWindowTextUtf8(GetDlgItem(hwnd, kIdPass));
      std::string err;
      if (st->session->Login(user, pass, &err)) {
        st->ok = true;
        DestroyWindow(hwnd);
      } else {
        const std::wstring werr = [&err]() {
          const int n = MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, NULL, 0);
          std::wstring o(static_cast<std::size_t>(n > 0 ? n - 1 : 0), L'\0');
          if (n > 0) {
            MultiByteToWideChar(CP_UTF8, 0, err.c_str(), -1, &o[0], n);
          }
          return o;
        }();
        MessageBoxW(hwnd, werr.empty() ? L"登录失败" : werr.c_str(), L"登录",
                    MB_ICONWARNING);
        SetWindowTextW(GetDlgItem(hwnd, kIdHint), L"登录失败，请重试");
      }
      return 0;
    }
  }

  if (msg == WM_CLOSE) {
    DestroyWindow(hwnd);
    return 0;
  }
  if (msg == WM_DESTROY) {
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

}  // namespace

bool ShowLoginDialog(HWND owner, SessionService* session) {
  if (!session) {
    return false;
  }

  static bool registered = false;
  if (!registered) {
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &LoginWndProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"tray_demo.LoginDlg";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);
    registered = true;
  }

  LoginDlgState state;
  state.session = session;
  state.ok = false;

  HWND hwnd = CreateWindowExW(
      WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"tray_demo.LoginDlg", L"登录 — tray_demo",
      WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT, 360, 240,
      owner, NULL, GetModuleHandleW(NULL), &state);
  if (!hwnd) {
    return false;
  }

  // 居中于屏幕
  RECT rc;
  GetWindowRect(hwnd, &rc);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  const int sw = GetSystemMetrics(SM_CXSCREEN);
  const int sh = GetSystemMetrics(SM_CYSCREEN);
  SetWindowPos(hwnd, HWND_TOP, (sw - w) / 2, (sh - h) / 2, 0, 0, SWP_NOSIZE);
  ShowWindow(hwnd, SW_SHOW);
  EnableWindow(owner, FALSE);

  MSG msg;
  while (IsWindow(hwnd)) {
    const BOOL gm = GetMessageW(&msg, NULL, 0, 0);
    if (gm <= 0) {
      break;
    }
    if (!IsDialogMessageW(hwnd, &msg)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }

  EnableWindow(owner, TRUE);
  if (owner) {
    SetForegroundWindow(owner);
  }
  return state.ok;
}

}  // namespace win
}  // namespace tray_demo

#endif
