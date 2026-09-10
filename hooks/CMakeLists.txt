cmake_minimum_required(VERSION 3.16)

if(NOT TARGET tray_hooks)
  project(tray_hooks LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

add_library(tray_hooks STATIC
  src/hooks_api.cpp
  src/collector.cpp
  src/apm_sink.cpp
  src/backtrace/backtrace_all.cpp
)

set_property(TARGET tray_hooks PROPERTY POSITION_INDEPENDENT_CODE ON)

target_include_directories(tray_hooks
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
  target_sources(tray_hooks PRIVATE
    src/plat/win_iat_backend.cpp
    src/plat/win_iat_patch.cpp
  )
  target_link_libraries(tray_hooks PUBLIC dbghelp winhttp)
else()
  target_sources(tray_hooks PRIVATE
    src/plat/elf_plt_backend.cpp
    src/plat/elf_plt_patch.cpp
  )
  target_link_libraries(tray_hooks PUBLIC ${CMAKE_DL_LIBS})
  find_package(Threads REQUIRED)
  target_link_libraries(tray_hooks PUBLIC Threads::Threads)
endif()

if(MSVC)
  target_compile_options(tray_hooks PRIVATE /utf-8 /W3)
  target_compile_definitions(tray_hooks PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

option(TRAY_HOOKS_BUILD_SAMPLE "Build tray_hooks sample" ON)
if(TRAY_HOOKS_BUILD_SAMPLE)
  add_executable(tray_hooks_sample examples/sample_collect.cpp)
  target_link_libraries(tray_hooks_sample PRIVATE tray_hooks)
endif()

# ---- 可注入探针（无改业务工程编译）----
option(TRAY_HOOKS_BUILD_MEMPROBE "Build tray_memprobe shared library" ON)
if(TRAY_HOOKS_BUILD_MEMPROBE)
  set(MEMPROBE_SOURCES
    src/memprobe/memprobe_tracker.cpp
    src/memprobe/memprobe_install.cpp
    src/memprobe/memprobe_gpu.cpp
    src/memprobe/memprobe_npu.cpp
    src/memprobe/memprobe_init.cpp
  )
  if(NOT WIN32)
    list(APPEND MEMPROBE_SOURCES src/memprobe/alloc_interpose.cpp)
  endif()

  add_library(tray_memprobe SHARED ${MEMPROBE_SOURCES})
  target_include_directories(tray_memprobe
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/memprobe
            ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_definitions(tray_memprobe PRIVATE TRAY_MEMPROBE_EXPORTS)
  target_link_libraries(tray_memprobe PRIVATE tray_hooks)

  if(WIN32)
    target_link_libraries(tray_memprobe PRIVATE dbghelp psapi)
  else()
    target_link_libraries(tray_memprobe PRIVATE ${CMAKE_DL_LIBS} pthread)
    target_compile_options(tray_memprobe PRIVATE -fvisibility=hidden)
  endif()

  if(MSVC)
    target_compile_options(tray_memprobe PRIVATE /utf-8 /W3)
    target_compile_definitions(tray_memprobe PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  set_target_properties(tray_memprobe PROPERTIES
    OUTPUT_NAME "tray_memprobe"
    VERSION 0.1.0
    SOVERSION 0
  )

  add_executable(memprobe_smoke examples/memprobe_smoke.cpp)
  if(WIN32)
    # Windows：直接链 DLL，验证 IAT install 路径
    target_link_libraries(memprobe_smoke PRIVATE tray_memprobe)
    target_compile_definitions(memprobe_smoke PRIVATE TRAY_MEMPROBE_STATIC=0)
  else()
    target_link_libraries(memprobe_smoke PRIVATE tray_hooks)
  endif()
endif()

# ---- 单元自测 + 白盒 hook ----
option(TRAY_HOOKS_BUILD_TESTS "Build hooks unit tests and whitebox" ON)
if(TRAY_HOOKS_BUILD_TESTS)
  add_executable(hooks_unit_tests tests/test_collector_unit.cpp)
  target_link_libraries(hooks_unit_tests PRIVATE tray_hooks)
  if(MSVC)
    target_compile_options(hooks_unit_tests PRIVATE /utf-8)
    target_compile_definitions(hooks_unit_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()

  add_executable(hooks_apm_tests tests/test_apm_unit.cpp)
  target_link_libraries(hooks_apm_tests PRIVATE tray_hooks)
  if(MSVC)
    target_compile_options(hooks_apm_tests PRIVATE /utf-8)
    target_compile_definitions(hooks_apm_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  # 白盒 victim 动态库
  add_library(tray_wb_lib SHARED tests/wb_lib.cpp)
  target_include_directories(tray_wb_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_compile_definitions(tray_wb_lib PRIVATE TRAY_WB_LIB_EXPORTS)
  if(NOT WIN32)
    target_compile_options(tray_wb_lib PRIVATE -fvisibility=hidden)
  endif()
  if(MSVC)
    target_compile_options(tray_wb_lib PRIVATE /utf-8)
  endif()
  set_target_properties(tray_wb_lib PROPERTIES OUTPUT_NAME "tray_wb_lib")

  add_library(tray_wb_late SHARED tests/wb_late.cpp)
  target_include_directories(tray_wb_late PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(tray_wb_late PRIVATE tray_wb_lib)
  if(NOT WIN32)
    target_compile_options(tray_wb_late PRIVATE -fvisibility=hidden)
  endif()
  if(MSVC)
    target_compile_options(tray_wb_late PRIVATE /utf-8)
  endif()
  set_target_properties(tray_wb_late PROPERTIES OUTPUT_NAME "tray_wb_late")

  add_executable(hook_whitebox examples/hook_whitebox.cpp)
  target_include_directories(hook_whitebox PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  target_link_libraries(hook_whitebox PRIVATE tray_hooks tray_wb_lib)
  add_dependencies(hook_whitebox tray_wb_late)
  if(MSVC)
    target_compile_options(hook_whitebox PRIVATE /utf-8)
  endif()

  enable_testing()
  add_test(NAME hooks_unit_tests COMMAND hooks_unit_tests)
  add_test(NAME hooks_apm_tests COMMAND hooks_apm_tests)
  add_test(NAME hook_whitebox COMMAND hook_whitebox)
  # 保证测试工作目录能 LoadLibrary 到 late DLL
  set_tests_properties(hook_whitebox PROPERTIES
    WORKING_DIRECTORY $<TARGET_FILE_DIR:hook_whitebox>)
endif()
