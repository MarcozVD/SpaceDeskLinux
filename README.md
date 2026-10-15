# SpaceDeskLinux

Native spacedesk **viewer** for Linux (Debian): turn a Debian machine into an extra
monitor for a Windows PC running the official spacedesk server.

The official Linux option is the HTML5 viewer in a browser (JPEG only). This project
provides a native client with H.264 (VA-API hardware decoding), lower latency and input forwarding.

## Approach

Clean-room interoperability work:

1. Document the wire protocol in [`docs/protocol.md`](docs/protocol.md) from observed traffic
   and from analysis of the official clients/server ([`docs/re-log.md`](docs/re-log.md)).
2. Write an independent implementation from that document only.

No spacedesk vendor code or binaries are in this repository. Analysis targets live in `re/targets/`
and analysis tools in `tools/`, both git-ignored. The only third-party code is the vendored GUI
toolkit (see [Third-party code](#third-party-code)).

## Layout

| Path | Contents |
|---|---|
| `docs/protocol.md` | Protocol specification (living document) |
| `docs/re-log.md` | How each finding was obtained (Ghidra addresses, probes) |
| `docs/development.md` | Toolchain, test machines, build and run instructions |
| `viewer/` | `sdviewer`: C11, SDL2, FFmpeg (H.264 + VA-API), TurboJPEG, Nuklear UI |
| `tools/probe/sd_probe.py` | Stdlib-only protocol probe |

## Features

`sdviewer` mirrors the official Windows client:

- **Menu bar** (Spanish): Archivo (disconnect, quit); Ver (status bar, full screen); Funciones (display,
  keyboard, mouse remote / mouse local as touch, touchscreen absolute / touchpad relative / touch off,
  audio speaker); Ajustes (resolutions, quality/performance, automatic connect, full screen on connect,
  keep monitor active, reset, advanced); Ayuda (manual, GitHub project, about).
- **Connect screen** with LAN discovery (see below), manual address entry and a recent-servers list.
- **Mouse remote** (default): clicking the video captures the pointer and sends relative mouse packets,
  like the official Raw Input path. **Mouse local** drives one absolute touch contact instead.
- **Keyboard** sent by scancode + virtual key with extended/key-up flags.
- **Audio** through a separate Speaker session (PCM 48 kHz stereo s16) played via SDL (PipeWire/Pulse),
  bounded to ~150 ms latency.
- **Settings** persisted in `~/.config/sdviewer/settings.ini`.

### Usage

```
sdviewer [-w] [--no-hw] [-v] [server]
  -w, --windowed   start windowed even if "full screen upon connect" is set
      --no-hw      disable VA-API decoding for this run
  -v, --verbose    log connection details and statistics
```

The server address is optional: without it the connect screen appears and offers the servers found by
LAN discovery. Hotkeys: **Alt+Enter** toggles full screen, **Ctrl+Alt+M** releases the mouse and opens
the menu, **Alt+Shift** leaves "mouse local" touch emulation.

## Quick start (Debian 12)

Build without touching the target system (Docker), then copy the binary:

```sh
docker run --rm -v "$PWD":/src debian:bookworm sh /src/viewer/docker-build.sh
./dist/sdviewer <windows-ip>
```

Install for the current user with a GNOME menu entry ("SpaceDesk Linux"), no root needed:

```sh
sh packaging/install-user.sh dist/sdviewer <windows-ip>
```

The settings live in `~/.config/sdviewer/settings.ini` (the old `~/.config/sdviewer/server` file written
by earlier versions is migrated); the log in `~/.cache/sdviewer.log`.

Or natively:

```sh
su -c 'apt-get install -y build-essential cmake pkg-config libsdl2-dev libavcodec-dev \
  libavutil-dev libturbojpeg0-dev intel-media-va-driver'
cmake -S viewer -B build && cmake --build build -j
./build/sdviewer <windows-ip>
```

Details in [`docs/development.md`](docs/development.md).

## Status

- [x] Protocol: identification (legacy and extended), frames, flow control, input
- [x] H.264 negotiation (extended identification, encoding 4)
- [x] Viewer source: H.264/JPEG/BGRX/YUV decoding, fullscreen, mouse, keyboard, touch
- [x] Viewer runs on Debian 12 (GNOME Wayland, VA-API H.264, ~33 fps)
- [x] UDP discovery (LAN server list on the connect screen)
- [x] Audio (Speaker session, PCM 48 kHz stereo, PipeWire/Pulse)
- [x] Feature parity with the official viewer (menus, settings, mouse modes, audio, discovery)
- [ ] Input verified by a human (mouse remote, mouse local, keyboard layout, touchscreen)
- [ ] Slow start (~6 fps for the first seconds) and 60 fps target
- [ ] Pen, multi-touch, rotation
- [ ] `.deb` package

## Third-party code

| Component | Version | License | Location |
|---|---|---|---|
| Nuklear (single-header GUI toolkit) | 4.13.2 | MIT / Public Domain | `viewer/third_party/nuklear/` |
