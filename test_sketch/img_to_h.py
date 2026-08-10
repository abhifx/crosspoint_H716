from PIL import Image
import sys

def convert_image(image_path, output_path):
    # 1. Open
    img = Image.open(image_path).convert('L')

    # 2. Resizing: Fit to the 540-pixel height of the H716 landscape panel
    # We do NOT rotate here; the 960x540 panel will be held vertically.
    img.thumbnail((960, 540), Image.Resampling.LANCZOS)

    # Create a 960x540 canvas (White = 255)
    new_img = Image.new('L', (960, 540), 255)
    # Center the portrait girl in the middle of the landscape canvas
    offset = ((960 - img.width) // 2, (540 - img.height) // 2)
    new_img.paste(img, offset)

    # 3. Successful Test Mapping
    # In your first successful test: 15 - (p // 16) gave a positive.
    # Level 0 = White, Level 15 = Black.
    pixels = list(new_img.getdata())
    data = [max(0, min(15, 15 - (p // 16))) for p in pixels]

    with open(output_path, 'w') as f:
        f.write('#include "benchmark_image.h"\n\n')
        f.write('const uint8_t benchmark_image_data[518400] = {\n')
        for i, val in enumerate(data):
            f.write(str(val))
            if i < len(data) - 1:
                f.write(', ')
            if (i + 1) % 32 == 0:
                f.write('\n')
        f.write('\n};')

if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(1)
    convert_image(sys.argv[1], 'benchmark_image.cpp')
    print("Generated benchmark_image.cpp (Successful Logic Replicated)")
