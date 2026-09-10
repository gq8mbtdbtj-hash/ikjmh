#include "tray_demo/platform/win/win_platform.hpp"
#include "tray_demo/platform/win/login_dialog.hpp"
#include "tray_demo/platform/win/win_panel_placement.hpp"
#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/util/fuzzy_match.hpp"

#ifdef _WIN32

#include <commctrl.h>
#include <shellapi.h>

#include <cstring>
#include <string>
#include <vector>

namespace tray_demo {
namespace win {

namespace {

const wchar_t kTrayClass[] = L"tray_demo.TrayWnd";
const wchar_t kPanelClass[] = L"tray_demo.PanelWnd";
const UINT kTrayCallbackMsg = WM_APP + 1;
const UINT_PTR kTrayIconId = 1;

std::wstring Utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return std::wstring();
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, NULL, 0);
  if (n <= 0) {
    return std::wstring();
  }
  std::wstring out(static_cast<std::size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], n);
  return out;
}

WinTrayHost* TrayFromHwnd(HWND hwnd) {
  return reinterpret_cast<WinTrayHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

}  // namespace

// ---- WinTrayHost ----

WinTrayHost::WinTrayHost()
    : hwnd_(NULL), handler_(0), icon_added_(false) {
  std::memset(&nid_, 0, sizeof(nid_));
}

WinTrayHost::~WinTrayHost() { Destroy(); }

bool WinTrayHost::Create() {
  if (hwnd_) {
    return true;
  }

  WNDCLASSEXW wc;
  std::memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &WinTrayHost::WndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = kTrayClass;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClassExW(&wc);

  hwnd_ = CreateWindowExW(
      0, kTrayClass, L"tray_demo",
      WS_OVERLAPPED, 0, 0, 0, 0,
      NULL, NULL, GetModuleHandleW(NULL), this);
  if (!hwnd_) {
    return false;
  }

  return AddNotifyIcon();
}

void WinTrayHost::Destroy() {
  RemoveNotifyIcon();
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void WinTrayHost::SetEventHandler(ITrayEventHandler* handler) {
  handler_ = handler;
}

void WinTrayHost::SetTooltip(const std::string& text) {
  tooltip_ = Utf8ToWide(text);
  if (!icon_added_ || !hwnd_) {
    return;
  }
  nid_.uFlags = NIF_TIP;
  wcsncpy_s(nid_.szTip, _countof(nid_.szTip), tooltip_.c_str(), _TRUNCATE);
  Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void WinTrayHost::SetIcon(const std::string& /*icon_key*/) {
  // demo：固定用应用图标；后续可按 key 加载
  if (!icon_added_) {
    return;
  }
  nid_.uFlags = NIF_ICON;
  nid_.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));  // IDI_APPLICATION
  Shell_NotifyIconW(NIM_MODIFY, &nid_);
}

void WinTrayHost::RequestQuit() {
  if (hwnd_) {
    PostMessageW(hwnd_, WM_CLOSE, 0, 0);
  } else {
    PostQuitMessage(0);
  }
}

bool WinTrayHost::AddNotifyIcon() {
  std::memset(&nid_, 0, sizeof(nid_));
  nid_.cbSize = sizeof(nid_);
  nid_.hWnd = hwnd_;
  nid_.uID = static_cast<UINT>(kTrayIconId);
  nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid_.uCallbackMessage = kTrayCallbackMsg;
  nid_.hIcon = LoadIconW(NULL, MAKEINTRESOURCEW(32512));  // IDI_APPLICATION
  const wchar_t* tip = tooltip_.empty() ? L"tray_demo" : tooltip_.c_str();
  wcsncpy_s(nid_.szTip, _countof(nid_.szTip), tip, _TRUNCATE);

  if (!Shell_NotifyIconW(NIM_ADD, &nid_)) {
    return false;
  }
  icon_added_ = true;

  // Vista+：设置版本，获得更完整点击通知
  nid_.uVersion = NOTIFYICON_VERSION_4;
  Shell_NotifyIconW(NIM_SETVERSION, &nid_);
  return true;
}

void WinTrayHost::RemoveNotifyIcon() {
  if (!icon_added_) {
    return;
  }
  Shell_NotifyIconW(NIM_DELETE, &nid_);
  icon_added_ = false;
}

LRESULT CALLBACK WinTrayHost::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    WinTrayHost* self = reinterpret_cast<WinTrayHost*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  WinTrayHost* self = TrayFromHwnd(hwnd);
  if (self) {
    return self->HandleMessage(hwnd, msg, wparam, lparam);
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT WinTrayHost::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == kTrayCallbackMsg) {
    const UINT event = LOWORD(lparam);
    if (event == WM_LBUTTONUP || event == NIN_SELECT) {
      if (handler_) {
        handler_->OnTrayLeftClick();
      }
    } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
      if (handler_) {
        handler_->OnTrayRightClick();
      }
    } else if (event == WM_LBUTTONDBLCLK) {
      if (handler_) {
        handler_->OnTrayDoubleClick();
      }
    }
    return 0;
  }

  if (msg == WM_CLOSE) {
    RemoveNotifyIcon();
    DestroyWindow(hwnd);
    return 0;
  }
  if (msg == WM_DESTROY) {
    hwnd_ = NULL;
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ---- WinMenuRenderer ----

WinMenuRenderer::WinMenuRenderer()
    : owner_(NULL), handler_(0) {}

void WinMenuRenderer::ClearMaps() { id_by_cmd_.clear(); }

void WinMenuRenderer::MapCommand(UINT cmd, const std::string& action_id) {
  if (cmd == 0) {
    return;
  }
  const std::size_t idx = static_cast<std::size_t>(cmd - 1);
  if (id_by_cmd_.size() <= idx) {
    id_by_cmd_.resize(idx + 1);
  }
  id_by_cmd_[idx] = action_id;
}

HMENU WinMenuRenderer::BuildMenu(const std::vector<MenuItem>& items, UINT* next_id) {
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return NULL;
  }

  for (std::size_t i = 0; i < items.size(); ++i) {
    const MenuItem& it = items[i];
    if (it.kind == kMenuSeparator) {
      AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
      continue;
    }

    const std::wstring title = Utf8ToWide(it.title);
    UINT flags = MF_STRING;
    if (!it.enabled) {
      flags |= MF_GRAYED;
    }
    if (it.checked) {
      flags |= MF_CHECKED;
    }

    if (it.kind == kMenuSubMenu) {
      HMENU sub = BuildMenu(it.children, next_id);
      AppendMenuW(menu, flags | MF_POPUP, reinterpret_cast<UINT_PTR>(sub),
                  title.c_str());
    } else {
      const UINT cmd = (*next_id)++;
      MapCommand(cmd, it.id);
      AppendMenuW(menu, flags, cmd, title.c_str());
    }
  }
  return menu;
}

void WinMenuRenderer::PresentContextMenu(const MenuModel& model,
                                         IMenuActionHandler* handler) {
  handler_ = handler;
  ClearMaps();

  HWND owner = owner_;
  if (!owner) {
    owner = GetForegroundWindow();
  }

  UINT next_id = 1;
  HMENU menu = BuildMenu(model.items(), &next_id);
  if (!menu) {
    return;
  }

  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(owner);
  const UINT cmd = TrackPopupMenu(
      menu,
      TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
      pt.x, pt.y, 0, owner, NULL);
  PostMessageW(owner, WM_NULL, 0, 0);
  DestroyMenu(menu);

  if (cmd != 0 && handler_ && cmd <= id_by_cmd_.size()) {
    const std::string& action = id_by_cmd_[cmd - 1];
    if (!action.empty()) {
      handler_->OnMenuAction(action);
    }
  }
}

void WinMenuRenderer::Dismiss() {}

// ---- WinPanelRenderer ----

WinPanelRenderer::WinPanelRenderer()
    : hwnd_(NULL),
      search_(NULL),
      filter_(NULL),
      list_(NULL),
      hint_(NULL),
      visible_(false),
      fullscreen_(false),
      normal_w_(480),
      normal_h_(520),
      list_model_(0) {
  title_ = L"tray_demo";
  line1_ = L"";
  line2_ = L"";
  body_ = L"";
}

WinPanelRenderer::~WinPanelRenderer() { Destroy(); }

std::wstring WinPanelRenderer::Utf8ToWide(const std::string& utf8) const {
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

std::string WinPanelRenderer::WideToUtf8(const std::wstring& wide) const {
  if (wide.empty()) {
    return std::string();
  }
  const int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, NULL, 0, NULL, NULL);
  std::string out(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
  if (n > 0) {
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &out[0], n, NULL, NULL);
  }
  return out;
}

bool WinPanelRenderer::Create() {
  if (hwnd_) {
    return true;
  }

  WNDCLASSEXW wc;
  std::memset(&wc, 0, sizeof(wc));
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = &WinPanelRenderer::PanelWndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.lpszClassName = kPanelClass;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  RegisterClassExW(&wc);

  // CUSTOMIZE: 面板默认宽高（窗口模式）
  hwnd_ = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      kPanelClass,
      L"tray_demo",
      WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, normal_w_, normal_h_,
      NULL, NULL, GetModuleHandleW(NULL), this);
  if (!hwnd_) {
    return false;
  }
  EnsureChildControls();
  LayoutControls();
  return true;
}

void WinPanelRenderer::Destroy() {
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
  search_ = filter_ = list_ = hint_ = NULL;
  visible_ = false;
}

void WinPanelRenderer::EnsureChildControls() {
  if (!hwnd_ || search_) {
    return;
  }
  const HINSTANCE inst = GetModuleHandleW(NULL);
  hint_ = CreateWindowExW(0, L"STATIC", L"模糊搜索 / 分类过滤",
                          WS_CHILD | WS_VISIBLE | SS_LEFT,
                          0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(kIdHint), inst, NULL);
  search_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(kIdSearch), inst, NULL);
  filter_ = CreateWindowExW(0, L"COMBOBOX", L"",
                            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                            0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(kIdFilter), inst, NULL);
  list_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                          WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                          0, 0, 0, 0, hwnd_, reinterpret_cast<HMENU>(kIdList), inst, NULL);

  HFONT ui = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  SendMessageW(hint_, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
  SendMessageW(search_, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
  SendMessageW(filter_, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);
  SendMessageW(list_, WM_SETFONT, reinterpret_cast<WPARAM>(ui), TRUE);

  SetWindowTextW(search_, L"");
#ifndef ECM_FIRST
#define ECM_FIRST 0x1500
#endif
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER (ECM_FIRST + 1)
#endif
  SendMessageW(search_, EM_SETCUEBANNER, TRUE,
               reinterpret_cast<LPARAM>(L"输入关键字过滤列表…"));
}

void WinPanelRenderer::LayoutControls() {
  if (!hwnd_ || !search_) {
    return;
  }
  RECT client;
  GetClientRect(hwnd_, &client);
  const int pad = 12;
  const int header = 96;  // 给 Paint 标题区留空
  const int row_h = 24;
  const int gap = 8;
  int y = header;
  int x = pad;
  int w = client.right - pad * 2;
  if (w < 80) {
    w = 80;
  }

  MoveWindow(hint_, x, y, w, 18, TRUE);
  y += 20;
  const int search_w = (w * 58) / 100;
  const int filter_w = w - search_w - gap;
  MoveWindow(search_, x, y, search_w, row_h, TRUE);
  MoveWindow(filter_, x + search_w + gap, y, filter_w, 200, TRUE);
  y += row_h + gap;
  const int list_h = client.bottom - y - pad;
  MoveWindow(list_, x, y, w, list_h > 40 ? list_h : 40, TRUE);
}

void WinPanelRenderer::RebuildFilterCombo() {
  if (!filter_) {
    return;
  }
  const int sel = static_cast<int>(SendMessageW(filter_, CB_GETCURSEL, 0, 0));
  std::wstring prev;
  if (sel >= 0) {
    const int len = static_cast<int>(SendMessageW(filter_, CB_GETLBTEXTLEN, sel, 0));
    if (len > 0) {
      prev.assign(static_cast<std::size_t>(len), L'\0');
      SendMessageW(filter_, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(&prev[0]));
    }
  }
  SendMessageW(filter_, CB_RESETCONTENT, 0, 0);
  const std::vector<std::string> cats = CollectCategories(all_items_);
  int restore = 0;
  for (std::size_t i = 0; i < cats.size(); ++i) {
    const std::wstring w = Utf8ToWide(cats[i]);
    SendMessageW(filter_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
    if (!prev.empty() && w == prev) {
      restore = static_cast<int>(i);
    }
  }
  SendMessageW(filter_, CB_SETCURSEL, restore, 0);
}

void WinPanelRenderer::ApplyFilter() {
  if (!list_) {
    return;
  }
  filtered_index_.clear();
  SendMessageW(list_, LB_RESETCONTENT, 0, 0);

  std::string query;
  if (search_) {
    const int n = GetWindowTextLengthW(search_);
    std::wstring buf(static_cast<std::size_t>(n), L'\0');
    if (n > 0) {
      GetWindowTextW(search_, &buf[0], n + 1);
    }
    query = WideToUtf8(buf);
  }

  std::string category;
  if (filter_) {
    const int sel = static_cast<int>(SendMessageW(filter_, CB_GETCURSEL, 0, 0));
    if (sel > 0) {
      const int len = static_cast<int>(SendMessageW(filter_, CB_GETLBTEXTLEN, sel, 0));
      std::wstring w(static_cast<std::size_t>(len > 0 ? len : 0), L'\0');
      if (len > 0) {
        SendMessageW(filter_, CB_GETLBTEXT, sel, reinterpret_cast<LPARAM>(&w[0]));
        category = WideToUtf8(w);
      }
    }
  }

  struct Scored {
    int index;
    int score;
  };
  std::vector<Scored> scored;
  for (std::size_t i = 0; i < all_items_.size(); ++i) {
    const PanelListItem& it = all_items_[i];
    if (!category.empty() && it.category != category) {
      continue;
    }
    const std::string hay = it.name + " " + it.detail + " " + it.id + " " + it.category;
    const int score = FuzzyScore(hay, query);
    if (score < 0) {
      continue;
    }
    Scored s;
    s.index = static_cast<int>(i);
    s.score = score;
    scored.push_back(s);
  }
  for (std::size_t a = 0; a < scored.size(); ++a) {
    for (std::size_t b = a + 1; b < scored.size(); ++b) {
      if (scored[b].score > scored[a].score) {
        Scored tmp = scored[a];
        scored[a] = scored[b];
        scored[b] = tmp;
      }
    }
  }
  for (std::size_t i = 0; i < scored.size(); ++i) {
    filtered_index_.push_back(scored[i].index);
    const PanelListItem& it = all_items_[static_cast<std::size_t>(scored[i].index)];
    std::string line = "[" + it.category + "] " + it.name;
    if (!it.detail.empty()) {
      line += "  —  " + it.detail;
    }
    const std::wstring w = Utf8ToWide(line);
    SendMessageW(list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
  }

  if (hint_) {
    wchar_t tip[128];
    wsprintfW(tip, L"模糊搜索 / 分类过滤 — 显示 %d / %d",
              static_cast<int>(filtered_index_.size()),
              static_cast<int>(all_items_.size()));
    SetWindowTextW(hint_, tip);
  }
}

void WinPanelRenderer::Show() {
  if (!hwnd_) {
    return;
  }
  EnsureChildControls();
  ApplyWindowPlacement();
  LayoutControls();
  if (list_model_) {
    ReloadList();
  } else {
    ApplyFilter();
  }
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  visible_ = true;
}

void WinPanelRenderer::Hide() {
  if (!hwnd_) {
    return;
  }
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

bool WinPanelRenderer::IsVisible() const { return visible_; }

bool WinPanelRenderer::IsFullscreen() const { return fullscreen_; }

void WinPanelRenderer::SetFullscreen(bool enabled) {
  fullscreen_ = enabled;
  if (!hwnd_) {
    return;
  }
  ApplyWindowPlacement();
  LayoutControls();
}

void WinPanelRenderer::ApplyWindowPlacement() {
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
    SetWindowPos(hwnd_, HWND_TOP,
                 mi.rcWork.left, mi.rcWork.top,
                 mi.rcWork.right - mi.rcWork.left,
                 mi.rcWork.bottom - mi.rcWork.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);
  } else {
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, normal_w_, normal_h_,
                 SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    PositionNearTray();
  }
}

void WinPanelRenderer::SetNotifyIconAnchor(HWND notify_hwnd, UINT notify_id) {
  tray_anchor_.hwnd = notify_hwnd;
  tray_anchor_.id = notify_id;
  tray_anchor_.valid = (notify_hwnd != NULL);
}

void WinPanelRenderer::SetHeader(const std::string& title,
                                 const std::string& line1,
                                 const std::string& line2,
                                 const std::string& body) {
  title_ = Utf8ToWide(title);
  line1_ = Utf8ToWide(line1);
  line2_ = Utf8ToWide(line2);
  body_ = Utf8ToWide(body);
  if (hwnd_) {
    SetWindowTextW(hwnd_, title_.c_str());
    InvalidateRect(hwnd_, NULL, TRUE);
  }
}

void WinPanelRenderer::BindListModel(IListPanelModel* model) {
  list_model_ = model;
}

void WinPanelRenderer::ReloadList() {
  all_items_.clear();
  if (list_model_) {
    list_model_->CopyItems(&all_items_);
  }
  EnsureChildControls();
  RebuildFilterCombo();
  ApplyFilter();
  if (hwnd_) {
    LayoutControls();
    InvalidateRect(hwnd_, NULL, TRUE);
  }
}

void WinPanelRenderer::SetListItems(const std::vector<PanelListItem>& items) {
  list_model_ = 0;
  all_items_ = items;
  EnsureChildControls();
  RebuildFilterCombo();
  ApplyFilter();
  if (hwnd_) {
    LayoutControls();
    InvalidateRect(hwnd_, NULL, TRUE);
  }
}

void WinPanelRenderer::RenderNavigation(const PageNavigator& navigator) {
  (void)navigator;
  if (hwnd_ && visible_) {
    LayoutControls();
    InvalidateRect(hwnd_, NULL, TRUE);
  }
}

void WinPanelRenderer::InvalidateCurrentPage() {
  if (hwnd_) {
    LayoutControls();
    if (list_model_) {
      ReloadList();
    } else {
      ApplyFilter();
    }
    InvalidateRect(hwnd_, NULL, TRUE);
  }
}

void WinPanelRenderer::PositionNearTray() {
  PositionPanelNearTray(hwnd_, tray_anchor_);
}

void WinPanelRenderer::Paint(HDC hdc) {
  // =========================================================================
  // CUSTOMIZE: 改这里 — 面板顶部状态区绘制；列表控件在下方
  // =========================================================================
  RECT client;
  GetClientRect(hwnd_, &client);
  FillRect(hdc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
  RECT header = client;
  header.bottom = 96;

  SetBkMode(hdc, TRANSPARENT);
  const HFONT title_font =
      CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
  HFONT old = reinterpret_cast<HFONT>(SelectObject(hdc, title_font));
  SetTextColor(hdc, RGB(20, 20, 20));
  RECT r = header;
  r.left += 12;
  r.top += 8;
  r.right -= 12;
  DrawTextW(hdc, title_.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);

  SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
  SetTextColor(hdc, RGB(60, 60, 60));
  r.top += 28;
  DrawTextW(hdc, line1_.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
  r.top += 20;
  DrawTextW(hdc, line2_.c_str(), -1, &r, DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
  if (fullscreen_) {
    r.top += 20;
    DrawTextW(hdc, L"模式: 全屏（右键菜单可退出）", -1, &r,
              DT_LEFT | DT_TOP | DT_SINGLELINE);
  }

  SelectObject(hdc, old);
  DeleteObject(title_font);
}

LRESULT CALLBACK WinPanelRenderer::PanelWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (msg == WM_NCCREATE) {
    CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
    WinPanelRenderer* self = reinterpret_cast<WinPanelRenderer*>(cs->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }

  WinPanelRenderer* self =
      reinterpret_cast<WinPanelRenderer*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

  if (msg == WM_PAINT && self) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    self->Paint(hdc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_SIZE && self) {
    self->LayoutControls();
    return 0;
  }
  if (msg == WM_COMMAND && self) {
    const int id = LOWORD(wparam);
    const int code = HIWORD(wparam);
    if (id == kIdSearch && (code == EN_CHANGE)) {
      self->ApplyFilter();
      return 0;
    }
    if (id == kIdFilter && (code == CBN_SELCHANGE)) {
      self->ApplyFilter();
      return 0;
    }
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_CLOSE) {
    if (self) {
      self->Hide();
    }
    return 0;
  }
  if (msg == WM_DESTROY) {
    if (self) {
      self->hwnd_ = NULL;
      self->search_ = self->filter_ = self->list_ = self->hint_ = NULL;
      self->visible_ = false;
    }
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

WinAuthUi::WinAuthUi(HWND owner) : owner_(owner) {}

bool WinAuthUi::PromptLogin(SessionService* session) {
  return ShowLoginDialog(owner_, session);
}

void BindWinPlatform(PlatformServices* out,
                     WinTrayHost* tray,
                     WinMenuRenderer* menu,
                     WinPanelRenderer* panel,
                     WinAuthUi* auth,
                     IWebViewPanel* webview) {
  if (!out) {
    return;
  }
  out->tray = tray;
  out->menu_renderer = menu;
  out->panel_renderer = panel;
  out->webview_panel = webview;
  out->auth_ui = auth;
  out->panel_content_ui = panel;
}

int RunMessageLoop() {
  MSG msg;
  while (GetMessageW(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}


namespace {

HANDLE g_single_instance_mutex = NULL;

}  // namespace

bool TryAcquireSingleInstance() {
  // CUSTOMIZE: 改互斥名以避免与其它工具冲突，例如 Local\MyTool.single_instance
  SetLastError(0);
  g_single_instance_mutex = CreateMutexW(
      NULL, FALSE, L"Local\\tray_demo.single_instance");
  if (g_single_instance_mutex == NULL) {
    return false;
  }
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(g_single_instance_mutex);
    g_single_instance_mutex = NULL;
    return false;
  }
  // 持有句柄直至进程退出；无需占用互斥锁本身
  return true;
}

void ReleaseSingleInstance() {
  if (g_single_instance_mutex) {
    CloseHandle(g_single_instance_mutex);
    g_single_instance_mutex = NULL;
  }
}

}  // namespace win
}  // namespace tray_demo

#endif  // _WIN32
