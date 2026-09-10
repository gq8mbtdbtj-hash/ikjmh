/**
 * @file wb_lib.cpp
 * @brief 白盒 victim 实现（独立 DLL/SO）。
 */

#include "wb_lib.h"

extern "C" int wb_target(int a, int b) {
  return a + b + 1000;  // magic=1000，proxy 可改为 +2000 以区分
}

extern "C" const char* wb_lib_version(void) {
  return "tray_wb_lib/1.0";
}
