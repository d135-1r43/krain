#!/usr/bin/env bash
# Indexes the renders directory and serves the compare app.
#
# A server is needed rather than opening the file directly because the page
# fetches manifest.json and the WAVs, and fetch() is blocked on file:// origins.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
renders="${1:-$(cd "$here/../.." && pwd)/build/renders}"
port="${PORT:-8420}"

if [ ! -d "$renders" ]; then
  echo "No renders directory at $renders"
  echo "Build and run the renderer first:"
  echo "  cmake --build build --target krain-render && ./build/tools/krain-render"
  exit 1
fi

cp "$here/index.html" "$renders/index.html"
python3 "$here/build_manifest.py" "$renders"

echo "Serving $renders on http://localhost:$port"
(sleep 1 && open "http://localhost:$port") >/dev/null 2>&1 &
exec python3 -m http.server "$port" --directory "$renders"
