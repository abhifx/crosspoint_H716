#pragma once

#include <stdint.h>

// =============================================================================
// ImageRenderConfig is pulled in so the Bayer ditherer uses the same runtime
// thresholds as quantizeSimple(). This keeps in-book images consistent with
// cover/screensaver rendering when the user adjusts the thresholds.
// =============================================================================
#include <ImageRenderConfig.h>

// 4x4 Bayer matrix for ordered dithering
inline const uint8_t bayer4x4[4][4] = {
    {0, 8, 2, 10},
    {12, 4, 14, 6},
    {3, 11, 1, 9},
    {15, 7, 13, 5},
};

// Apply Bayer dithering and quantize to 4 levels (0-3).
// Stateless - works correctly with any pixel processing order.
//
// Thresholds are read from the runtime ImageRenderConfig globals so that
// the settings UI controls all image dithering uniformly.
inline uint8_t applyBayerDither4Level(uint8_t gray, int x, int y) {
  int bayer = bayer4x4[y & 3][x & 3];

  // Scale dither amplitude proportionally to the threshold gaps.
  // Use the gap between black and dark as a reference for the dither spread.
  const int gap = static_cast<int>(g_imageRenderThresholdDark) -
                  static_cast<int>(g_imageRenderThresholdBlack);
  const int dither = (bayer - 8) * gap / 17;  // ~half of the gap

  int adjusted = gray + dither;
  if (adjusted < 0) adjusted = 0;
  if (adjusted > 255) adjusted = 255;

  if (adjusted < g_imageRenderThresholdBlack) return 0;
  if (adjusted < g_imageRenderThresholdDark)  return 1;
  if (adjusted < g_imageRenderThresholdLight) return 2;
  return 3;
}
