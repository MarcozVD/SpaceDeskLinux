#!/usr/bin/env python3
"""Minimal spacedesk protocol probe (clean-room, stdlib only).

Connects to a spacedesk server, sends an Identification packet, logs every
server packet header, saves frames (JPEG files or an Annex-B .h264 stream),
acks frames (flow control) and disconnects after N frames or a timeout.

Transports:
  tcp  - raw TCP on 28252 (native viewers)
  ws   - WebSocket on TCP/28252 (what the HTML5 viewer uses)
"""
import argparse
import base64
import os
import socket
import struct
import sys
import time

HEADER_LEN = 128
PAYLOAD_LEN = 334
VERSION_MAJOR = 4
VERSION_MINOR = 8

T_IDENTIFICATION, T_PING, T_FRAMEBUFFER, T_VISIBILITY = 0, 1, 2, 3
T_DISPLAY_SETTINGS, T_UNUSED_01, T_EVT_DISPLAY_SETTINGS, T_FLOWACK = 4, 5, 6, 7
T_ERROR, T_ROTATION, T_MOUSE, T_KEYBOARD, T_TOUCH, T_PEN = 8, 9, 10, 11, 12, 13
TYPE_NAMES = {
    0: "Identification", 1: "Ping", 2: "FrameBuffer", 3: "Visibility",
    4: "DisplaySettings", 5: "Unused01", 6: "EvtDisplaySettings",
    7: "FlowControlAck", 8: "Error", 9: "Rotation", 10: "Mouse",
    11: "Keyboard", 12: "Touch", 13: "Pen",
}
# 0..2 open a Display session, 3 opens a Speaker (audio) session
CLIENT_TYPES = {"remote-monitor": 0, "browser": 1, "usb-apple": 2, "speaker": 3}
# Encoding type as understood by the server (see docs/protocol.md)
ENCODING = {"bgrx": 0, "yuv420": 1, "jpeg": 3, "h264": 4}
IDENT_FLAG_EXTENDED = 0x01  # header byte 38: encoding is read from u16 @36


def u32(buf, off):
    return struct.unpack_from("<I", buf, off)[0]


def build_identification(client_type, encoding, quality, fps, width, height, name,
                         device_id="1111122222", os_type=0, extended=True):
    h = bytearray(HEADER_LEN)
    struct.pack_into("<IIIIII", h, 0, T_IDENTIFICATION, PAYLOAD_LEN,
                     VERSION_MAJOR, VERSION_MINOR, client_type, os_type)
    if extended:
        struct.pack_into("<I", h, 24, ENCODING["jpeg"])  # ignored by server in this mode
        struct.pack_into("<HB", h, 36, encoding, IDENT_FLAG_EXTENDED)
    else:
        struct.pack_into("<I", h, 24, encoding)  # legacy: only 0 or 3 accepted
    struct.pack_into("<I", h, 32, max(quality, 1))
    struct.pack_into("<HH", h, 44, fps, 0)  # fps: 0 -> 30, 1 -> 60, else literal
    struct.pack_into("<I", h, 48, 1)  # resolution mode: 1 = client, 2 = custom
    struct.pack_into("<I", h, 52, width)
    struct.pack_into("<I", h, 88, height)
    p = bytearray(PAYLOAD_LEN)
    dev = device_id.encode("utf-16-le")[:76]
    p[0:len(dev)] = dev
    enc = name.encode("utf-16-le")[: PAYLOAD_LEN - 78]
    p[78:78 + len(enc)] = enc
    return bytes(h + p)


def build_flow_ack():
    h = bytearray(HEADER_LEN)
    struct.pack_into("<II", h, 0, T_FLOWACK, 0)
    return bytes(h)


def build_disconnect():
    return struct.pack("<I", T_ERROR) + bytes(24)  # headerKVMLength = 28


class WsTransport:
    def __init__(self, host, port, timeout):
        self.s = socket.create_connection((host, port), timeout=timeout)
        key = base64.b64encode(os.urandom(16)).decode()
        req = (f"GET / HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
               f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
               f"Sec-WebSocket-Version: 13\r\nOrigin: http://viewer.spacedesk.net\r\n\r\n")
        self.s.sendall(req.encode())
        resp = b""
        while b"\r\n\r\n" not in resp:
            chunk = self.s.recv(4096)
            if not chunk:
                raise ConnectionError("closed during handshake: %r" % resp)
            resp += chunk
        head, self.buf = resp.split(b"\r\n\r\n", 1)
        print("[ws] handshake:", head.decode(errors="replace").splitlines()[0])
        if b" 101 " not in head.split(b"\r\n")[0]:
            raise ConnectionError(head.decode(errors="replace"))

    def _read(self, n):
        while len(self.buf) < n:
            chunk = self.s.recv(65536)
            if not chunk:
                raise ConnectionError("socket closed")
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def send(self, data, opcode=0x2):
        mask = os.urandom(4)
        n = len(data)
        hdr = bytes([0x80 | opcode])
        if n < 126:
            hdr += bytes([0x80 | n])
        elif n < 65536:
            hdr += bytes([0x80 | 126]) + struct.pack(">H", n)
        else:
            hdr += bytes([0x80 | 127]) + struct.pack(">Q", n)
        masked = bytes(b ^ mask[i & 3] for i, b in enumerate(data))
        self.s.sendall(hdr + mask + masked)

    def recv_message(self):
        """Return one complete binary/text message (reassembles fragments)."""
        msg = b""
        while True:
            b0, b1 = self._read(2)
            fin, opcode = b0 & 0x80, b0 & 0x0F
            n = b1 & 0x7F
            if n == 126:
                n = struct.unpack(">H", self._read(2))[0]
            elif n == 127:
                n = struct.unpack(">Q", self._read(8))[0]
            mask = self._read(4) if b1 & 0x80 else None
            data = self._read(n)
            if mask:
                data = bytes(b ^ mask[i & 3] for i, b in enumerate(data))
            if opcode == 0x8:
                code = struct.unpack(">H", data[:2])[0] if len(data) >= 2 else None
                raise ConnectionError(f"ws close code={code} reason={data[2:]!r}")
            if opcode == 0x9:
                self.send(data, opcode=0xA)
                continue
            if opcode == 0xA:
                continue
            msg += data
            if fin:
                return msg

    def close(self):
        try:
            self.send(struct.pack(">H", 1000), opcode=0x8)
        except OSError:
            pass
        finally:
            self.s.close()


class TcpTransport:
    """Raw TCP. Framing hypothesis: 128-byte header, then header[4] payload bytes."""

    def __init__(self, host, port, timeout):
        self.s = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""

    def _read(self, n):
        while len(self.buf) < n:
            chunk = self.s.recv(65536)
            if not chunk:
                raise ConnectionError("socket closed (buffered %d bytes: %r)" % (len(self.buf), self.buf[:64]))
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def send(self, data):
        self.s.sendall(data)

    def recv_message(self):
        hdr = self._read(HEADER_LEN)
        return hdr + self._read(u32(hdr, 4))

    def close(self):
        self.s.close()


def describe(msg):
    t = u32(msg, 0)
    name = TYPE_NAMES.get(t, f"?{t}")
    follow = u32(msg, 4)
    extra = ""
    if t == T_FRAMEBUFFER:
        w, h, pitch, fmt, x1, y1, x2, y2, comp = struct.unpack_from("<9I", msg, 8)
        extra = (f" res={w}x{h} pitch={pitch} fmt={fmt} rect=({x1},{y1})-({x2},{y2})"
                 f" comp={comp} frag={u32(msg, 64)}")
    elif t == T_VISIBILITY:
        extra = f" visible={u32(msg, 8)} @12={u32(msg, 12)}"
    elif t == T_PING:
        extra = f" option={u32(msg, 12)}"
    elif t in (T_DISPLAY_SETTINGS, T_ERROR):
        nz = [(o, u32(msg, o)) for o in range(8, HEADER_LEN, 4) if u32(msg, o)]
        extra = f" nonzero={nz}"
    return f"{name:<18} len={len(msg):>8} follow={follow:>8}{extra}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=28252)
    ap.add_argument("--transport", choices=["ws", "tcp"], default="tcp")
    ap.add_argument("--client", choices=CLIENT_TYPES, default="remote-monitor")
    ap.add_argument("--encoding", choices=ENCODING, default="h264")
    ap.add_argument("--legacy", action="store_true",
                    help="HTML5-style identification (encoding @24, only bgrx/jpeg)")
    ap.add_argument("--device-id", default="{5D3E8A4C-0B1F-4C2E-9A57-53504C444B31}",
                    help="UTF-16 device id; server persists settings per id")
    ap.add_argument("--os-type", type=int, default=0)
    ap.add_argument("--quality", type=int, default=70)
    ap.add_argument("--fps", type=int, default=60)
    ap.add_argument("--size", default="1280x720")
    ap.add_argument("--name", default="sd_probe on Debian")
    ap.add_argument("--frames", type=int, default=10, help="stop after N FrameBuffer packets")
    ap.add_argument("--seconds", type=float, default=15.0)
    ap.add_argument("--out", default="captures")
    ap.add_argument("--raw", action="store_true", help="also dump every raw packet to --out")
    a = ap.parse_args()

    width, height = map(int, a.size.lower().split("x"))
    os.makedirs(a.out, exist_ok=True)
    T = WsTransport if a.transport == "ws" else TcpTransport
    t = T(a.host, a.port, timeout=5)
    ident = build_identification(CLIENT_TYPES[a.client], ENCODING[a.encoding],
                                 a.quality, a.fps, width, height, a.name,
                                 device_id=a.device_id, os_type=a.os_type,
                                 extended=not a.legacy)
    t.send(ident)
    print(f"[>] Identification {len(ident)} bytes ({a.client}, {a.encoding}, {width}x{height})")

    frames = 0
    fragments = 0
    n = 0
    h264 = open(os.path.join(a.out, "stream.h264"), "wb") if a.encoding == "h264" else None
    deadline = time.time() + a.seconds
    try:
        while time.time() < deadline and frames < a.frames:
            msg = t.recv_message()
            n += 1
            print(f"[<] #{n:04d} {describe(msg)}")
            if a.raw:
                with open(os.path.join(a.out, f"pkt_{n:04d}.bin"), "wb") as f:
                    f.write(msg)
            if u32(msg, 0) == T_FRAMEBUFFER:
                body = msg[HEADER_LEN:]
                if h264:
                    h264.write(body)
                else:
                    ext = "jpg" if body[:2] == b"\xff\xd8" else "bin"
                    with open(os.path.join(a.out, f"frame_{n:04d}.{ext}"), "wb") as f:
                        f.write(body)
                # ack once per complete frame (fragmented frames: @60 = fragment count)
                if u32(msg, 64):
                    fragments += 1
                    if fragments < max(u32(msg, 60), 1):
                        continue
                    fragments = 0
                frames += 1
                t.send(build_flow_ack())
    except (ConnectionError, socket.timeout) as e:
        print("[!]", e)
    finally:
        try:
            t.send(build_disconnect())
        except OSError:
            pass
        t.close()
        if h264:
            h264.close()
    print(f"[=] {n} packets, {frames} frames -> {os.path.abspath(a.out)}")


if __name__ == "__main__":
    sys.exit(main())
