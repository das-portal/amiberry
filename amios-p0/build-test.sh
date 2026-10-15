#!/usr/bin/env bash
# Isolated, user-owned AmiOS P0 build; never writes to system directories.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build-amios-p0"
PREFIX="$HOME/Amiberry"
LOGDIR="$PREFIX/logs"
mkdir -p "$PREFIX" "$LOGDIR"
echo "AmiOS P0: building isolated Amiberry in $PREFIX"
command -v cmake >/dev/null || { echo "cmake missing"; exit 2; }
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release 2>&1 | tee "$LOGDIR/configure.log"
cmake --build "$BUILD" -j"$(nproc)" 2>&1 | tee "$LOGDIR/build.log"
BIN="$(find "$BUILD" -type f -name amiberry -executable -print -quit)"
[[ -n "$BIN" ]] || { echo "No executable found; inspect $LOGDIR/build.log"; exit 3; }
install -m 755 "$BIN" "$PREFIX/amiberry"
for dir in configs roms harddrives whdboot plugins controllers logs; do mkdir -p "$PREFIX/$dir"; done
# Copy only bundled resources present in this source checkout; do not overwrite user data.
for dir in data controllers whdboot; do
  if [[ -d "$ROOT/$dir" ]]; then cp -an "$ROOT/$dir/." "$PREFIX/$dir/" 2>/dev/null || true; fi
done
cat > "$PREFIX/run-amiberry.sh" <<'EOF'
#!/usr/bin/env bash
set -Eeuo pipefail
cd "$HOME/Amiberry"
export AMIBERRY_HOME_DIR="$HOME/Amiberry"
export AMIBERRY_PLUGINS_DIR="$HOME/Amiberry/plugins"
exec "$HOME/Amiberry/amiberry" "$@"
EOF
chmod 755 "$PREFIX/run-amiberry.sh"
echo "Installed under $PREFIX (no sudo needed to modify)"
echo "Launch: $PREFIX/run-amiberry.sh"
echo "With config: $PREFIX/run-amiberry.sh -f /path/to/copied-config.uae"
echo "Do not run alongside the existing AmiOS68K emulator."
