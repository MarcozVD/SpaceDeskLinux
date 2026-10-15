# SpaceDeskLinux

Goal: native Linux (Debian) **viewer** for spacedesk. Windows PC runs official server; Debian acts as extra monitor.

## Rules
- Clean-room: protocol facts go to `docs/protocol.md` with source tag (`[js]`, `[live]`, `[bin]`, `[?]`). Implementation code is written from that doc, never copied from vendor code.
- Vendor artifacts stay in `re/targets/` (git-ignored). Never commit them.

## Environment (Windows dev host)
- spacedesk server installed: `C:\Program Files\datronicsoft\spacedesk\spacedeskService.exe` (native x64 C++, static OpenSSL 3.1.1 + libjpeg-turbo 3.1.0, Media Foundation H.264). Listens TCP/UDP 28252.
- REA MCP (morluto/rea, `rea-agents@6.5.0`) registered at local scope with Ghidra provider.
- Ghidra 12.1.4: `tools/ghidra_12.1.4_PUBLIC`; JDK 21: `tools/jdk-21.0.12.1+1`.
- Python: `py -3.13 -I tools/probe/sd_probe.py ...` (probing local server creates a temporary virtual monitor).
- HTML5 viewer source: `re/targets/html5-viewer/spacedesk.min.js` (protocol classes around lines 3280-3900, KVM 5500-5870).
- Server binary copy for Ghidra: `re/targets/server/spacedeskService.exe`. Key functions listed in `docs/re-log.md`.

## Debian test host
- `ssh marcozv@192.168.1.10` (key auth from this PC). Debian 12, Intel HD 620, GNOME Wayland session on seat0.
- `marcozv` is not in sudoers: package installs need the user to run `su -c '...'`.
- Run GUI from SSH: `XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 SDL_VIDEODRIVER=wayland`.

## Viewer
- `viewer/` (C11, SDL2, FFmpeg/VA-API, TurboJPEG). Build: `cmake -S viewer -B build && cmake --build build -j`.
- Pointer: touch hover (type 12, flag 0x1) for absolute position; mouse packets only for buttons/wheel (server ignores MOUSEEVENTF_ABSOLUTE).
