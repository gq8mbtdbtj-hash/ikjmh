#include "tray_demo/modules/demo/demo_catalog.hpp"

namespace tray_demo {
namespace modules {

std::vector<PanelListItem> BuildDemoCatalog() {
  // =========================================================================
  // CUSTOMIZE: demo 业务列表条目
  // =========================================================================
  std::vector<PanelListItem> items;
  PanelListItem a;
  a.id = "demo-tool";
  a.name = "Demo Tool";
  a.category = "Toolchains";
  a.detail = "1.0.0 → 1.1.0";
  items.push_back(a);

  PanelListItem b;
  b.id = "clang-kit";
  b.name = "Clang Toolchain";
  b.category = "Toolchains";
  b.detail = "18.1.0";
  items.push_back(b);

  PanelListItem c;
  c.id = "msvc-kit";
  c.name = "MSVC Build Tools";
  c.category = "Toolchains";
  c.detail = "14.40";
  items.push_back(c);

  PanelListItem d;
  d.id = "idea";
  d.name = "IntelliJ IDEA Community";
  d.category = "IDE";
  d.detail = "2024.3";
  items.push_back(d);

  PanelListItem e;
  e.id = "vscode";
  e.name = "VS Code";
  e.category = "IDE";
  e.detail = "1.96";
  items.push_back(e);

  PanelListItem f;
  f.id = "cmake";
  f.name = "CMake";
  f.category = "Build";
  f.detail = "3.31";
  items.push_back(f);

  PanelListItem g;
  g.id = "ninja";
  g.name = "Ninja";
  g.category = "Build";
  g.detail = "1.12";
  items.push_back(g);

  PanelListItem h;
  h.id = "ldap-proxy";
  h.name = "LDAP Auth Helper";
  h.category = "Network";
  h.detail = "0.2.0";
  items.push_back(h);

  return items;
}

}  // namespace modules
}  // namespace tray_demo
