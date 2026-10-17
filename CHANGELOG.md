# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.3.3] - 2026-10-16

### Fixed

- Disconnect every ~60 s on a still desktop: with nothing to send, the viewer was dropped by the
  server's inactivity timer. It now answers every server Ping request (keepalive), in the display and
  audio sessions.
- The "click to control" hint no longer stays on screen forever: it is shown for 6 s after connecting
  and after releasing the mouse with Ctrl+Alt+M.

## [0.3.2] - 2026-10-15

### Fixed

- In full screen the menu bar shown from the top edge never hid again: it swallowed the click meant
  to close it. The bar now hides after the pointer leaves it (0.8 s, 3 s when opened with Ctrl+Alt+M,
  1 s from the top edge).

### Added

- Debian package attached to the releases as `sdviewer_0.3.2_amd64.deb`, built by
  `packaging/build-deb.sh`; the Docker build now produces it in `dist/` as well.

## [0.3.1] - 2026-10-15

### Fixed

- The menu bar stopped responding after closing a settings dialog from the connect screen: while a
  dialog was open Nuklear marked the bar window read-only and the flag was never cleared.

### Notes

- Prebuilt x86_64 binaries are published on
  [GitHub Releases](https://github.com/MarcozVD/SpaceDeskLinux/releases) starting with this version.

## [0.3.0] - 2026-10-15

### Added

- Redesigned interface: graphite palette with a single amber accent, Cantarell font with
  DejaVu/Noto fallback, a non-card connect screen with a server list, setup steps and key-cap
  shortcuts, restyled dialogs and status bar.
- `--version` flag; `-v` prints per-frame timing (wait, decode, transfer, upload).

### Changed

- Steady 60 fps at native resolution: the identification requests twice the configured fps (capped
  at 120), H.264 frames are acked as soon as they are received, and a decoded picture larger than the
  video area is scaled on the GPU with VA-API video processing.
- The viewer automatically uses the `i965` VA-API driver when the default `iHD` build lacks video
  processing (override with `LIBVA_DRIVER_NAME`).

## [0.2.0] - 2026-10-14

### Added

- Nuklear menus equivalent to the official viewer, persistent settings file, LAN discovery, audio
  (Speaker session), mouse remote and mouse-as-touch modes, touchscreen absolute/relative modes,
  keyboard by scancode, automatic reconnect, and a per-user install script.

## [0.1.0]

- First H.264/JPEG viewer (see the git history for the first commits).

[0.3.3]: https://github.com/MarcozVD/SpaceDeskLinux/compare/v0.3.2...v0.3.3
[0.3.2]: https://github.com/MarcozVD/SpaceDeskLinux/compare/v0.3.1...v0.3.2
[0.3.1]: https://github.com/MarcozVD/SpaceDeskLinux/compare/v0.3.0...v0.3.1
[0.3.0]: https://github.com/MarcozVD/SpaceDeskLinux/compare/v0.2.0...v0.3.0
