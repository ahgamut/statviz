#!/usr/bin/env bash
# Assemble the deployable site into $1 (default: _site):
#   index.html   landing page (repo root)
#   app/         the web app (web/, including the built web/wasm/)
# Run tools/build.sh first.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
out="${1:-$root/_site}"

for f in statviz.js statviz.wasm; do
  [ -f "$root/web/wasm/$f" ] || { echo "missing web/wasm/$f; run tools/build.sh" >&2; exit 1; }
done

rm -rf "$out"
mkdir -p "$out"
cp "$root/index.html" "$out/index.html"
cp -r "$root/web" "$out/app"
echo "site assembled in $out"
