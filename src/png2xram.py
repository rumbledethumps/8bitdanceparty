#!/usr/bin/env python3
"""Convert an indexed PNG into RP6502 bitmap or sprite data.

The data is the image rows, each starting on a byte boundary, with the
leftmost pixel of each byte in the highest bits. This is the layout of
mode 3 bitmaps and mode 5 sprites without the reverse-bits option.

The palette is 1 << bpp RGB555 words, little-endian. A color is opaque
unless the tRNS alpha for that index is below 128. Indices past the end
of the PNG palette are transparent black.

usage: png2xram.py <in.png> --bpp {1,2,4,8} [--data <out.bin>]
                   [--palette <out.bin>] [--rows <first> <count>]
"""

import argparse
import struct
import sys
import zlib


def read_png(path):
    """Return rows of palette indices, the palette as (r, g, b) tuples, and
    the tRNS alpha values."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        sys.exit(f"{path}: not a PNG file")

    pos = 8
    idat = []
    plte = trns = b""
    while pos < len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(
                ">IIBBBBB", body)
        elif kind == b"PLTE":
            plte = body
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat.append(body)

    if color_type != 3 or interlace:
        sys.exit(f"{path}: only indexed, non-interlaced PNG files are supported")

    stride = (width * depth + 7) // 8
    lines = unfilter(zlib.decompress(b"".join(idat)), stride, height)
    mask = (1 << depth) - 1
    rows = []
    for line in lines:
        row = []
        for x in range(width):
            bit = x * depth
            row.append(line[bit // 8] >> (8 - depth - bit % 8) & mask)
        rows.append(row)
    palette = [tuple(plte[i:i + 3]) for i in range(0, len(plte), 3)]
    return rows, palette, trns


def unfilter(raw, stride, height):
    """Undo the filter that starts each scanline. Indexed rows are filtered
    a byte at a time, so the left neighbor of a byte is the byte before it."""
    lines = []
    above = bytearray(stride)
    pos = 0
    for _ in range(height):
        kind = raw[pos]
        line = bytearray(raw[pos + 1:pos + 1 + stride])
        pos += 1 + stride
        for i in range(stride):
            left = line[i - 1] if i else 0
            up = above[i]
            up_left = above[i - 1] if i else 0
            if kind == 0:
                predict = 0
            elif kind == 1:
                predict = left
            elif kind == 2:
                predict = up
            elif kind == 3:
                predict = (left + up) // 2
            else:
                p = left + up - up_left
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
                if pa <= pb and pa <= pc:
                    predict = left
                elif pb <= pc:
                    predict = up
                else:
                    predict = up_left
            line[i] = (line[i] + predict) & 0xFF
        lines.append(line)
        above = line
    return lines


def pack(rows, bpp):
    out = bytearray()
    for row in rows:
        acc = bits = 0
        for index in row:
            acc = acc << bpp | index
            bits += bpp
            if bits == 8:
                out.append(acc)
                acc = bits = 0
        if bits:
            out.append(acc << (8 - bits))
    return out


def rgb555(palette, trns, bpp):
    out = bytearray()
    for i in range(1 << bpp):
        color = 0
        if i < len(palette):
            r, g, b = palette[i]
            color = (b >> 3) << 11 | (g >> 3) << 6 | r >> 3
            if i >= len(trns) or trns[i] >= 128:
                color |= 1 << 5
        out += struct.pack("<H", color)
    return out


def main():
    parser = argparse.ArgumentParser(
        description="Convert an indexed PNG into RP6502 bitmap or sprite data.")
    parser.add_argument("png")
    parser.add_argument("--bpp", type=int, choices=(1, 2, 4, 8), required=True)
    parser.add_argument("--data", help="pixel data output file")
    parser.add_argument("--palette", help="RGB555 palette output file")
    parser.add_argument("--rows", type=int, nargs=2, metavar=("FIRST", "COUNT"),
                        help="convert only these rows")
    args = parser.parse_args()
    if not args.data and not args.palette:
        parser.error("nothing to write: give --data, --palette or both")

    rows, palette, trns = read_png(args.png)
    if args.rows:
        first, count = args.rows
        if first < 0 or count < 1 or first + count > len(rows):
            sys.exit(f"{args.png}: rows {first}..{first + count - 1} "
                     f"are outside rows 0..{len(rows) - 1}")
        rows = rows[first:first + count]
    if args.data:
        highest = max(max(row) for row in rows)
        if highest >> args.bpp:
            sys.exit(f"{args.png}: index {highest} does not fit in {args.bpp} bits")
        with open(args.data, "wb") as f:
            f.write(pack(rows, args.bpp))
    if args.palette:
        with open(args.palette, "wb") as f:
            f.write(rgb555(palette, trns, args.bpp))


if __name__ == "__main__":
    main()
