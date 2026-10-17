# AmiOS68K v0.10 — Amiberry 8.4.1 development baseline

Experimental first-source build on the 8.4.1-era GitHub fork. This has not yet been built or run against Debian/AmigaOS.

## Install on AmiOS
```bash
git clone -b amios/8.4.1-portable-window-export-p0 https://github.com/das-portal/amiberry.git
cd amiberry
bash amios/install-p0.sh
```

Run this as your normal `amios` user (do not use `sudo`). The installer builds the branch and installs to `$HOME/.local/Amiberry`. The `amiberry.portable` marker uses upstream 8.4.1 portable mode, anchoring settings, config, drives and ROMs to the executable directory. It never moves/overwrites existing HDFs, ROMs or configs. The application directory itself can be moved because the portable mode locates its data relative to the executable; review any absolute HDF/ROM paths saved in existing `.uae` configurations.

On your AmiOS login, run: `~/.local/Amiberry/amios-run --config Configurations/YourOS.uae`.

If you previously installed to `~/AmiOS/.local/Amiberry` and have already moved the entire `Amiberry` folder to `~/.local/`, **do not reinstall** solely to fix the location. Check that `amiberry`, `amiberry.portable`, and `amios-run` are together in `~/.local/Amiberry/` and run `~/.local/Amiberry/amiberry --dump-paths`.

## Testing
1. Copy/link your licensed ROMs and existing OS drives to `roms/`, `harddrives/`; edit the `.uae` paths to relative file paths and verify using `./amiberry --dump-paths`.
2. Select the virtual 4096x2160 P96 mode in guest ScreenMode. Verify Intuition actually reports the full dimensions; Overscan/P96Fix settings may be required.
3. Install [AmiOSBridge v0.10](https://github.com/das-portal/AmiOS68K/tree/main/bridge/v0.10) (`C:AmiOSBridge`), based on the original Stage 4 allocator for non-overlapping window positioning; it is not automatically installed in this experimental branch.
4. Open EditPad and IconEdit; inspect independent SDL3 windows while leaving the original emulator monitor open.

## Flags
`AMIOS_EXPORT_WINDOWS=1`: enable SDL window export. `AMIOS_VIRTUAL_RTG=1`: add 4K mode. `AMIOS_RTG_MONITOR=0`: RTG monitor (set 1 if your UAEGFX uses monitor 1). `AMIOS_EXPORT_INPUT=1`: enable experimental mouse and keyboard routing. `AMIOS_HIDE_ROOT=1`: hide original monitor after exports appear; Ctrl+Alt+F12 toggles it.

## Known limitations
Input, scrolling, resize, close handling, native screens, and custom-P96 screens have not been reimplemented/validated in this P0. Overlapping guest windows are not sampled; the host retains previous texture. The new code uploads complete windows rather than an incremental dirty-tile list. Test with input disabled initially. This is not a production release.

The historic RC1 source is retained separately rather than indiscriminately overlaid on 8.4.1.
## Versioning and project home

- **AmiOS68K v0.10** is the integration project version; **Amiberry 8.4.1** is the underlying emulator version.
- **AmiOSBridge v0.10** is the guest helper package (unchanged historic Stage 4d-R2 executable, guest wire protocol 2).
- [AmiOS68K project homepage](https://github.com/das-portal/AmiOS68K) shows the current architecture and separates old R17E7 work from the active development.
- This branch remains experimental until compiled and runtime-validated. Do not merge the existing draft PR solely because its naming is now aligned.
