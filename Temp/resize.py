import sys
from PIL import Image

src = "Temp/daniel-leone-v7daTKlZzaw-unsplash.jpg"
dst = "Temp/tinexus-default.jpg"

try:
    im = Image.open(src)
    print(f"Original size: {im.size}")
    im = im.resize((1920, 1080), Image.LANCZOS)
    im.save(dst, quality=92)
    print(f"Resized successfully to 1920x1080 -> {dst}")
except Exception as e:
    print(f"Error: {e}")
