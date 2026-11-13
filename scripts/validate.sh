#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build-validation}"
command -v cmake >/dev/null || { echo 'CMake 3.24+ required' >&2; exit 1; }
command -v node >/dev/null || { echo 'Node.js required' >&2; exit 1; }
cmake -S "$ROOT" -B "$BUILD_DIR" -DUNIFY_BUILD_TESTS=ON
cmake --build "$BUILD_DIR" --parallel
ctest --test-dir "$BUILD_DIR" --output-on-failure
node --check "$ROOT/src/frontend/app.js"
node "$ROOT/tests/frontend/diagnostics.test.cjs"
python3 -m json.tool "$ROOT/database/devices.json" >/dev/null
python3 -m json.tool "$ROOT/database/protocol_research.json" >/dev/null
python3 -m json.tool "$ROOT/database/capability_catalog.json" >/dev/null
python3 -m json.tool "$ROOT/database/adapter_research_queue.json" >/dev/null
python3 -m json.tool "$ROOT/plugins/manifest.schema.json" >/dev/null
python3 -m json.tool "$ROOT/plugins/adapter-manifest.schema.json" >/dev/null
python3 - "$ROOT" <<'PYVALIDATE'
from pathlib import Path
import sys
root=Path(sys.argv[1])
app=(root/"src/frontend/app.js").read_text()
assert "all.filter(" not in app, "Diagnostics must not reference the removed global `all`"
assert "function summarizeFindings(inventory)" in app and "inventory.flatMap" in app, "Diagnostics must aggregate per-device findings"
assert (root/"Assets/UNIFY_CONTROL.ico").is_file(), "Application icon missing"
version=(root/"VERSION").read_text().strip()
assert version == "1.7.0", f"Unexpected release version: {version}"
print("Frontend release invariants passed.")
PYVALIDATE
echo 'UNIFY CONTROL portable validation passed.'
