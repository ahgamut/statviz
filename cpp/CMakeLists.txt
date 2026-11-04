cmake_minimum_required(VERSION 3.20)
project(statviz CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(statviz_core STATIC src/expr.cpp src/families.cpp src/gpu.cpp src/meta.cpp src/points.cpp src/scene.cpp src/scrub.cpp)
target_include_directories(statviz_core PUBLIC include)
target_compile_options(statviz_core PRIVATE -Wall -Wextra -Wpedantic)
if(EMSCRIPTEN)
  target_compile_options(statviz_core PUBLIC -msimd128)
endif()

# Tests run natively, and under node for the WebAssembly build
# (Emscripten's toolchain sets CMAKE_CROSSCOMPILING_EMULATOR).
enable_testing()
add_executable(statviz_tests tests/main.cpp tests/test_expr.cpp tests/test_families.cpp tests/test_points.cpp tests/test_scene.cpp tests/test_scrub.cpp)
target_link_libraries(statviz_tests PRIVATE statviz_core)
target_include_directories(statviz_tests SYSTEM PRIVATE third_party/doctest)
add_test(NAME statviz_tests COMMAND statviz_tests)

if(EMSCRIPTEN)
  set(SV_WASM_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../web/wasm" CACHE PATH
      "Where the browser module (statviz.js + statviz.wasm) is written")
  add_executable(statviz src/api.cpp)
  target_link_libraries(statviz PRIVATE statviz_core)
  set_target_properties(statviz PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${SV_WASM_OUT_DIR}")
  target_link_options(statviz PRIVATE
    --no-entry
    -sMODULARIZE
    -sEXPORT_ES6
    -sENVIRONMENT=web
    -sALLOW_MEMORY_GROWTH=0
    -sINITIAL_MEMORY=128MB
    -sEXPORTED_RUNTIME_METHODS=HEAPU8,HEAP32,HEAPF32,HEAPF64)
endif()

if(NOT EMSCRIPTEN)
  add_executable(cf_dump tools/cf_dump.cpp)
  target_link_libraries(cf_dump PRIVATE statviz_core)
endif()
