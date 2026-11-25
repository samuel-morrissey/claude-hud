#!/usr/bin/env python3
"""Converte um PPM binário (P6) da entrada padrão em PNG, sem dependências."""

import struct
import sys
import zlib


def chunk(kind: bytes, data: bytes) -> bytes:
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))


def main() -> None:
    raw = sys.stdin.buffer.read()
    magic, width, height, _maxval, pixels = raw.split(maxsplit=4)
    if magic != b"P6":
        sys.exit("entrada não é PPM P6")
    width, height = int(width), int(height)

    stride = width * 3
    rows = b"".join(b"\x00" + pixels[y * stride : (y + 1) * stride] for y in range(height))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9))
    png += chunk(b"IEND", b"")

    with open(sys.argv[1], "wb") as out:
        out.write(png)


if __name__ == "__main__":
    main()
