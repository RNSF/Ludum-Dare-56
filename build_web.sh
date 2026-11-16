#!/usr/bin/env bash
# Build the game for the web with Emscripten.
#
#   ./build_web.sh          build into build/web/
#   ./build_web.sh serve    build, then serve on http://localhost:8080
#
# Needs emcc on PATH. If you use emsdk, either `source <emsdk>/emsdk_env.sh` first
# or set EMSDK=<emsdk dir> and this script will source it.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAYLIB_VERSION="6.2"
DEPS="$ROOT/build/web-deps"
RAYLIB_SRC="$DEPS/raylib/src"
RAYLIB_LIB="$DEPS/raylib-web/libraylib.web.a"
OUT="$ROOT/build/web"

if ! command -v emcc >/dev/null 2>&1; then
    if [[ -n "${EMSDK:-}" && -f "$EMSDK/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1091
        source "$EMSDK/emsdk_env.sh" >/dev/null
    elif [[ -f "$HOME/emsdk/emsdk_env.sh" ]]; then
        # shellcheck disable=SC1091
        source "$HOME/emsdk/emsdk_env.sh" >/dev/null
    elif [[ -x /usr/lib/emscripten/emcc ]]; then
        # Arch's emscripten package; its PATH entry is only added for new login shells
        export PATH="/usr/lib/emscripten:$PATH"
    fi
fi
command -v emcc >/dev/null 2>&1 || { echo "error: emcc not found. Install Emscripten or source emsdk_env.sh." >&2; exit 1; }

# 1. raylib for the web (built once, then reused)
if [[ ! -f "$RAYLIB_LIB" ]]; then
    if [[ ! -d "$DEPS/raylib" ]]; then
        echo "== Fetching raylib $RAYLIB_VERSION"
        mkdir -p "$DEPS"
        git clone -q --depth 1 --branch "$RAYLIB_VERSION" https://github.com/raysan5/raylib "$DEPS/raylib"
    fi
    echo "== Building raylib $RAYLIB_VERSION for PLATFORM_WEB"
    mkdir -p "$(dirname "$RAYLIB_LIB")"
    make -C "$RAYLIB_SRC" clean >/dev/null
    make -C "$RAYLIB_SRC" -j"$(nproc)" PLATFORM=PLATFORM_WEB RAYLIB_BUILD_MODE=RELEASE \
        RAYLIB_RELEASE_PATH="$(dirname "$RAYLIB_LIB")"
fi

# 2. the game
echo "== Building game"
mkdir -p "$OUT"
cd "$ROOT"   # --preload-file paths are relative to here, matching the desktop working directory
emcc src/*.c -o "$OUT/index.html" \
    -std=gnu17 -O2 -Wall -Wno-unknown-pragmas \
    -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 \
    -Isrc -Isrc/include -I"$RAYLIB_SRC" \
    "$RAYLIB_LIB" \
    -sUSE_GLFW=3 \
    -sALLOW_MEMORY_GROWTH=1 \
    -sINITIAL_MEMORY=64MB \
    -sSTACK_SIZE=1MB \
    --preload-file assets \
    --shell-file "$ROOT/web/shell.html"

echo "== Built $OUT/index.html"

if [[ "${1:-}" == "serve" ]]; then
    echo "== Serving on http://localhost:8080 (Ctrl+C to stop)"
    python3 -m http.server 8080 --bind 127.0.0.1 --directory "$OUT"
fi
