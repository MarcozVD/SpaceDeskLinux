# spacedesk wire protocol (reverse-engineered notes)

Status: **display path complete enough for a viewer** (JPEG, raw, H.264, input). Audio and
discovery documented; TLS still open. Each fact carries its source:

- `[js]`   official HTML5 viewer `spacedesk.min.js` v0.9.10 (viewer.spacedesk.net)
- `[live]` observed with `tools/probe/sd_probe.py` against spacedesk server 2.2.x on Windows 11
- `[bin]`  static analysis of `spacedeskService.exe` (Ghidra 12.1.4 via REA; addresses refer to
           SHA-256 `c9fa7955…8ccff`)
- `[viewer]` static analysis of the official Windows viewer `spacedeskWindowsVIEWER.exe` v0.9.48 BETA
           (Ghidra 12.1.4 via REA; addresses refer to SHA-256 `3f8d127d…3ec4`)
- `[reg]`  server registry state `HKLM\SOFTWARE\datronicsoft\spacedesk`
- `[?]`    hypothesis, not verified

All integers are **little-endian**. Protocol version **4.8** (server accepts minor 7 or 8) `[bin]`.

## Transport

| Item | Value | Source |
|---|---|---|
| TCP port | 28252 | `[js]` `[live]` |
| Raw TCP | 128-byte header + `header[4]` payload bytes, back to back | `[live]` |
| WebSocket | `ws://<server>:28252/`, one packet per binary message; server detects HTTP `GET` upgrade on the same port | `[js]` `[live]` `[bin]` |
| UDP 28252 | discovery: client broadcasts `SPACEDESK-NET-CLIENT`, server replies with a 308-byte record (see [LAN discovery](#lan-discovery-udp)) | `[bin]` `[viewer]` `[live]` |
| TLS | `L4SocketTcpCommSSL`, enabled by server setting `Layer4EncryptionSsl` (off by default) | `[bin]` |

## Packet layout

```
+----------------------+-----------------------------+
| header (128 bytes)   | payload (header[4] bytes)   |
+----------------------+-----------------------------+
header[0]  u32 type
header[4]  u32 countByteDataFollowingHeader
```

### Header types

Server names are `PROTOCOL_SPCDSK_SMPL_HEADER_TYPE_*` `[bin]`. The HTML5 viewer uses older names `[js]`.

| Value | Server name | HTML5 name | Direction |
|---|---|---|---|
| 0 | IDENTIFICATION | Identification | C→S |
| 1 | PING (has Option @12) | Ping | both |
| 2 | DISPLAY_FRAMEBUFFER | FrameBuffer | S→C |
| 3 | DISPLAY_VISIBILITY | Visibility | S→C |
| 4 | DISPLAY_SETTINGS | CursorPosition | S→C (extended clients) |
| 5 | UNUSED_01 | CursorBitmap | — |
| 6 | EVT_DISPLAY_SETTINGS | PowerSuspendHibernate | C→S |
| 7 | (flow control ack) | FlowControlAck | C→S |
| 8 | ERROR | Disconnect | both |
| 9 | (rotation) | Rotation | C→S |
| 10 | EVT_INPUT_MOUSE | Mouse | C→S |
| 11 | EVT_INPUT_KEYBOARD | Keyboard | C→S |
| 12 | EVT_INPUT_TOUCH | Touch | C→S |
| 13 | EVT_INPUT_PEN | — | C→S |
| 14 | AUDIO_SPEAKER | — | S→C (speaker session) |

Types accepted from an identified display client: bitmask `0x3ec2` = {1, 6, 7, 9, 10, 11, 12, 13};
type 8 closes the session; anything else is logged and ignored `[bin]` (`L5ThreadReceiver::Run`, `0x1400e61f0`).

## Session flow

1. Client connects.
2. Client sends **Identification** (128 + 334 = 462 bytes).
3. Server validates it (rules below). On failure it closes the socket after ~2 s `[bin]`.
4. Extended clients get `DISPLAY_SETTINGS` (type 4, all zero observed) `[live]` `[bin]`.
5. Server sends `DISPLAY_VISIBILITY` (`@8` visible, `@12` 1 observed), then 1–2 `PING`.
6. Server streams `DISPLAY_FRAMEBUFFER`. Client acks each **complete** frame with type 7.
7. Client ends with type 8 (28 bytes from HTML5 viewer) and closes.

`[live]`: idle desktop, ack per frame: JPEG ≈11 fps (4 fragments each); H.264 at requested 60 fps
with tiny P-frames. No Ping reply needed in 40 s sessions as long as acks flow.

## Identification (C→S)

### Validation `[bin]` (`L5Protocol::PacketIdentificationValidate`, inlined in `0x1400e61f0`)

| Check | Rule |
|---|---|
| `@4` | must be 334 (0x14e) |
| `@8`, `@12` | major == 4, minor ∈ {7, 8} |
| `@16` client type | < 4. 0, 1, 2 → **Display** session; 3 → **Speaker** (audio) session |
| `@20` OS type | < 5 |
| payload[0..77] | device identification, UTF-16LE, non-empty |
| encoding | if byte `@38` bit0 == 0 ("older clients"): `@24` must be 0 or 3 |

### Header fields

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | u32 | type = 0 | |
| 4 | u32 | 334 | |
| 8 | u32 | version major = 4 | |
| 12 | u32 | version minor = 8 | |
| 16 | u32 | client type | see above `[bin]`; HTML5 sends 1 |
| 20 | u32 | OS type 0..4 | `[reg]` real viewers: 1, 2 (Windows PCs report 2) |
| 24 | u32 | legacy encoding | used only when `@38` bit0 = 0 `[bin]` |
| 28 | u32 | legacy color/subsampling | used only when `@38` bit0 = 0; extended clients get 2 `[bin]` |
| 32 | u32 | quality 1..100 | JPEG / H.264 quality `[bin]` `[reg]` |
| 36 | u16 | **encoding** | used when `@38` bit0 = 1 `[bin]` `[live]`; official viewer reads it from registry value `CompressionType` (default 5) `[viewer]` |
| 38 | u8 | **flags**; bit0 = extended identification | also makes server send DISPLAY_SETTINGS `[bin]`; official viewer sets bit0 = 1 `[viewer]` |
| 44 | i16 | fps: 0 → 30, 1 → 60, other = literal | `[bin]` `[reg]` (real clients 60, 90; 90 and 120 accepted by the server `[live]`). The server paces capture at the requested rate plus its own overhead: 60 → ~45 fps, 90 → 55–61, 120 → 60 with a 60 fps source; extra flow-control acks (2–3 frames in flight) do not raise it `[live]` |
| 46 | u16 | 0 `[js]`, 4 in the official viewer `[viewer]` |
| 48 | u32 | **number of resolutions** (0..9) in the list below `[viewer]`; HTML5 calls it "resolution mode" (1 = client, 2 = client + custom) `[js]`; stored by server `[bin]` |
| 52 + 4*i | u32 | width of resolution i, i = 0..n-1 `[viewer]` (HTML5: width `@52`, custom width `@56` = second list entry `[js]`) |
| 88 + 4*i | u32 | height of resolution i, i = 0..n-1 `[viewer]` (HTML5: height `@88`, custom height `@92` = second list entry `[js]`) |

The official Windows viewer builds this header as: `@16` = 0 (display session), `@20` = 1 (Windows),
`@24` = 3 if compression is on else 0 (legacy field), `@28` = 2, `@32` = quality, `@36` = encoding
from registry value `CompressionType` (default 5), `@38` = 1 (extended flag), `@44` = fps (default 60),
`@46` = 4, `@48` = number of resolutions in the list, widths at `@52 + 4*i` and heights at `@88 + 4*i`
`[viewer]` (`FUN_00435450`). Payload: device id UTF-16LE at payload offset 0 (max 39 chars), computer
name UTF-16LE at payload offset 78 `[viewer]`.

### Encoding values (server) `[live]`

| Value | Output | FrameBuffer payload |
|---|---|---|
| 0 | raw BGRX, 4 horizontal fragments | `w*h/4*4` bytes per fragment |
| 1 | raw YUV 4:2:0 (12 bpp), full frame | `w*h*3/2` bytes |
| 3 | JPEG, 4 horizontal fragments | baseline JFIF |
| 4 | **H.264 Annex-B**, full frame, not fragmented | starts `00 00 00 01 09` (AUD); SPS `27 64 00 20` = High profile, level 3.2 at 1280x720 |
| 5 | "choose best": server picks hardware H.264 or software JPEG | on the test PC it picked JPEG `[live]` |

Official Windows viewer UI labels `[viewer]`: "Off (RGBA 32bit)" = 0, Advanced "YUV" = 1,
"Software Encoding JPEG (force always)" = 3, "Hardware Encoding H264 (force always)" = 4,
"Choose best (Hardware or Software Encoding)" = 5.

### Payload (334 bytes)

| Offset | Content |
|---|---|
| 0..77 | device id, UTF-16LE (real viewers send a GUID `{…}`; HTML5 sends `1111122222`) |
| 78..333 | device name, UTF-16LE |

The server persists per-device settings under `HKLM\…\spacedesk\<device id>` `[reg]`
(`Width`, `Height`, `CompressionQuality`, `EncodingType`, `FramesPerSec`, `OsType`, `ClientDescription`, …).
For extended identification, each connection's `@36`/`@32`/`@44` values take effect `[live]`.
A viewer should use one stable GUID per installation.

## DISPLAY_FRAMEBUFFER (S→C)

| Offset | Type | Field | Source |
|---|---|---|---|
| 8 | u32 | width | `[js]` |
| 12 | u32 | height | `[js]` |
| 16 | u32 | pitch | `[js]` |
| 20 | u32 | pixel format, 21 = D3DFMT_A8R8G8B8 | `[js]` `[live]` |
| 24..36 | u32×4 | rect x1, y1, x2, y2 | `[js]` |
| 40 | u32 | encoding of this payload (table above) | `[js]` `[live]` |
| 48 | u32 | quality echo | `[live]` |
| 52 | u32 | ? (3 observed with JPEG) | `[live]` |
| 60 | u32 | fragment count (4 for JPEG/BGRX) | `[live]` |
| 64 | u32 | is fragment (1 = part of a fragmented frame) | `[js]` `[live]` |
| 68 | u32 | payload bytes (duplicate of @4) | `[live]` |

## Client → server packets `[bin]` (`OnPacketReceived_`, `0x1400e7180`) `[js]`

All are 128-byte headers with `@4 = 0` unless stated.

| Type | Fields |
|---|---|
| 1 Ping | `@12` option; option 2 = echo reply (refreshes inactivity timer) |
| 6 EVT_DISPLAY_SETTINGS | `@8` stream state: 1 or 2 (ignored if unchanged) — exact meaning `[?]` (likely start/pause stream) |
| 7 FlowControlAck | no fields; one per complete frame; also refreshes inactivity timer |
| 8 Error/Disconnect | HTML5 sends 28 bytes with only `@0 = 8` |
| 9 Rotation | `@8` rotation value |
| 10 Mouse | `@8 x`, `@12 y`, `@16 buttonData` (wheel delta), `@20 buttonFlags` (Win32 `MOUSEEVENTF_*`), `@24 = 0` |
| 11 Keyboard | `@8 vkey`, `@12 scancode`, `@16 flags` (Win32 `KEYEVENTF_*`). `flags & 4` (UNICODE): `@12` is the UTF-16 char. `flags & 0x10000`: "hardware" key path using `@8` |
| 12 Touch | `@8 x`, `@12 y`, `@16 client w`, `@20 client h`, `@24 flags` (0x1 absolute, 0x2 down, 0x4 up), `@28 timestamp ms`, `@32` contact index (server passes `@32 + 1`), `@36…` extra contact data `[?]` |
| 13 Pen | `@8`, `@12`, `@16`, `@20`, `@24`, `@28`, `@32`, bytes `@36..@39` — layout `[?]` |

MOUSEEVENTF: Move 0x1, LeftDown 0x2, LeftUp 0x4, RightDown 0x8, RightUp 0x10, MiddleDown 0x20,
MiddleUp 0x40, XDown 0x80, XUp 0x100, Wheel 0x800, HWheel 0x1000, VirtualDesk 0x4000, Absolute 0x8000 `[js]`.
KEYEVENTF: ExtendedKey 0x1, KeyUp 0x2, Unicode 0x4, ScanCode 0x8 `[js]`.

### Mouse (type 10) `[viewer]`

Official Windows viewer **"Mouse remote"** mode (`FUN_0042c300`) registers Raw Input and clips the
cursor to its window. Each `RAWMOUSE` event becomes a type-10 packet: `@8` `lLastX`, `@12` `lLastY`
(relative device deltas), `@16` wheel delta as an unsigned 16-bit value (e.g. `0xFF88` for −120),
`@20` `MOUSEEVENTF_*` flags translated from `RI_MOUSE_*` (MOVE `0x1` set when dx or dy ≠ 0,
LEFTDOWN `0x2`, LEFTUP `0x4`, RIGHTDOWN `0x8`, RIGHTUP `0x10`, MIDDLEDOWN `0x20`, MIDDLEUP `0x40`,
WHEEL `0x800`), `@24` = 0. The remote cursor is drawn by Windows into the video `[viewer]`.

Positioning the pointer with touch hover (the earlier viewer approach) is a workaround that makes
Windows treat input as touch; relative mouse packets are what the official viewer uses.

### Keyboard (type 11) `[viewer]`

The official viewer (`FUN_0042c470`) takes input from a `WH_KEYBOARD_LL` hook and sends `@8` `vkCode`,
`@12` `scanCode`, `@16` `flags` = (`LLKHF_EXTENDED` ? `0x1` : 0) | (`LLKHF_UP` ? `0x2` : 0). It does
not set `KEYEVENTF_SCANCODE`. `[live]` The server injects by scancode: a packet with VK = CapsLock
(`0x14`) and scancode = NumLock (`0x45`) toggled NumLock, with or without flag `0x8`. The Windows
keyboard layout therefore decides the characters (physical-key semantics), not the client layout.

### Touch (type 12) `[viewer]`

The official viewer (`FUN_0042c570`) sends `@8` x, `@12` y, `@16` width, `@20` height (coordinates in
the remote target space), `@24` flags: bit0 = absolute ("Touchscreen (absolute)"); cleared =
"Touchpad / Trackpad (relative)"; `0x2` down; `0x4` up. `@28` = `timeGetTime()`. Single contact only
(`@32` stays 0) `[viewer]`.

## LAN discovery (UDP) `[viewer]` `[live]`

Broadcast discovery, used by the official viewer to find servers on the LAN. Sender
`FUN_00437860` (`L4ThreadNetDiscovery` on the server side) `[viewer]` `[bin]`.

Request (client → server), UDP to port **28252**:

- ASCII `SPACEDESK-NET-CLIENT` followed by a NUL byte `[viewer]`.
- Sent 5 times: as broadcast on every interface and also as unicast to known servers `[viewer]` `[live]`.

Reply (server → client), sent from port 28252, **308 bytes** `[live]`:

| Offset | Type | Field |
|---|---|---|
| 0 | 256 bytes | computer name, UTF-16LE (`"Marcos"` observed) |
| 256 | u32 | IP (0 from the server; the viewer fills it in from the datagram sender address) |
| 260 | u32 | port = 28252 |
| 264 | u32 | 10 `[?]` |
| 272 | u32 | 4 |
| 276 | u32 | 8 |
| 280 | u32 | 11 `[?]` (likely capability flags) |

`@272 = 4` and `@276 = 8` are protocol version 4.8 `[live]`.

## Speaker session (audio) `[viewer]` `[live]`

Audio is a **separate TCP connection** to 28252, not part of the display session. Sender
`FUN_00439ed0`; server side `L5CommServiceSpeaker` `[viewer]` `[bin]`.

Identification: same 128-byte header as a display client, but `@16` client type = **3 (Speaker)**,
`@20` OS type = 1, the legacy encoding fields are zero, and the payload carries the same device id
and computer name `[viewer]`.

The server then streams header type **14** (`AUDIO_SPEAKER`) `[viewer]` `[live]`:

| Offset | Type | Field |
|---|---|---|
| 4 | u32 | payload bytes (1920) |
| 8 | u32 | channels (2) |
| 12 | u32 | sample rate (48000) |
| 16 | u32 | bits per sample (16) |
| 20 | u32 | payload bytes again (1920) |

Payload: PCM signed 16-bit little-endian, interleaved. 1920 bytes = 480 frames = 10 ms, about
100 packets per second. No acks needed `[live]`.

## Server architecture `[bin]` (RTTI class names)

| Layer | Classes | Notes |
|---|---|---|
| L4 transport | `L4SocketTcpListener`, `L4SocketTcpCommRegular`, `L4SocketTcpCommSSL`, `L4SocketUdp`, `L4ThreadNetDiscovery`, `L4DeviceUsbAoap`, `L4CommunicationLinkUsb`, `L4CommunicationLinkAudio`, `L4QosFlow` | TCP plain/TLS, UDP discovery, Android USB accessory |
| L5 session | `L5CommunicationServiceCore`, `ServiceUnidentified` → `IdentifiedCommon` → `L5CommServiceDesktop` / `L5CommServiceSpeaker`, `L5Settings`, `L5ThreadReceiver/Transmitter/Timer` | |
| L6 frames | `L6FrameDirtyRegions`, `L6FrameTarget`, `L5L6BufferDisplay` | |
| Encoders | `SETaskEncodeJpegCpu`, `SETaskEncodeMftH264`, `SETaskEncodeYuvCpu`, `SETaskEncodeYuvGpu`, `SETaskEncodeOff` | H.264 via Media Foundation (`ForceH264SwEncoding` registry switch) |
| Input | `SEDeviceInputKeyboard/Mouse/Pen/Touch`, `IspsdkDeviceHidKeyboard/Pen/Touch` | |
| Audio | `SEDeviceAudioRender`, `SEThreadFrameProcessorAudioRender`, `L5BufferAudio` | separate Speaker session (client type 3) |

Server L5 tunables (registry, names from `L5Settings`): `FlowControlTransmitWindowSize`,
`FlowControlTimeoutMs`, `PingEchoRequestIntervalMs`, `PingEchoReplyIntervalMs`, `InactivityDisconnectTimeoutMs`.

## Open questions

- [ ] DISPLAY_SETTINGS (type 4) payload when non-zero; Visibility `@12`.
- [ ] EVT_DISPLAY_SETTINGS states 1/2 semantics.
- [ ] Touch multi-contact layout (`@32`, `@36…`) and Pen layout.
- [ ] Rotation values; resolution change mid-session.
- [ ] TLS mode and password (`EncryptionPassword` in registry).
