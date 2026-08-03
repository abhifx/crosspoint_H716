#pragma once

#include <cstdint>

// Central configuration for the image-to-grayscale (2-bit / 4-level) pipeline
// shared by the BMP reader, the JPEG cover converter, the PNG cover converter,
// and the sleep/screensaver image rendering.
//
// 4-level grayscale: 0=black, 1=dark gray, 2=light gray, 3=white.

constexpr bool USE_ATKINSON = true;           // Atkinson error diffusion (preferred)
constexpr bool USE_FLOYD_STEINBERG = false;   // Floyd-Steinberg (serpentine) fallback

constexpr uint8_t GRAY_LEVEL_0 = 0;
constexpr uint8_t GRAY_LEVEL_1 = 85;
constexpr uint8_t GRAY_LEVEL_2 = 170;
constexpr uint8_t GRAY_LEVEL_3 = 255;

constexpr float GAMMA_VALUE = 1.5f;  // Gamma applied to input luminance before dithering

// 8-bit -> gamma-corrected lookup table (built by initGammaLUT()).
extern uint8_t gammaLUT[256];

// Fill gammaLUT once at startup (out = 255 * (in/255)^(1/gamma)).
void initGammaLUT();
