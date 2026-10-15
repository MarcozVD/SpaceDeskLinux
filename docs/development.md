# Development setup

Two machines:

| Role | Machine | Notes |
|---|---|---|
| Analysis + development | Windows 11 PC running the official spacedesk server | Ghidra, REA, probe |
| Test target | Debian 12 laptop on the same LAN, i5-7200U / Intel HD 620, GNOME on Wayland, reached over SSH (key auth) | builds and runs `sdviewer` |

## Windows: reverse-engineering toolchain

Everything lives under `tools/` (git-ignored except `tools/probe/`).

| Tool | Version | Path |
|---|---|---|
| Temurin JDK | 21.0.12.1 | `tools/jdk-21.0.12.1+1` |
| Ghidra | 12.1.4 | `tools/ghidra_12.1.4_PUBLIC` |
| REA (morluto/rea) | `rea-agents@6.5.0` | via `npx` |
| Python | 3.13 (`py -3.13`) | probe only, stdlib |

Downloads were verified against the Adoptium API checksum and the Ghidra release SHA-256.

REA is registered as an MCP server for this project only (Claude Code local scope):

```powershell
claude mcp add --scope local rea `
  --env REA_ANALYSIS_PROVIDER=ghidra `
  --env "GHIDRA_INSTALL_DIR=$PWD\tools\ghidra_12.1.4_PUBLIC" `
  --env "JAVA_HOME=$PWD\tools\jdk-21.0.12.1+1" `
  -- cmd.exe /d /c npx -y rea-agents@6.5.0 mcp
```

Run it from PowerShell: Git Bash rewrites `/d /c` into drive paths.
Check the provider with `npx -y rea-agents@6.5.0 doctor --json` (the `ghidra*` checks must be healthy).

### Analysis targets

Vendor files are copied into `re/targets/` (git-ignored) and never committed:

| Target | Source |
|---|---|
| `re/targets/server/spacedeskService.exe` | `C:\Program Files\datronicsoft\spacedesk\` (SHA-256 `c9fa7955…8ccff`) |
| `re/targets/html5-viewer/` | `http://viewer.spacedesk.net/` (`index.html`, `spacedesk.min.js` v0.9.10) |

## Protocol probe

`tools/probe/sd_probe.py` is a stdlib-only client used to verify every `[live]` fact.

```sh
# H.264 (extended identification), saves captures/stream.h264
python3 tools/probe/sd_probe.py --host <server> --encoding h264 --frames 120 --out captures
# JPEG fragments as files
python3 tools/probe/sd_probe.py --host <server> --encoding jpeg --frames 10 --out captures
# HTML5-style identification over WebSocket
python3 tools/probe/sd_probe.py --host <server> --transport ws --legacy --encoding jpeg
```

While connected, the server adds a virtual monitor on the Windows PC. The server stores
settings per device id under `HKLM\SOFTWARE\datronicsoft\spacedesk\<device id>`; test ids can
be deleted there afterwards.

## Debian: building the viewer

Build dependencies (needs root):

```sh
su -c 'apt-get update && apt-get install -y build-essential cmake pkg-config git \
  libsdl2-dev libavcodec-dev libavutil-dev libswscale-dev libturbojpeg0-dev \
  libva-dev vainfo intel-media-va-driver ffmpeg'
```

Build and run:

```sh
cmake -S viewer -B build && cmake --build build -j
./build/sdviewer -v <windows-ip>          # fullscreen, H.264
./build/sdviewer -w -j <windows-ip>       # windowed, JPEG
```

Running it from SSH on the GNOME session:

```sh
XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY=wayland-0 SDL_VIDEODRIVER=wayland ./build/sdviewer -v <windows-ip>
```

Hotkeys: `Ctrl+Alt+F` toggles fullscreen, `Ctrl+Alt+Q` quits.
