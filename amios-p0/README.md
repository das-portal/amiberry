# AmiOS P0 — user-owned home-directory install

This is a **vanilla Amiberry baseline build**, not the dual-display or Wayland export implementation. It does not replace R17E7.

## Build on Debian

```bash
sudo apt update
sudo apt install -y git cmake ninja-build build-essential pkg-config
git clone --branch amios/wayland-foundation-p0 https://github.com/das-portal/amiberry.git ~/AmiOS84-test
cd ~/AmiOS84-test
bash amios-p0/build-test.sh
```

If you already cloned the branch, update it instead:

```bash
cd ~/AmiOS84-test
git pull --ff-only
bash amios-p0/build-test.sh
```

CMake may require additional Debian development libraries; install the specific missing dependencies according to the upstream Amiberry build guide.

## Layout

The test executable and launcher are installed to `~/Amiberry/amiberry` and `~/Amiberry/run-amiberry.sh`. The installer creates user-owned `configs/`, `roms/`, `harddrives/`, `whdboot/`, `plugins/`, `controllers/`, `logs/` folders there. It copies existing bundled source assets when present without overwriting existing files.

`AMIBERRY_HOME_DIR` and `AMIBERRY_PLUGINS_DIR` are set in the launcher. Amiberry can maintain other per-user paths depending on its settings; confirm the effective paths in its Paths GUI. The installer does **not** move ROMs, HDFs or personal configurations, or alter existing installation paths.

```bash
~/Amiberry/run-amiberry.sh
# To test an existing configuration, make a copy first:
~/Amiberry/run-amiberry.sh -f /absolute/path/to/test-copy.uae
```

Build files remain in `~/AmiOS84-test/build-amios-p0/`. Build logs: `~/Amiberry/logs/configure.log` and `~/Amiberry/logs/build.log`.

**Safety:** if `~/Amiberry` already contains an installation, this installer replaces its `amiberry` executable. Back it up or rename that directory before running. No system directories are written by the build script. Do not run this emulator at the same time as R17E7.

## Baseline test

Stop existing emulator, then test RTG Workbench, EditPad input, ProTracker native PAL open/close and returning to Workbench. Ordinary emulator windows are expected: native Wayland export and retained RTG are future work.
