#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(FREEINK_MCU_S3) && !defined(SIMULATOR)

/**
 * @brief Expand a 1-bpp buffer to an 8-bpp grayscale buffer.
 *
 * Each set bit (1) in src becomes 255 in dst.
 * Each cleared bit (0) in src becomes 0 in dst.
 *
 * @param src Pointer to source 1-bpp buffer (MSB first)
 * @param dst Pointer to destination 8-bpp buffer
 * @param num_bytes Number of bytes in src to process (output will be 8 * num_bytes)
 */
void s3_expand_1bpp_to_8bpp(const uint8_t* src, uint8_t* dst, int num_bytes);

/**
 * @brief Merge three 1-bpp buffers into an 8-bpp buffer using H716 Ink-Priority logic.
 *
 * Logic (4-level):
 * 0 (Black) -> Level 15 (Luminance 0)   if !bw
 * 1 (Dark)  -> Level 14 (Luminance 11)  if bw && msb && lsb
 * 2 (Light) -> Level 11 (Luminance 56)  if bw && msb && !lsb
 * 3 (White) -> Level 0  (Luminance 255) if bw && !msb
 *
 * @param bw  Pointer to B/W base 1-bpp buffer
 * @param lsb Pointer to LSB plane 1-bpp buffer (optional, can be null)
 * @param msb Pointer to MSB plane 1-bpp buffer (optional, can be null)
 * @param dst Pointer to destination 8-bpp buffer
 * @param num_bytes Number of bytes in each input buffer to process
 */
void s3_merge_3x1bpp_to_8bpp_h716(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb, uint8_t* dst, int num_bytes);

/**
 * @brief Apply a 4x4 Bayer Dither to an 8-bpp grayscale line.
 *
 * Maps 0..255 grayscale to 0..3 (4-level) output values.
 *
 * @param src Pointer to input 8-bpp grayscale row
 * @param dst Pointer to output 8-bpp (but values 0..3) row
 * @param width Number of pixels to process
 * @param y The physical Y coordinate (used to select the dither row)
 */
void s3_bayer_dither_8bpp_to_4level_simd(const uint8_t* src, uint8_t* dst, int width, int y);

#endif

#ifdef __cplusplus
}
#endif
