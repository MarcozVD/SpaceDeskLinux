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

No vendor code or binaries are in this repository. Analysis targets live in `re/targets/`
and analysis tools in `tools/`, both git-ignored.

## Layout

| Path | Contents |
|---|---|
| `docs/protocol.md` | Protocol specification (living document) |
| `docs/re-log.md` | How each finding was obtained (Ghidra addresses, probes) |
| `docs/development.md` | Toolchain, test machines, build and run instructions |
| `viewer/` | `sdviewer`: C11, SDL2, FFmpeg (H.264 + VA-API), TurboJPEG |
| `tools/probe/sd_probe.py` | Stdlib-only protocol probe |

## Quick start (Debian 12)

Build without touching the target system (Docker), then copy the binary:

```sh
docker run --rm -v "$PWD":/src debian:bookworm sh /src/viewer/docker-build.sh
./dist/sdviewer <windows-ip>
```

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
- [x] Viewer source: H.264/JPEG/BGRX decoding, fullscreen, mouse, keyboard, touch
- [x] Viewer runs on Debian 12 (GNOME Wayland, VA-API H.264, ~30 fps)
- [ ] Input verified end to end (mouse, keyboard, touch)
- [ ] Slow start (~6 fps for the first seconds) and 60 fps target
- [ ] UDP discovery
- [ ] Audio (Speaker session)
- [ ] Pen, multi-touch, rotation
- [ ] `.deb` package
