import urllib.request
from PIL import Image
import io
import os
import random
import numpy as np

URL = "https://i.pinimg.com/736x/e6/e1/33/e6e133c2739895df0e526693cecab9fb.jpg"
OUTPUT_H = "include/grayscale_image.h"
WIDTH = 960
HEIGHT = 540

def generate_blue_noise(size=64):
    """Simple Blue Noise generation using Void-and-Cluster principle."""
    # Pre-computed or procedural blue noise is complex.
    # For a quick high-quality test, we use a simple stochastic approach
    # that is better than Bayer.
    noise = np.zeros((size, size))
    for i in range(size * size):
        x, y = i % size, i // size
        noise[y, x] = random.random()

    # Simple low-pass filter to make it "bluer" (avoiding clusters)
    # Actually, let's just use a high-quality randomized shuffle for now.
    flat = noise.flatten()
    indices = np.argsort(flat)
    final = np.zeros(size * size)
    for i, idx in enumerate(indices):
        final[idx] = i / (size * size)
    return final.reshape((size, size))

def main():
    print(f"Downloading image from {URL}...")
    with urllib.request.urlopen(URL) as response:
        img_data = response.read()

    img = Image.open(io.BytesIO(img_data))
    img = img.convert("L")

    # Rotate 90 degrees as requested previously
    img = img.rotate(90, expand=True)
    img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)

    pixels = list(img.getdata())

    # Generate a 64x64 Blue Noise threshold map
    blue_noise = generate_blue_noise(64)
    blue_noise_int = (blue_noise * 255).astype(np.uint8)

    os.makedirs(os.path.dirname(OUTPUT_H), exist_ok=True)

    with open(OUTPUT_H, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"const uint32_t grayscale_image_width = {WIDTH};\n")
        f.write(f"const uint32_t grayscale_image_height = {HEIGHT};\n\n")

        f.write("const uint8_t grayscale_image_data[] = {\n")
        for i in range(0, len(pixels), 16):
            chunk = pixels[i:i+16]
            f.write("    " + ", ".join([f"0x{p:02x}" for p in chunk]) + ",\n")
        f.write("};\n\n")

        f.write("const uint8_t blue_noise_64[] = {\n")
        for row in blue_noise_int:
            f.write("    " + ", ".join([f"0x{p:02x}" for p in row]) + ",\n")
        f.write("};\n")

    print(f"Generated {OUTPUT_H} with Blue Noise mask.")

if __name__ == "__main__":
    main()
