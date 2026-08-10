#pragma once

#include <GfxRenderer.h>
#include <HalDisplay.h>
#include <stdint.h>

#include <cassert>

// Direct framebuffer writer that eliminates per-pixel overhead from the image
// rendering hot path.  Pre-computes orientation transform as linear coefficients
// and caches render-mode state so the inner loop is: one multiply, one add,
// one shift, and one AND per pixel — no branches, no method calls.
struct DirectPixelWriter {
  uint8_t* fb;
  GfxRenderer::RenderMode mode;
  uint16_t displayWidthBytes;
  uint16_t phyW;
  int originY;
  int clipRows;
  bool is8BitSource = false;

  // Orientation is collapsed into a linear transform:
  //   phyX = phyXBase + x * phyXStepX + y * phyXStepY
  //   phyY = phyYBase + x * phyYStepX + y * phyYStepY
  int phyXBase, phyYBase;
  int phyXStepX, phyYStepX;  // per logical-X step
  int phyXStepY, phyYStepY;  // per logical-Y step

  // Row-precomputed: the Y-dependent portion of the physical coords
  int rowPhyXBase, rowPhyYBase;

  void init(GfxRenderer& renderer, bool is8Bit = false) {
    mode = renderer.getRenderMode();
    if (mode == GfxRenderer::GRAYSCALE_8BIT) {
      fb = renderer.getGrayBuffer();
      originY = 0;
      clipRows = renderer.getDisplayHeight();
    } else {
      fb = renderer.getWriteTarget();
      originY = renderer.getWriteOriginY();
      clipRows = renderer.getWriteRows();
    }
    displayWidthBytes = renderer.getDisplayWidthBytes();
    phyW = renderer.getDisplayWidth();
    is8BitSource = is8Bit;

    const int phyH = renderer.getDisplayHeight();

    switch (renderer.getOrientation()) {
      case GfxRenderer::Portrait:
        phyXBase = 0;
        phyYBase = phyH - 1;
        phyXStepX = 0;
        phyYStepX = -1;
        phyXStepY = 1;
        phyYStepY = 0;
        break;
      case GfxRenderer::LandscapeClockwise:
        phyXBase = phyW - 1;
        phyYBase = phyH - 1;
        phyXStepX = -1;
        phyYStepX = 0;
        phyXStepY = 0;
        phyYStepY = -1;
        break;
      case GfxRenderer::PortraitInverted:
        phyXBase = phyW - 1;
        phyYBase = 0;
        phyXStepX = 0;
        phyYStepX = 1;
        phyXStepY = -1;
        phyYStepY = 0;
        break;
      case GfxRenderer::LandscapeCounterClockwise:
        phyXBase = 0;
        phyYBase = 0;
        phyXStepX = 1;
        phyYStepX = 0;
        phyXStepY = 0;
        phyYStepY = 1;
        break;
      default:
        phyXBase = 0;
        phyYBase = 0;
        phyXStepX = 1;
        phyYStepX = 0;
        phyXStepY = 0;
        phyYStepY = 1;
        break;
    }
  }

  inline void beginRow(int logicalY) {
    rowPhyXBase = phyXBase + logicalY * phyXStepY;
    rowPhyYBase = phyYBase + logicalY * phyYStepY;
  }

  inline void bandColRange(int xBase, int width, int& colStart, int& colEnd) const {
    assert(phyYStepX == 0 || phyYStepX == 1 || phyYStepX == -1);
    colStart = 0;
    colEnd = width;
    if (phyYStepX == 0) {
      const int sy = rowPhyYBase - originY;
      if (static_cast<unsigned>(sy) >= static_cast<unsigned>(clipRows)) colEnd = 0;
      return;
    }
    const int loY = originY;
    const int hiY = originY + clipRows - 1;
    int xLo, xHi;
    if (phyYStepX > 0) {
      xLo = loY - rowPhyYBase;
      xHi = hiY - rowPhyYBase;
    } else {
      xLo = rowPhyYBase - hiY;
      xHi = rowPhyYBase - loY;
    }
    const int cs = xLo - xBase;
    const int ce = xHi - xBase + 1;
    if (cs > colStart) colStart = cs;
    if (ce < colEnd) colEnd = ce;
    if (colStart < 0) colStart = 0;
    if (colEnd > width) colEnd = width;
    if (colStart > colEnd) colStart = colEnd;
  }

  inline void writePixel(int logicalX, uint8_t pixelValue) const {
    if (mode == GfxRenderer::GRAYSCALE_8BIT) {
      const int px = rowPhyXBase + logicalX * phyXStepX;
      const int py = rowPhyYBase + logicalX * phyYStepX;
      if (static_cast<unsigned>(py) >= static_cast<unsigned>(clipRows)) return;
      uint8_t gray8 = is8BitSource ? pixelValue : (pixelValue * 85);
      fb[py * phyW + px] = gray8;
      return;
    }

    bool draw;
    bool state;
    switch (mode) {
      case GfxRenderer::BW:
        draw = (pixelValue < 3);
        state = true;
        break;
      case GfxRenderer::GRAYSCALE_MSB:
        draw = (pixelValue == 1 || pixelValue == 2);
        state = false;
        break;
      case GfxRenderer::GRAYSCALE_LSB:
        draw = (pixelValue == 1);
        state = false;
        break;
      default:
        return;
    }

    if (!draw) return;

    const int phyX = rowPhyXBase + logicalX * phyXStepX;
    const int phyY = rowPhyYBase + logicalX * phyYStepX;

    const int sy = phyY - originY;
    if (static_cast<unsigned>(sy) >= static_cast<unsigned>(clipRows)) return;

    const uint16_t byteIndex = static_cast<uint16_t>(sy * displayWidthBytes + (phyX >> 3));
    const uint8_t bitMask = 1 << (7 - (phyX & 7));

    if (state) {
      fb[byteIndex] &= ~bitMask;
    } else {
      fb[byteIndex] |= bitMask;
    }
  }
};

struct DirectCacheWriter {
  uint8_t* buffer;
  int bytesPerRow;
  int bandRows;
  int originX;
  uint8_t* rowPtr;

  void init(uint8_t* cacheBuffer, int cacheBytesPerRow, int cacheBandRows, int cacheOriginX) {
    buffer = cacheBuffer;
    bytesPerRow = cacheBytesPerRow;
    bandRows = cacheBandRows;
    originX = cacheOriginX;
    rowPtr = nullptr;
  }

  inline void beginRow(int screenY, int cacheOriginY) {
    const int localRow = screenY - cacheOriginY;
    rowPtr = (static_cast<unsigned>(localRow) < static_cast<unsigned>(bandRows))
                 ? buffer + (size_t)localRow * bytesPerRow
                 : nullptr;
  }

  inline void writePixel(int screenX, uint8_t value) const {
    if (!rowPtr) return;
    const int localX = screenX - originX;
    const int byteIdx = localX >> 2;
    if (static_cast<unsigned>(byteIdx) >= static_cast<unsigned>(bytesPerRow)) return;
    const int bitShift = 6 - (localX & 3) * 2;
    rowPtr[byteIdx] = (rowPtr[byteIdx] & ~(0x03 << bitShift)) | ((value & 0x03) << bitShift);
  }
};
