#pragma once

#include <stdint.h>

// 4x4 Bayer matrix for ordered dithering
inline const uint8_t bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

// Apply Bayer dithering and quantize to 4 levels (0-3)
// Stateless - works correctly with any pixel processing order
inline uint8_t applyBayerDither4Level(uint8_t gray, int x, int y) {
  int bayer = bayer4x4[y & 3][x & 3];
  int dither = (bayer - 8) * 5;  // Scale to +/-40 (half of quantization step 85)

  int adjusted = gray + dither;
  if (adjusted < 0) adjusted = 0;
  if (adjusted > 255) adjusted = 255;

  if (adjusted < 64) return 0;
  if (adjusted < 128) return 1;
  if (adjusted < 192) return 2;
  return 3;
}

// Apply Bayer dithering and quantize to 16 levels (0..255 output in steps of 17)
inline uint8_t applyBayerDither16Level(uint8_t gray, int x, int y) {
  int bayer = bayer4x4[y & 3][x & 3];
  // 16 levels: step is 17. dither should be +/- 8.
  int dither = (bayer - 8);
  int adjusted = gray + dither;
  if (adjusted < 0) adjusted = 0;
  if (adjusted > 255) adjusted = 255;

  // Return mapped to 8-bit for renderer: (adjusted/17)*17
  return (uint8_t)((adjusted >> 4) << 4);
}
