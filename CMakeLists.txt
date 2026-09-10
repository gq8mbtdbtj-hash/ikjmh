cmake_minimum_required(VERSION 3.16)
project(tray_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL
    "Export compile_commands.json for IDE indexing" FORCE)

if(MSVC)
  add_compile_options(/utf-8 /W3)
endif()

if(WIN32)
  add_compile_definitions(_WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

option(TRAY_DEMO_BUILD_APP "Build tray app executable" ON)
option(TRAY_DEMO_BUILD_DEMO_MODULE "Build demo business module" ON)

# ---- Framework library (no product business) ----
set(TRAY_DEMO_CORE_SOURCES
  src/core/menu/menu_model.cpp
  src/core/page/page_navigator.cpp
  src/core/app/app_controller.cpp
  src/core/auth/auth_module.cpp
  src/core/thread/job_queue.cpp
  src/core/config/settings.cpp
  src/core/net/http_client.cpp
  src/core/net/session.cpp
  src/core/util/fuzzy_match.cpp
  src/core/panel/list_item.cpp
  src/platform/null/null_platform.cpp
)

if(WIN32)
  list(APPEND TRAY_DEMO_CORE_SOURCES
    src/platform/win/win_platform.cpp
    src/platform/win/login_dialog.cpp
    src/platform/win/win_webview_panel.cpp
    src/platform/win/win_panel_placement.cpp
  )
elseif(APPLE)
  list(APPEND TRAY_DEMO_CORE_SOURCES
    src/platform/mac/mac_platform.cpp
  )
elseif(UNIX)
  list(APPEND TRAY_DEMO_CORE_SOURCES
    src/platform/linux/linux_platform.cpp
  )
endif()

add_library(tray_demo_core ${TRAY_DEMO_CORE_SOURCES})

target_include_directories(tray_demo_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(WIN32)
  set(TRAY_DEMO_WEBVIEW2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/webview2"
      CACHE PATH "WebView2 SDK root (headers + lib)")
  if(EXISTS "${TRAY_DEMO_WEBVIEW2_ROOT}/include/WebView2.h")
    target_include_directories(tray_demo_core PUBLIC "${TRAY_DEMO_WEBVIEW2_ROOT}/include")
    target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_WEBVIEW2=1)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(_wv2_lib "${TRAY_DEMO_WEBVIEW2_ROOT}/lib/x64/WebView2LoaderStatic.lib")
    else()
      set(_wv2_lib "${TRAY_DEMO_WEBVIEW2_ROOT}/lib/x86/WebView2LoaderStatic.lib")
    endif()
    if(EXISTS "${_wv2_lib}")
      message(STATUS "WebView2 loader: ${_wv2_lib}")
      target_link_libraries(tray_demo_core PUBLIC "${_wv2_lib}" version)
    else()
      message(WARNING "WebView2 headers found but loader lib missing: ${_wv2_lib}")
    endif()
  else()
    message(WARNING "WebView2 SDK not found under ${TRAY_DEMO_WEBVIEW2_ROOT}")
    target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_WEBVIEW2=0)
  endif()
  target_link_libraries(tray_demo_core PUBLIC shell32 user32 gdi32 winhttp ole32 oleaut32 comctl32)
endif()

if(WIN32)
  set(TRAY_DEMO_PTHREADS4W_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/pthreads4w"
      CACHE PATH "pthreads4w root (Windows)")
  if(EXISTS "${TRAY_DEMO_PTHREADS4W_ROOT}/pthread.h")
    target_include_directories(tray_demo_core PUBLIC "${TRAY_DEMO_PTHREADS4W_ROOT}")
    target_compile_definitions(tray_demo_core PUBLIC
      TRAY_DEMO_HAS_PTHREAD=1
      HAVE_STRUCT_TIMESPEC
    )
    find_library(TRAY_DEMO_PTHREAD_LIB
      NAMES pthreadVC2 pthreadVC3 pthreadVCE2 pthreadVSE2 pthread
      PATHS "${TRAY_DEMO_PTHREADS4W_ROOT}" "${TRAY_DEMO_PTHREADS4W_ROOT}/lib"
      NO_DEFAULT_PATH)
    if(TRAY_DEMO_PTHREAD_LIB)
      message(STATUS "pthreads lib: ${TRAY_DEMO_PTHREAD_LIB}")
      target_link_libraries(tray_demo_core PUBLIC ${TRAY_DEMO_PTHREAD_LIB})
    else()
      message(FATAL_ERROR "找到 pthread.h 但未找到 pthreadVC2.lib")
    endif()
    set(TRAY_DEMO_PTHREAD_DLL "${TRAY_DEMO_PTHREADS4W_ROOT}/pthreadVC2.dll")
    if(EXISTS "${TRAY_DEMO_PTHREAD_DLL}")
      set(TRAY_DEMO_PTHREAD_DLL_PATH "${TRAY_DEMO_PTHREAD_DLL}" CACHE FILEPATH "" FORCE)
    endif()
  else()
    target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_PTHREAD=0)
  endif()
else()
  find_package(Threads REQUIRED)
  target_link_libraries(tray_demo_core PUBLIC Threads::Threads)
  target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_PTHREAD=1)

  # POSIX HTTPS via OpenSSL（可选；未找到则仅 http://）
  option(TRAY_DEMO_USE_OPENSSL "Enable HTTPS on POSIX via OpenSSL" ON)
  if(TRAY_DEMO_USE_OPENSSL)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
      message(STATUS "OpenSSL: ${OPENSSL_VERSION} (POSIX HTTPS enabled)")
      target_link_libraries(tray_demo_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
      target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_OPENSSL=1)
    else()
      message(WARNING "OpenSSL not found — POSIX HttpClient http:// only")
      target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_OPENSSL=0)
    endif()
  else()
    target_compile_definitions(tray_demo_core PUBLIC TRAY_DEMO_HAS_OPENSSL=0)
  endif()
endif()

option(TRAY_DEMO_BUILD_ABOUT_MODULE "Build about sample module" ON)
option(TRAY_DEMO_BUILD_HOOKS "Build cross-platform tray_hooks library" ON)
option(TRAY_DEMO_BUILD_MEMPROBE_MODULE "Tray menu for tray_memprobe" ON)

# ---- Demo business module ----
if(TRAY_DEMO_BUILD_DEMO_MODULE)
  add_library(tray_demo_module_demo
    src/modules/demo/demo_module.cpp
    src/modules/demo/demo_catalog.cpp
    src/modules/demo/demo_catalog_model.cpp
  )
  target_link_libraries(tray_demo_module_demo PUBLIC tray_demo_core)
endif()

# ---- About sample module（A5 多 module 并存示例）----
if(TRAY_DEMO_BUILD_ABOUT_MODULE)
  add_library(tray_demo_module_about
    src/modules/about/about_module.cpp
  )
  target_link_libraries(tray_demo_module_about PUBLIC tray_demo_core)
endif()

# ---- Cross-platform hook + backtrace collector ----
if(TRAY_DEMO_BUILD_HOOKS)
  add_subdirectory(hooks)
endif()

# ---- Memprobe 托盘联动（依赖 tray_memprobe）----
if(TRAY_DEMO_BUILD_MEMPROBE_MODULE AND TARGET tray_memprobe)
  add_library(tray_demo_module_memprobe
    src/modules/memprobe/memprobe_module.cpp
  )
  target_link_libraries(tray_demo_module_memprobe PUBLIC tray_demo_core tray_memprobe)
endif()

if(TRAY_DEMO_BUILD_APP)
  if(WIN32)
    add_executable(tray_demo WIN32 src/app/main.cpp)
  else()
    add_executable(tray_demo src/app/main.cpp)
  endif()
  target_link_libraries(tray_demo PRIVATE tray_demo_core)
  if(TRAY_DEMO_BUILD_DEMO_MODULE)
    target_link_libraries(tray_demo PRIVATE tray_demo_module_demo)
    target_compile_definitions(tray_demo PRIVATE TRAY_DEMO_HAS_DEMO_MODULE=1)
  endif()
  if(TRAY_DEMO_BUILD_ABOUT_MODULE)
    target_link_libraries(tray_demo PRIVATE tray_demo_module_about)
    target_compile_definitions(tray_demo PRIVATE TRAY_DEMO_HAS_ABOUT_MODULE=1)
  endif()
  if(TARGET tray_demo_module_memprobe)
    target_link_libraries(tray_demo PRIVATE tray_demo_module_memprobe)
    target_compile_definitions(tray_demo PRIVATE TRAY_DEMO_HAS_MEMPROBE_MODULE=1)
    add_custom_command(TARGET tray_demo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:tray_memprobe>
        $<TARGET_FILE_DIR:tray_demo>
      COMMENT "Copy tray_memprobe next to tray_demo")
  endif()
  if(WIN32 AND DEFINED TRAY_DEMO_PTHREAD_DLL_PATH)
    add_custom_command(TARGET tray_demo POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TRAY_DEMO_PTHREAD_DLL_PATH}"
        $<TARGET_FILE_DIR:tray_demo>)
  endif()
endif()

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(sync_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_BINARY_DIR}/compile_commands.json"
      "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Sync compile_commands.json -> source root"
    VERBATIM)
endif()
