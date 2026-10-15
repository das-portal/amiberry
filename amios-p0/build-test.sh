#!/usr/bin/env bash
# Isolated, user-owned AmiOS P0 build; never writes to system directories.
set -Eeuo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build-amios-p0"
PREFIX="$HOME/Amiberry"
LOGDIR="$PREFIX/logs"
mkdir -p "$PREFIX" "$LOGDIR"
echo "AmiOS P0: building isolated Amiberry in $PREFIX"
# Check the Debian development dependencies against the desktop CMake build.
# No packages are installed automatically; report the complete missing set.
if command -v dpkg-query >/dev/null 2>&1 && command -v apt-get >/dev/null 2>&1; then
  packages=(build-essential git cmake pkg-config libsdl3-dev libsdl3-image-dev
    libflac-dev libmpg123-dev libpng-dev zlib1g-dev libmpeg2-4-dev
    libserialport-dev libportmidi-dev libenet-dev libpcap-dev libzstd-dev
    libcurl4-openssl-dev nlohmann-json3-dev libdbus-1-dev
    libavformat-dev libavcodec-dev libavutil-dev libswscale-dev libswresample-dev)
  missing=()
  for pkg in "${packages[@]}"; do
    if ! dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -qx 'install ok installed'; then
      missing+=("$pkg")
    fi
  done
  if (("${#missing[@]}" > 0)); then
    echo "Missing Debian build packages:"
    printf '  %s\n' "${missing[@]}"
    printf '\nInstall together with:\n  sudo apt update && sudo apt install -y'
    printf ' %q' "${missing[@]}"
    printf '\n'
    exit 2
  fi
fi
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
