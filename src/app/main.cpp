/**
 * @file main.cpp
 * @brief 程序入口：平台 + AuthModule + 业务 module。
 *
 * CUSTOMIZE: 实现 IAppModule 并 RegisterModule；勿把业务写进 AppController。
 * 登录由框架 AuthModule 提供（auth.login / auth.logout）。
 */

#include "tray_demo/app/app_controller.hpp"
#include "tray_demo/auth/auth_module.hpp"
#include "tray_demo/page/basic_page.hpp"

#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
#include "tray_demo/modules/demo/demo_module.hpp"
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
#include "tray_demo/modules/about/about_module.hpp"
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
#include "tray_demo/modules/memprobe/memprobe_module.hpp"
#endif

#if defined(_WIN32)
#include "tray_demo/platform/win/win_platform.hpp"
#include "tray_demo/platform/win/win_webview_panel.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#elif defined(__APPLE__)
#include "tray_demo/platform/mac/mac_platform.hpp"
#elif defined(__linux__)
#include "tray_demo/platform/linux/linux_platform.hpp"
#else
#include "tray_demo/platform/null_platform.hpp"
#endif

namespace {

void RegisterBusinessModules(tray_demo::AppController& app
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
                             ,
                             tray_demo::modules::DemoModule* demo
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
                             ,
                             tray_demo::modules::AboutModule* about
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
                             ,
                             tray_demo::modules::MemprobeModule* memprobe
#endif
) {
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
  if (demo) {
    app.RegisterModule(demo);
  }
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
  if (about) {
    app.RegisterModule(about);
  }
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
  if (memprobe) {
    app.RegisterModule(memprobe);
  }
#endif
  (void)app;
}

}  // namespace

#if defined(_WIN32)

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
  using namespace tray_demo;
  using namespace tray_demo::win;

  CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

  if (!TryAcquireSingleInstance()) {
    CoUninitialize();
    return 0;
  }

  WinTrayHost tray;
  WinMenuRenderer menu;
  WinPanelRenderer panel;
  WinWebViewPanel webview;
  WinAuthUi auth(NULL);
  AuthModule auth_module;
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
  modules::DemoModule demo;
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
  modules::AboutModule about;
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
  modules::MemprobeModule memprobe;
#endif

  AppController app;
  PlatformServices services;
  BindWinPlatform(&services, &tray, &menu, &panel, &auth, &webview);
  app.SetPlatform(services);
  app.RegisterModule(&auth_module);
  RegisterBusinessModules(app
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
                          ,
                          &demo
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
                          ,
                          &about
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
                          ,
                          &memprobe
#endif
  );

  BasicPage home("home", "Home");
  app.navigator().SetRoot(&home);

  if (!app.Start()) {
    MessageBoxW(NULL, L"Failed to create tray icon.", L"tray_demo", MB_ICONERROR);
    ReleaseSingleInstance();
    CoUninitialize();
    return 1;
  }

  menu.SetOwnerHwnd(tray.hwnd());
  auth.SetOwner(tray.hwnd());
  panel.SetNotifyIconAnchor(tray.hwnd(), tray.notify_icon_id());
  webview.SetNotifyIconAnchor(tray.hwnd(), tray.notify_icon_id());
  if (!panel.Create()) {
    MessageBoxW(NULL, L"Failed to create panel window.", L"tray_demo", MB_ICONERROR);
    app.Shutdown();
    ReleaseSingleInstance();
    CoUninitialize();
    return 1;
  }

  if (!webview.CreateHost()) {
    // 非致命
  }

  app.platform().tray->SetTooltip("tray_demo — auth + modules");
  app.RefreshPanelUi();

  const int code = RunMessageLoop();
  webview.Destroy();
  ReleaseSingleInstance();
  CoUninitialize();
  return code;
}

#elif defined(__APPLE__)

int main() {
  using namespace tray_demo;
  using namespace tray_demo::mac;

  MacTrayHost tray;
  MacMenuRenderer menu;
  MacPanelRenderer panel;
  MacAuthUi auth;
  AuthModule auth_module;
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
  modules::DemoModule demo;
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
  modules::AboutModule about;
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
  modules::MemprobeModule memprobe;
#endif

  AppController app;
  PlatformServices services;
  BindMacPlatform(&services, &tray, &menu, &panel, &auth);
  app.SetPlatform(services);
  app.RegisterModule(&auth_module);
  RegisterBusinessModules(app
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
                          ,
                          &demo
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
                          ,
                          &about
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
                          ,
                          &memprobe
#endif
  );

  BasicPage home("home", "Home");
  app.navigator().SetRoot(&home);
  if (!app.Start()) {
    return 1;
  }
  // PLATFORM: stub — 接入 NSApplication 后在此跑主循环
  app.platform().tray->SetTooltip("tray_demo mac stub");
  app.RefreshPanelUi();
  app.Shutdown();
  return 0;
}

#elif defined(__linux__)

int main() {
  using namespace tray_demo;
  using namespace tray_demo::linux_plat;

  LinuxTrayHost tray;
  LinuxMenuRenderer menu;
  LinuxPanelRenderer panel;
  LinuxAuthUi auth;
  AuthModule auth_module;
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
  modules::DemoModule demo;
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
  modules::AboutModule about;
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
  modules::MemprobeModule memprobe;
#endif

  AppController app;
  PlatformServices services;
  BindLinuxPlatform(&services, &tray, &menu, &panel, &auth);
  app.SetPlatform(services);
  app.RegisterModule(&auth_module);
  RegisterBusinessModules(app
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
                          ,
                          &demo
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
                          ,
                          &about
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
                          ,
                          &memprobe
#endif
  );

  BasicPage home("home", "Home");
  app.navigator().SetRoot(&home);
  if (!app.Start()) {
    return 1;
  }
  // PLATFORM: stub — 接入 GTK/Qt 事件循环后在此阻塞
  app.platform().tray->SetTooltip("tray_demo linux stub");
  app.RefreshPanelUi();
  app.Shutdown();
  return 0;
}

#else

int main() {
  using namespace tray_demo;
  NullTrayHost tray;
  NullMenuRenderer menu;
  NullPanelRenderer panel;
  NullAuthUi auth;
  AuthModule auth_module;
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
  modules::DemoModule demo;
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
  modules::AboutModule about;
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
  modules::MemprobeModule memprobe;
#endif
  AppController app;
  PlatformServices services;
  BindNullPlatform(&services, &tray, &menu, &panel, &auth);
  app.SetPlatform(services);
  app.RegisterModule(&auth_module);
  RegisterBusinessModules(app
#if defined(TRAY_DEMO_HAS_DEMO_MODULE) && TRAY_DEMO_HAS_DEMO_MODULE
                          ,
                          &demo
#endif
#if defined(TRAY_DEMO_HAS_ABOUT_MODULE) && TRAY_DEMO_HAS_ABOUT_MODULE
                          ,
                          &about
#endif
#if defined(TRAY_DEMO_HAS_MEMPROBE_MODULE) && TRAY_DEMO_HAS_MEMPROBE_MODULE
                          ,
                          &memprobe
#endif
  );
  BasicPage home("home", "Home");
  app.navigator().SetRoot(&home);
  if (!app.Start()) {
    return 1;
  }
  app.Shutdown();
  return 0;
}

#endif
