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
`AMIOS_EXPORT_WINDOWS=1`: enable SDL window export. `AMIOS_VIRTUAL_RTG=1`: add 4K mode. `AMIOS_RTG_MONITOR=0`: RTG monitor (set 1 if your UAEGFX uses monitor 1). `AMIOS_EXPORT_INPUT=1`: enable experimental mouse and keyboard routing. `AMIOS_EXPORT_RESIZE=1`: opt in to the Stage 4 style real Intuition size-gadget drag (requires input=1; disabled by default). `AMIOS_HIDE_ROOT=1`: hide original monitor after exports appear; Ctrl+Alt+F12 toggles it.

## Known limitations
Close requests now use AmiOSBridge's original protocol-2 command port; the host window remains until the guest window actually closes. Real live resize has been added as an opt-in Stage 4-style synthetic Intuition size-gadget gesture, **not yet build- or runtime-validated** on 8.4.1. Input, scrolling, close handling, resize, native screens, and custom-P96 screens still require further runtime testing. Overlapping guest windows are not sampled; the host retains previous texture. The new code uploads complete windows rather than an incremental dirty-tile list. Test with input disabled initially. This is not a production release.

The historic RC1 source is retained separately rather than indiscriminately overlaid on 8.4.1.
## Versioning and project home

- **AmiOS68K v0.10** is the integration project version; **Amiberry 8.4.1** is the underlying emulator version.
- **AmiOSBridge v0.10** is the guest helper package (unchanged historic Stage 4d-R2 executable, guest wire protocol 2).
- [AmiOS68K project homepage](https://github.com/das-portal/AmiOS68K) shows the current architecture and separates old R17E7 work from the active development.
- This branch remains experimental until compiled and runtime-validated. Do not merge the existing draft PR solely because its naming is now aligned.

## Next controlled v0.10 test (post-Stage 4 port)

1. Close all programs and shut down AmigaOS; back up the boot HDF.
2. Pull and rebuild branch `amios/8.4.1-portable-window-export-p0`. Confirm `C:AmiOSBridge` is the corrected **2452-byte** executable.
3. First test **Linux X/close** with `AMIOS_EXPORT_RESIZE=0`. Only an Amiga window with a working close gadget should disappear. Noncloseable windows should remain open (no forced kill).
4. For resizing, start the launcher explicitly with `AMIOS_EXPORT_INPUT=1 AMIOS_EXPORT_RESIZE=1 ~/.local/Amiberry/amios-run --config Configurations/YourOS.uae`. Test a *single disposable Workbench drawer* first. Observe whether contents update at 1:1 while dragging Labwc borders; verify both enlarging and shrinking.
5. If the guest becomes unresponsive, exit cleanly and rerun with `AMIOS_EXPORT_RESIZE=0`. Do not combine resize test with input or focus changes not already proven on this machine.

The installer defaults continue to **input=0, resize=off, root visible**. Changes are experimental, not confirmed working yet.

## Live resize diagnostics (Stage 4 input-ownership port)

With the corrected 2452-byte AmiOSBridge installed, build the latest development branch and launch once with:

```bash
AMIOS_EXPORT_INPUT=1 AMIOS_EXPORT_RESIZE=1 ~/.local/Amiberry/amios-run --log --config Configurations/YourOS.uae
```

Only test a disposable Workbench drawer initially. The log now distinguishes: Wayland host resize event; synthetic guest input activation and selected mouse index; start/target size-gadget coordinates; held and released drag; actual changed Intuition geometry versus timeout. If the Wayland window expands to a black area and snaps back, collect the `AmiOS P0 resize:` log lines, particularly the device index and guest geometry. **This is a candidate repair, not a tested fix.** Re-run without `AMIOS_EXPORT_RESIZE=1` to disable the experiment.

### 17 Oct: short-drag hold correction

A live test showed the pointer movement at 23:525 followed by button release at 23:542 (17 ms); guest Intuition remained 282x216. The experimental host now holds the synthetic size-gadget press for **at least 180 ms after moving**, reasserts pointer position during the hold, and logs `host injection mouse=... button-mask=... mousehack=... tablet=...`, `released after ... ms`, and actual Intuition geometry. This is **not yet validated**. Do not merge until tested. If the guest still fails to resize, investigate a direct `SizeWindow`/`ChangeWindowBox` command through an updated guest helper instead of adding more synthetic-mouse timing patches.
