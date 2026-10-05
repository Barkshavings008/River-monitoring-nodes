#!/bin/bash
# Builds the river node code in ../src into WebAssembly and puts it inside
# index.html, so the page runs the same code as the ESP32.
#
# Needs clang with the wasm32 target (clang 16 or newer) and curl. The first
# run downloads the WASI C library (about 3 MB) into simulator/.wasi/.
#
# Usage:  ./simulator/build.sh     (then open simulator/index.html in a browser)
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../src"
WASI="$HERE/.wasi"
WASI_VERSION=24

# The C library for WebAssembly (strings, maths, snprintf)
if [ ! -d "$WASI/wasi-sysroot-$WASI_VERSION.0" ]; then
    mkdir -p "$WASI"
    for file in "wasi-sysroot-$WASI_VERSION.0.tar.gz" "libclang_rt.builtins-wasm32-wasi-$WASI_VERSION.0.tar.gz"; do
        curl -sSL -o "$WASI/$file" \
            "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-$WASI_VERSION/$file"
        tar xzf "$WASI/$file" -C "$WASI"
    done
fi
SYSROOT="$WASI/wasi-sysroot-$WASI_VERSION.0"
BUILTINS="$WASI/libclang_rt.builtins-wasm32-wasi-$WASI_VERSION.0/libclang_rt.builtins-wasm32.a"

# The same files the ESP32 uses, minus the hardware ones (sensors, commands,
# simulator, main). DEMO_MODE=0 so it uses the real 24 h baseline.
clang++ --target=wasm32-wasi --sysroot="$SYSROOT" -O2 -fno-exceptions -fno-rtti \
    -mexec-model=reactor -nostdlib -DDEMO_MODE=0 \
    -I"$HERE/wasm" -I"$SRC" \
    "$SRC/types.cpp" "$SRC/labels.cpp" "$SRC/compensation.cpp" "$SRC/baseline.cpp" \
    "$SRC/rules.cpp" "$SRC/persistence.cpp" "$SRC/node_engine.cpp" "$SRC/network.cpp" \
    "$SRC/display.cpp" "$HERE/wasm/bridge.cpp" \
    "$SYSROOT/lib/wasm32-wasi/crt1-reactor.o" -L"$SYSROOT/lib/wasm32-wasi" -lc "$BUILTINS" \
    -o "$HERE/river_node.wasm"

# Put the simulation script and the WebAssembly inside the page, so index.html works on its own
python3 - "$HERE" <<'EOF'
import base64, sys
here = sys.argv[1]
wasm = base64.b64encode(open(here + "/river_node.wasm", "rb").read()).decode()
page = open(here + "/page.html").read()
core = open(here + "/sim_core.js").read()
for marker in ["__RIVER_NODE_WASM__", "/*__SIM_CORE__*/"]:
    assert page.count(marker) == 1, "page.html needs exactly one " + marker
page = page.replace("/*__SIM_CORE__*/", core).replace("__RIVER_NODE_WASM__", wasm)
open(here + "/index.html", "w").write(page)
EOF
echo "Built $HERE/index.html ($(wc -c < "$HERE/river_node.wasm") byte WebAssembly module)"
