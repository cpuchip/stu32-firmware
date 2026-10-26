#!/usr/bin/env python3
"""Unit 082 (4C): tests/golden-v22-4c.img, a newer firmware's image, from tests/golden-v21-082.img by bytes alone.

Not this build's writer: the image is golden-v21-082's with
  - the version 22;
  - Y, Z and T (T_STACK's second to fourth values: an expression, the name B, a number) each replaced by a value kind
    this build doesn't know, written as state.h says a later build writes one: its kind, len and mask, a 16-bit byte
    count, that many bytes (kind 5 with 7 bytes, kind 9 with none, kind 200 with 300);
  - a section this build doesn't know (tag 40, five bytes) before the checksum.
expr_test loads it and checks the rest of the state survived: the image it saves again equals golden-v21-082's state
with Y, Z and T set to 0 and OLDER FIRMWARE showing, saved by the build.

  python3 scripts/mk_golden_v22.py tests/golden-v21-082.img tests/golden-v22-4c.img
"""
import struct
import sys
import zlib

T_STACK = 2


def unknown(kind, ln, mask, payload):
    return bytes([kind, ln, mask]) + struct.pack('<H', len(payload)) + payload


def main(src, dst):
    b = open(src, 'rb').read()
    assert b[:4] == b'ABSI' and struct.unpack_from('<HHI', b, 4) == (21, 0, len(b))
    assert zlib.crc32(b[:-4]) == struct.unpack_from('<I', b, len(b) - 4)[0]
    out = bytearray(b[:12])
    at = 12
    while at < len(b) - 4:
        tag, size = struct.unpack_from('<HI', b, at)
        body = b[at + 6:at + 6 + size]
        if tag == T_STACK:
            # X and Y expressions (kind 3, the text's length, mask 0, the text), Z a name (kind 4, the letter, 0),
            # T a number up to LASTx, which is an expression ending the section
            def expr_end(p):
                assert body[p] == 3 and body[p + 2] == 0
                return p + 3 + body[p + 1]
            x_end = expr_end(0)
            y_end = expr_end(x_end)
            assert body[y_end] == 4 and body[y_end + 2] == 0
            z_end = y_end + 3
            lastx = len(body) - 11                  # SIN(A)+B: 3 + 8
            assert body[lastx:lastx + 3] == b'\x03\x08\x00' and body[lastx + 3:] == b'SIN(A)+B'
            assert body[z_end] == 0                 # T: a real number
            body = (body[:x_end] +
                    unknown(5, 0x11, 0x22, bytes(range(1, 8))) +
                    unknown(9, 0, 0, b'') +
                    unknown(200, 3, 7, bytes((i * 7) & 0xFF for i in range(300))) +
                    body[lastx:])
        out += struct.pack('<HI', tag, len(body)) + body
        at += 6 + size
    out += struct.pack('<HI', 40, 5) + b'\x01\x02\x03\x04\x05'
    out += b'\0\0\0\0'
    struct.pack_into('<H', out, 4, 22)
    struct.pack_into('<I', out, 8, len(out))
    struct.pack_into('<I', out, len(out) - 4, zlib.crc32(bytes(out[:-4])))
    open(dst, 'wb').write(out)
    print(f'{dst}: {len(out)} B, body CRC {zlib.crc32(bytes(out[:-4])):08X}')


if __name__ == '__main__':
    main(*sys.argv[1:3])
