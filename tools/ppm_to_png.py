#!/usr/bin/env python3
import sys
from pathlib import Path

def convert_ppm_to_png(ppm_file, png_file):
    ppm_path = Path(ppm_file)
    png_path = Path(png_file)
    
    if not ppm_path.exists():
        print(f"Error: {ppm_path} does not exist", file=sys.stderr)
        return False
        
    try:
        from PIL import Image
        with Image.open(ppm_path) as img:
            img.save(png_path, "PNG")
        print(f"[+] Converted {ppm_path} -> {png_path}")
        return True
    except ImportError:
        pass
        
    # Fallback to netpbm or imagemagick if PIL not installed
    import subprocess
    try:
        subprocess.run(["pnmtopng", str(ppm_path)], stdout=open(png_path, "wb"), check=True)
        print(f"[+] Converted via pnmtopng {ppm_path} -> {png_path}")
        return True
    except Exception:
        pass

    # Fallback to pure Python (zlib + struct, standard library only, 0 dependencies)
    try:
        import zlib
        import struct

        with open(ppm_path, "rb") as f:
            header = f.readline().strip()
            if header != b"P6":
                raise ValueError("Only binary P6 PPM supported")
            line = f.readline().strip()
            while line.startswith(b"#"):
                line = f.readline().strip()
            parts = line.split()
            while len(parts) < 2:
                parts.extend(f.readline().strip().split())
            width, height = int(parts[0]), int(parts[1])
            maxval_line = f.readline().strip()
            while maxval_line.startswith(b"#"):
                maxval_line = f.readline().strip()
            maxval = int(maxval_line)
            raw_data = f.read()

        row_len = width * 3
        raw_rows = bytearray()
        for y in range(height):
            raw_rows.append(0) # filter byte: None
            raw_rows.extend(raw_data[y * row_len : (y + 1) * row_len])

        compressed = zlib.compress(bytes(raw_rows), 6)

        def make_chunk(chunk_type, data):
            chunk = chunk_type + data
            crc = struct.pack(">I", zlib.crc32(chunk) & 0xffffffff)
            return struct.pack(">I", len(data)) + chunk + crc

        png_bytes = bytearray(b"\x89PNG\r\n\x1a\n")
        ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
        png_bytes.extend(make_chunk(b"IHDR", ihdr_data))
        png_bytes.extend(make_chunk(b"IDAT", compressed))
        png_bytes.extend(make_chunk(b"IEND", b""))

        with open(png_path, "wb") as f:
            f.write(png_bytes)
        print(f"[+] Converted via pure Python zlib {ppm_path} -> {png_path}")
        return True
    except Exception as e:
        print(f"Error converting PPM via pure Python: {e}", file=sys.stderr)
        return False

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} input.ppm output.png")
        sys.exit(1)
    success = convert_ppm_to_png(sys.argv[1], sys.argv[2])
    sys.exit(0 if success else 1)
