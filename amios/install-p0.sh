#!/usr/bin/env bash
set -euo pipefail
ROOT="${AMIOS_ROOT:-$HOME/AmiOS/.local/Amiberry}"
BUILD="${BUILD_DIR:-build-amios-p0}"
test -f src/osdep/amios_p0_window_export.cpp || { echo "Run from AmiOS P0 source checkout" >&2; exit 1; }
mkdir -p "$ROOT"
cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DUSE_OPENGL=ON -DUSE_VULKAN=OFF
cmake --build "$BUILD" -j "${JOBS:-2}"
install -m 755 "$BUILD/amiberry" "$ROOT/amiberry"
touch "$ROOT/amiberry.portable"
for dir in Configurations Settings roms harddrives whdboot data plugins savestates screenshots floppies logs; do mkdir -p "$ROOT/$dir"; done
if [ -d data ]; then cp -an data/. "$ROOT/data/"; fi
if [ -d whdboot ]; then cp -an whdboot/. "$ROOT/whdboot/"; fi
cat > "$ROOT/amios-run" <<'RUNNER'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd "$HERE"
export AMIOS_EXPORT_WINDOWS="${AMIOS_EXPORT_WINDOWS:-1}"
export AMIOS_VIRTUAL_RTG="${AMIOS_VIRTUAL_RTG:-1}"
export AMIOS_EXPORT_INPUT="${AMIOS_EXPORT_INPUT:-0}"
export AMIOS_HIDE_ROOT="${AMIOS_HIDE_ROOT:-0}"
export AMIOS_RTG_MONITOR="${AMIOS_RTG_MONITOR:-0}"
exec "$HERE/amiberry" "$@"
RUNNER
chmod +x "$ROOT/amios-run"
echo "Installed portable AmiOS Amiberry at $ROOT"
echo "Start: $ROOT/amios-run --config Configurations/YourOS.uae"
echo "NOTE: ROMs/OS disks and the historical Stage 4 guest allocator are not installed automatically."
