# spacedesk wire protocol (reverse-engineered notes)

Status: **display path complete enough for a viewer** (JPEG, raw, H.264, input). Audio, discovery
and TLS still open. Each fact carries its source:

- `[js]`   official HTML5 viewer `spacedesk.min.js` v0.9.10 (viewer.spacedesk.net)
- `[live]` observed with `tools/probe/sd_probe.py` against spacedesk server 2.2.x on Windows 11
- `[bin]`  static analysis of `spacedeskService.exe` (Ghidra 12.1.4 via REA; addresses refer to
           SHA-256 `c9fa7955…8ccff`)
- `[reg]`  server registry state `HKLM\SOFTWARE\datronicsoft\spacedesk`
- `[?]`    hypothesis, not verified

All integers are **little-endian**. Protocol version **4.8** (server accepts minor 7 or 8) `[bin]`.

## Transport

| Item | Value | Source |
|---|---|---|
| TCP port | 28252 | `[js]` `[live]` |
| Raw TCP | 128-byte header + `header[4]` payload bytes, back to back | `[live]` |
| WebSocket | `ws://<server>:28252/`, one packet per binary message; server detects HTTP `GET` upgrade on the same port | `[js]` `[live]` `[bin]` |
| UDP 28252 | discovery: server answers broadcasts ("Broadcast Message Received ipAddress … reply port") | `[bin]` format unknown |
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
| ? | AUDIO_SPEAKER | — | S→C (speaker session) |

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
| 36 | u16 | **encoding** | used when `@38` bit0 = 1 `[bin]` `[live]` |
| 38 | u8 | **flags**; bit0 = extended identification | also makes server send DISPLAY_SETTINGS `[bin]` |
| 44 | i16 | fps: 0 → 30, 1 → 60, other = literal | `[bin]` `[reg]` (real clients 60, 90) |
| 46 | u16 | 0 | `[js]` |
| 48 | u32 | resolution mode: 1 = client, 2 = client + custom | `[js]`; stored by server `[bin]` |
| 52 | u32 | width | `[js]` |
| 56 | u32 | custom width | `[js]` |
| 88 | u32 | height | `[js]` |
| 92 | u32 | custom height | `[js]` |

### Encoding values (server) `[live]`

| Value | Output | FrameBuffer payload |
|---|---|---|
| 0 | raw BGRX, 4 horizontal fragments | `w*h/4*4` bytes per fragment |
| 1 | raw YUV 4:2:0 (12 bpp), full frame | `w*h*3/2` bytes |
| 3 | JPEG, 4 horizontal fragments | baseline JFIF |
| 4 | **H.264 Annex-B**, full frame, not fragmented | starts `00 00 00 01 09` (AUD); SPS `27 64 00 20` = High profile, level 3.2 at 1280x720 |

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
| 10 Mouse | `@8 x`, `@12 y`, `@16 buttonData` (wheel delta), `@20 buttonFlags` (Win32 `MOUSEEVENTF_*`), `@24 flags` |
| 11 Keyboard | `@8 vkey`, `@12 scancode`, `@16 flags` (Win32 `KEYEVENTF_*`). `flags & 4` (UNICODE): `@12` is the UTF-16 char. `flags & 0x10000`: "hardware" key path using `@8` |
| 12 Touch | `@8 x`, `@12 y`, `@16 client w`, `@20 client h`, `@24 flags` (0x1 absolute, 0x2 down, 0x4 up), `@28 timestamp ms`, `@32` contact index (server passes `@32 + 1`), `@36…` extra contact data `[?]` |
| 13 Pen | `@8`, `@12`, `@16`, `@20`, `@24`, `@28`, `@32`, bytes `@36..@39` — layout `[?]` |

MOUSEEVENTF: Move 0x1, LeftDown 0x2, LeftUp 0x4, RightDown 0x8, RightUp 0x10, MiddleDown 0x20,
MiddleUp 0x40, XDown 0x80, XUp 0x100, Wheel 0x800, HWheel 0x1000, VirtualDesk 0x4000, Absolute 0x8000 `[js]`.
KEYEVENTF: ExtendedKey 0x1, KeyUp 0x2, Unicode 0x4, ScanCode 0x8 `[js]`.

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
- [ ] Speaker session: AUDIO_SPEAKER type value and audio format.
- [ ] UDP discovery request/reply format.
- [ ] Rotation values; resolution change mid-session.
- [ ] TLS mode and password (`EncryptionPassword` in registry).
