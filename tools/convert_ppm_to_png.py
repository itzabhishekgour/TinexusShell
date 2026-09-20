#!/usr/bin/env python3
import sys
import os

def convert(ppm_path, png_path):
    if not os.path.exists(ppm_path):
        print(f"File not found: {ppm_path}")
        return False

    try:
        from PIL import Image
        img = Image.open(ppm_path)
        img.save(png_path, "PNG")
        print(f"Successfully converted {ppm_path} to {png_path} using PIL")
        return True
    except ImportError:
        pass

    # Pure standard library PPM to PNG converter using zlib
    import zlib
    import struct

    with open(ppm_path, 'rb') as f:
        magic = f.readline().strip()
        if magic != b'P6':
            print(f"Unsupported PPM format: {magic}")
            return False

        line = f.readline()
        while line.startswith(b'#'):
            line = f.readline()

        dims = line.strip().split()
        while len(dims) < 2:
            dims.extend(f.readline().strip().split())
        w, h = int(dims[0]), int(dims[1])

        maxval = int(f.readline().strip())
        raw_data = f.read()

    # Raw scanlines with filter byte 0 (None)
    row_bytes = w * 3
    raw_lines = bytearray()
    for y in range(h):
        raw_lines.append(0) # filter type 0
        raw_lines.extend(raw_data[y * row_bytes : (y + 1) * row_bytes])

    compressed = zlib.compress(bytes(raw_lines), 9)

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        crc = zlib.crc32(tag + data) & 0xffffffff
        return c + struct.pack('>I', crc)

    with open(png_path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n') # PNG header
        # IHDR: width(4), height(4), bit_depth(1), color_type(1, 2=RGB), comp(1), filter(1), interlace(1)
        ihdr_data = struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)
        f.write(chunk(b'IHDR', ihdr_data))
        f.write(chunk(b'IDAT', compressed))
        f.write(chunk(b'IEND', b''))

    print(f"Successfully converted {ppm_path} to {png_path} using standard library PNG encoder")
    return True

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: convert_ppm_to_png.py <input.ppm> <output.png>")
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
