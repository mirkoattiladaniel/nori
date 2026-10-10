#!/usr/bin/env bash
# Fetch + build a shared raylib into vendor/ (raylib.h + libraylib.so*). A native build links C libraries
# dynamically; the program finds vendor/ through the run path the build records in it.
set -euo pipefail
cd "$(dirname "$0")"
TAG="${1:-5.5}"
command -v cmake >/dev/null && command -v git >/dev/null || { echo "need git + cmake"; exit 1; }

tmp="$(mktemp -d)"
echo "cloning raylib $TAG ..."
git clone --depth 1 --branch "$TAG" https://github.com/raysan5/raylib.git "$tmp/raylib" >/dev/null 2>&1
echo "building (shared) ..."
cmake -S "$tmp/raylib" -B "$tmp/build" \
  -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF >/dev/null
cmake --build "$tmp/build" -j"$(nproc)" >/dev/null

mkdir -p vendor
cp "$tmp/raylib/src/raylib.h" vendor/
cp -P "$tmp"/build/raylib/libraylib.so* vendor/
rm -rf "$tmp"
echo "done -> vendor/raylib.h, vendor/libraylib.so*"
echo "now:  roll run        (or:  mkdir -p out && noric --build src/main.nori out/snake --import-root . && out/snake)"
