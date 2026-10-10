# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.20)
project(hik_device_bridge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
find_package(Threads REQUIRED)

add_library(hik_bridge src/bridge.cpp src/password.cpp src/sdk_probe.cpp)
target_include_directories(hik_bridge PUBLIC include)
target_link_libraries(hik_bridge PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
target_compile_options(hik_bridge PRIVATE -Wall -Wextra -Wpedantic)
add_executable(hik-device-bridge src/main.cpp)
target_link_libraries(hik-device-bridge PRIVATE hik_bridge)

include(CTest)
if(BUILD_TESTING)
  add_library(symbols_missing SHARED tests/symbol_fixture.cpp)
  add_library(symbols_complete SHARED tests/symbol_fixture.cpp)
  target_compile_definitions(symbols_complete PRIVATE COMPLETE_SYMBOLS)
  add_executable(bridge_tests tests/bridge_tests.cpp)
  target_link_libraries(bridge_tests PRIVATE hik_bridge)
  add_test(NAME bridge_tests COMMAND bridge_tests
    $<TARGET_FILE:symbols_missing> $<TARGET_FILE:symbols_complete>)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME cli_tests COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_tests.py $<TARGET_FILE:hik-device-bridge>)
  set_tests_properties(bridge_tests cli_tests PROPERTIES TIMEOUT 30)
endif()
