#!/usr/bin/env bash
# Build and test the C++ natively and for WebAssembly.
# The browser module lands in web/wasm/. Needs emcmake (emsdk) on PATH.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"

cmake -S "$root/cpp" -B "$root/build-native" -G Ninja
cmake --build "$root/build-native"
ctest --test-dir "$root/build-native" --output-on-failure

emcmake cmake -S "$root/cpp" -B "$root/build-wasm" -G Ninja
cmake --build "$root/build-wasm"
ctest --test-dir "$root/build-wasm" --output-on-failure
