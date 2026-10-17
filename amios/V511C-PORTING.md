# AmiOS v0.10.2 — 8.3.0 v5.11c compatibility port (work plan)

**Status: reference and compatibility audit only; no host or guest runtime code is changed by this document.** This branch is independent of the working v0.10 P0 branch. Do not build/install it as a v5.11c port until functional code has been reviewed and compiled.

## Authoritative reference

Use the complete **AmiOS68k-Amiberry-8.3.0-v5.11c-complete-baseline.zip** archived in the AmiOS Library, not selected R17E7 files or the later 8.3.0 MultiP96 RC1 source.

- Archive SHA-256: `f8d09c1db251cade739fad84a61c6f841b2654dedbb202689701de86e15776ab`
- Legacy `overlay/src/osdep/amios68k_window_export.cpp` SHA-256: `445c81dc38c57f9bb3b857fea88d4f4f957bc8834aa365462c4defb0cb6655d7` (3,042 lines)
- Legacy protocol-v5 `guest/AmiOS68kAllocator` SHA-256: `63d288613eafcd3bef6eae71576788622a2f70227cfd3192c78dc25a7d5d0e30`
- Includes the original 68k source, mini-assembler, `amios68k_window_model.h`, complete overlay, integration tests, and a handover describing actual Labwc test results.
- Additional historical descendant: [old AmiOS68K v6/R17E7 host source](https://github.com/das-portal/AmiOS68K/blob/main/src/osdep/amios68k_window_export.cpp). This is **not** the validated v5.11c snapshot; do not accidentally substitute it.

## Features demonstrated in archived v5.11c

- Real-time Intuition live resize and redraw (not texture scaling); temporary transparent host enlargement
- Host move, resize, focus, close; genuine guest ownership rather than simulated independent programs
- Pointer / keyboard / right-Amiga hotkeys; explicit guest `ActiveWindow` and focus command
- Background texture retention, frontmost capture ownership, widget shelf, transient MagicMenu popups
- Host-side scrollbars with genuine PropGadget state and FreeWheel scroll bridge
- Genuine 1920x2048x32 P96 guest screen, with an ordinary overlapping-window arena above a protected widget shelf

## Architecture decision **before** transplanting

The current 8.4.1 P0 relies on **4096x2160 non-overlapping** Stage 4 backing and **protocol-2** AmiOSBridge; v5.11c instead relies on the **1920x2048 overlapping** Stage 5 model and **protocol-5** guest helper (with guest focus). Blindly replacing the exporter breaks its framebuffer/input/guest assumptions.

Preferred migration path:

1. Preserve v0.10 P0 as rollback and keep current `~/.local/Amiberry/` installation untouched.
2. Port the **whole coherent v5.11c host interaction subsystem** onto 8.4.1 SDL3 event, rendering, and monitor APIs, preserving its model tests. Adapt build glue rather than replacing newer emulator platform/core files.
3. Decide on the matching guest layout/protocol as a paired change: either use the preserved v5 helper and original v5 arena, or *explicitly adapt* proven v5 focus/resize behaviour to Stage 4 non-overlap with a newly versioned helper. Never mix v5 mailbox offsets with v2 helper.
4. Port the precise entrypoints: `amiberry.cpp` event dispatch, `amiberry_gfx.cpp` presenting, `display_modes.cpp` mode injection, and old `picasso96.cpp` only for minimal relevant hooks. Check 8.4.1 render/input API differences.
5. Build a separate executable and test executable compile, helper assembly reproducibility, smoke boot, multiple windows, guest focus, key/click input, close, resize (expand/shrink), draggable host titlebars, scrolling, menus and native Amiga screens.
6. Only after these pass, provide a paired, rollback-safe v0.10.2 installer and consider merging.

## Cause of failed P0 resizing

In the P0 log, Wayland requested 495x376, the synthetic left button was held 185 ms with button mask `00000001`, but real Intuition `Window.Width/Height` stayed 282x216. This proves host-side input-state updates, **not** that Intuition recognised a size gadget. The v5 source includes guest focus/activation and synchronised input/capture ownership absent from P0, so further synthetic delay changes are not the preferred path.

## Rules

- Do not overlay 8.3.0 `amiberry.cpp` or `picasso96.cpp` on 8.4.1.
- Keep original 8.3.0 archives and source history intact.
- No uncompiled fix or new guest binary should be labelled a working release.
- Preserve actual user progress: working window creation, 4K allocator placement, closing.
