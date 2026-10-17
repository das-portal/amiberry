# AmiOS Amiberry 8.4.1 — P0

Experimental first-source build on the 8.4.1-era GitHub fork. This has not yet been built or run against Debian/AmigaOS.

## Install on AmiOS
```bash
git clone -b amios/8.4.1-portable-window-export-p0 https://github.com/das-portal/amiberry.git
cd amiberry
bash amios/install-p0.sh
```

The installer builds the branch and installs to `$HOME/AmiOS/.local/Amiberry`. The `amiberry.portable` marker uses upstream 8.4.1 portable mode, anchoring settings, config, drives and ROMs to the executable directory. It never moves/overwrites existing HDFs, ROMs or configs.

Run: `$HOME/AmiOS/.local/Amiberry/amios-run --config Configurations/YourOS.uae`.

## Testing
1. Copy/link your licensed ROMs and existing OS drives to `roms/`, `harddrives/`; edit the `.uae` paths to relative file paths and verify using `./amiberry --dump-paths`.
2. Select the virtual 4096x2160 P96 mode in guest ScreenMode. Verify Intuition actually reports the full dimensions; Overscan/P96Fix settings may be required.
3. Use the old Stage 4 `C:AmiOS68kAllocator` guest helper for non-overlapping window positioning; it is not automatically installed in this experimental branch.
4. Open EditPad and IconEdit; inspect independent SDL3 windows while leaving the original emulator monitor open.

## Flags
`AMIOS_EXPORT_WINDOWS=1`: enable SDL window export. `AMIOS_VIRTUAL_RTG=1`: add 4K mode. `AMIOS_RTG_MONITOR=0`: RTG monitor (set 1 if your UAEGFX uses monitor 1). `AMIOS_EXPORT_INPUT=1`: enable experimental mouse and keyboard routing. `AMIOS_HIDE_ROOT=1`: hide original monitor after exports appear; Ctrl+Alt+F12 toggles it.

## Known limitations
Input, scrolling, resize, close handling, native screens, and custom-P96 screens have not been reimplemented/validated in this P0. Overlapping guest windows are not sampled; the host retains previous texture. The new code uploads complete windows rather than an incremental dirty-tile list. Test with input disabled initially. This is not a production release.

The historic RC1 source is retained separately rather than indiscriminately overlaid on 8.4.1.