# Reverse-engineering log

How each part of [`protocol.md`](protocol.md) was obtained, so findings can be re-checked against
newer spacedesk releases. Addresses refer to `spacedeskService.exe` SHA-256
`c9fa79559dd985804e88bcd9049d5a3be4e57ee3b2738f998dc05bcf0098ccff` (image base `0x140000000`,
Ghidra 12.1.4 default auto-analysis, 10 153 functions, 11 389 strings).

## 2026-10-14

### 1. Recon of the installed server

- `spacedeskService.exe`: native x64 C++ (no CLR header), static OpenSSL 3.1.1 and libjpeg-turbo 3.1.0,
  imports `MFPlat.DLL` (Media Foundation) and `WS2_32.dll`.
- Listens on TCP 28252 and UDP 28252 (one UDP socket per interface).
- Drivers: IddCx display (`spacedeskDisplayUmode1_*.dll`), UMDF HID (touch/pen), keyboard/mouse
  (`spacedeskKtmInput.sys`), audio, video capture, bus, Android USB.
- RTTI class names (`.?AV…@@`) give the layer architecture (L4 transport, L5 session, L6 frames,
  SE encoders) — table in `protocol.md`.

### 2. HTML5 viewer (`spacedesk.min.js` v0.9.10)

TypeScript output, not mangled. Read directly:

| Lines | Content |
|---|---|
| 3155–3180 | `Options` defaults (port 28252, quality 40, compression 3) |
| 3284–3375 | `ProtocolDefinition` (v4.8, header 128, payload 334), all enums |
| 3388–3610 | header classes and offsets (FrameBuffer, Visibility, Cursor*) |
| 3676–3810 | `IdentificationHeader.GetBytes`, `IdentificationPayload` |
| 4170–4185 | `FlowControlAckPacket` |
| 5337–5384 | fragment counting (ack after 4 fragments) |
| 5560–5760 | KVM: keyboard, mouse, touch, disconnect packets |
| 6000–6017 | mouse sends relative deltas scaled to remote resolution |

### 3. Live probe

`tools/probe/sd_probe.py` reproduced the HTML5 session over WebSocket and over raw TCP.
H.264 requested through the HTML5 identification (`@24 = 1`) made the server close the socket.

### 4. Identification validation — `0x1400e61f0` (`L5ThreadReceiver::Run`)

Found from the string `L5Protocol::PacketIdentificationValidate failed! Invalid protocol version…`
(`0x14043d830`, single xref `0x1400e6b72`). The validator is inlined:

- `@4 == 0x14e`, `@8 == 4`, `@12 - 7 < 2`, `@16 < 4`, `@20 < 5`
- device id `wcsnlen(payload, 0x4e) != 0`
- `(byte@38 & 1) == 0 && @24 != 3 && @24 != 0` → "Invalid compression type for older clients"
- client type 3 → Speaker session, else Display
- after identification, header types allowed by mask `0x3ec2`

### 5. Session creation — `0x1400e6e20` (`L5ThreadReceiver::InstanceCreate_`)

If `byte@38 & 1`, the server submits a Settings buffer to the client right after creating the
Desktop service ("TransmitBufferSubmit Settings"). Observed live as a type-4 packet.

### 6. Desktop settings — `0x1400e5250` (`L5SessionFactory::InstanceDesktopCreate`)

```
fps      = i16 @0x2c   (0 -> 30, 1 -> 60)
quality  = u32 @0x20
if (byte @0x26 & 1) == 0:  color = @0x1c, encoding = @0x18
else:                      color = 2,     encoding = i16 @0x24
```

Probing `@0x24` ∈ {1, 3, 4} gave YUV420 raw, JPEG and **H.264 Annex-B**. Registry entries of real
viewers (`EncodingType = 4`, `OsType = 2`, `FramesPerSec = 60/90`) matched.

### 7. Packet dispatch — `0x1400e7180` (`L5ThreadReceiver::OnPacketReceived_`)

Field layouts for types 1, 6, 7, 9, 10, 11, 12, 13 (see `protocol.md`). Header type names come from
the `PROTOCOL_SPCDSK_SMPL_HEADER_TYPE_*` log strings at `0x14043abb0…0x14043b2f0`.

### 8. Pointer behaviour (live)

- Mouse type 10 with `MOUSEEVENTF_ABSOLUTE`: ignored, treated as relative with Windows pointer
  acceleration (100,100 moved the cursor by ~155,156).
- Touch type 12 with flag `0x1` and no down/up: hover that positions the cursor absolutely,
  scaled from the client size onto the target monitor (640,360 of 1280x720 → centre).
- The viewer therefore moves the pointer with touch hover and sends buttons/wheel as mouse packets.

## Useful entry points for future work

| Topic | Start from |
|---|---|
| UDP discovery | string `Broadcast Message Received ipAddress:%d.%d.%d.%d reply port:%d` (`0x14042f430`), `L4ThreadNetDiscovery::Run` |
| Audio | `L5CommServiceSpeaker`, `L5TransmitBufferFactory::CreateAudioUpdate` (`0x14043de90`) |
| Settings packet layout | `L5TransmitBufferFactory::CreateSettings` (`0x14043dfb0`) |
| TLS | `L4SocketTcpCommSSL`, `SISettings::Layer4EncryptionSslIsEnabled` (`0x1403f54c0`) |
| Runtime encoder change | `SEComSvcListIteratorDisplayUpdateEncoder::Execute` (`0x1400c15b0`) |
