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
