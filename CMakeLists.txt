cmake_minimum_required(VERSION 3.24)
project(UNIFY_CONTROL VERSION 1.8.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(UNIFY_BUILD_TESTS "Build portable device-manager tests" ON)

add_library(unify_device_core STATIC src/native/src/device_manager.cpp src/native/src/device_database.cpp src/native/src/protocol_research.cpp src/native/src/capability_engine.cpp src/native/src/adapter_registry.cpp src/native/src/adapter_sdk.cpp src/native/src/universal_discovery.cpp src/native/src/hid_report_parser.cpp src/native/src/transport_registry.cpp src/native/src/evidence_engine.cpp src/native/src/diagnostic_engine.cpp src/native/src/usb_descriptor_parser.cpp src/native/src/transport_metadata.cpp src/native/src/standards.cpp src/native/src/via_protocol.cpp)
target_include_directories(unify_device_core PUBLIC src/native/include)
if(MSVC)
  target_compile_options(unify_device_core PRIVATE /utf-8 /W4 /permissive-)
else()
  target_compile_options(unify_device_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIFY_BUILD_TESTS)
  enable_testing()
  add_executable(unify_device_manager_tests tests/unit/device_manager_tests.cpp)
  target_link_libraries(unify_device_manager_tests PRIVATE unify_device_core)
  target_compile_definitions(unify_device_manager_tests PRIVATE UNIFY_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  if(MSVC)
    target_compile_options(unify_device_manager_tests PRIVATE /utf-8 /W4 /permissive-)
  else()
    target_compile_options(unify_device_manager_tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME device_manager_core COMMAND unify_device_manager_tests)
endif()

if(WIN32)
  set(WEBVIEW2_SDK_DIR "$ENV{WEBVIEW2_SDK_DIR}" CACHE PATH "Path to Microsoft.Web.WebView2 SDK package")
  if(NOT EXISTS "${WEBVIEW2_SDK_DIR}/build/native/include/WebView2.h")
    message(FATAL_ERROR "Set WEBVIEW2_SDK_DIR to the extracted Microsoft.Web.WebView2 package directory.")
  endif()
  configure_file(src/native/resources/app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/unify_app.rc" @ONLY)
  add_executable(UNIFY_CONTROL WIN32
    "${CMAKE_CURRENT_BINARY_DIR}/unify_app.rc"
    src/native/src/main.cpp
    src/native/src/device_scanner.cpp
    src/native/src/observation_store.cpp
    src/native/src/windows_transport_discovery.cpp
    src/native/src/system_settings.cpp
    src/native/src/via_hid.cpp)
  target_include_directories(UNIFY_CONTROL PRIVATE src/native/include "${WEBVIEW2_SDK_DIR}/build/native/include")
  target_link_directories(UNIFY_CONTROL PRIVATE "${WEBVIEW2_SDK_DIR}/build/native/x64")
  target_link_libraries(UNIFY_CONTROL PRIVATE unify_device_core setupapi hid cfgmgr32 winusb runtimeobject windowsapp ole32 oleaut32 shell32 shlwapi user32 uuid "${WEBVIEW2_SDK_DIR}/build/native/x64/WebView2LoaderStatic.lib")
  target_compile_definitions(UNIFY_CONTROL PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  if(MSVC)
    target_compile_options(UNIFY_CONTROL PRIVATE /W4 /permissive- /EHsc)
  endif()
  add_custom_command(TARGET UNIFY_CONTROL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/src/frontend" "$<TARGET_FILE_DIR:UNIFY_CONTROL>/frontend"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/database" "$<TARGET_FILE_DIR:UNIFY_CONTROL>/database")
elseif(NOT UNIFY_BUILD_TESTS)
  message(FATAL_ERROR "The desktop application requires Windows; enable UNIFY_BUILD_TESTS for portable core tests.")
endif()
