#!/usr/bin/env bash
# Build and test, then serve web/ at http://localhost:${PORT:-8000}/
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
py="$root/venv/bin/python"
[ -x "$py" ] || py=python3

"$root/tools/build.sh"
echo "serving $root/web at http://localhost:${PORT:-8000}/"
exec "$py" -m http.server "${PORT:-8000}" --bind 127.0.0.1 -d "$root/web"
