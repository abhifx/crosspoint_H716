#include "dsps_fft2r_platform.h"
#if (dsps_fft2r_sc16_aes3_enabled == 1)

    .text
    .align 4

    // void s3_ycbcr_convert_420(uint8_t *pY, uint8_t *pCB, uint8_t *pCR, uint16_t *pOut, int16_t *pConsts, uint8_t ucPixelType);
    .global s3_ycbcr_convert_420
    .type   s3_ycbcr_convert_420,@function
    .align 4
s3_ycbcr_convert_420:
    entry   a1, 32
    beqi    a7, 3, .L_gray_conv
    retw.n

    // void s3_ycbcr_convert_444(uint8_t *pY, uint8_t *pCB, uint8_t *pCR, uint16_t *pOut, int16_t *pConsts, uint8_t ucPixelType);
    .global s3_ycbcr_convert_444
    .type   s3_ycbcr_convert_444,@function
    .align 4
s3_ycbcr_convert_444:
    entry   a1, 32
    beqi    a7, 3, .L_gray_conv
    retw.n

.align 4
.L_gray_conv:
    l32i    a8, a2, 0
    l32i    a9, a2, 4
    s32i    a8, a5, 0
    s32i    a9, a5, 4
    retw.n

    // void s3_dequant(int16_t *pMCU, int16_t *pQuant);
    .global s3_dequant
    .type   s3_dequant,@function
    .align 4
s3_dequant:
    entry   a1, 32
    movi    a8, 8
.L_dequant_loop:
    ee.vld.128.ip q0, a2, 0
    ee.vld.128.ip q1, a3, 16
    ee.vmul.s16   q0, q0, q1
    ee.vst.128.ip q0, a2, 16
    addi    a8, a8, -1
    bnez    a8, .L_dequant_loop
    retw.n

    // void s3_png_rgb888_to_gray8_simd(const uint8_t* src, uint8_t* dst, int width);
    .global s3_png_rgb888_to_gray8_simd
    .type   s3_png_rgb888_to_gray8_simd,@function
    .align 4
s3_png_rgb888_to_gray8_simd:
    entry   a1, 32

    movi    a11, 77
    movi    a12, 150
    movi    a13, 29

.L_png_loop:
    blti    a4, 4, .L_png_tail

    l8ui    a8, a2, 0  // R0
    l8ui    a9, a2, 1  // G0
    l8ui    a10, a2, 2 // B0
    mull    a8, a8, a11
    mull    a9, a9, a12
    mull    a10, a10, a13
    add     a8, a8, a9
    add     a8, a8, a10
    srli    a8, a8, 8
    s8i     a8, a3, 0

    l8ui    a8, a2, 3  // R1
    l8ui    a9, a2, 4  // G1
    l8ui    a10, a2, 5 // B1
    mull    a8, a8, a11
    mull    a9, a9, a12
    mull    a10, a10, a13
    add     a8, a8, a9
    add     a8, a8, a10
    srli    a8, a8, 8
    s8i     a8, a3, 1

    l8ui    a8, a2, 6  // R2
    l8ui    a9, a2, 7  // G2
    l8ui    a10, a2, 8 // B2
    mull    a8, a8, a11
    mull    a9, a9, a12
    mull    a10, a10, a13
    add     a8, a8, a9
    add     a8, a8, a10
    srli    a8, a8, 8
    s8i     a8, a3, 2

    l8ui    a8, a2, 9  // R3
    l8ui    a9, a2, 10 // G3
    l8ui    a10, a2, 11// B3
    mull    a8, a8, a11
    mull    a9, a9, a12
    mull    a10, a10, a13
    add     a8, a8, a9
    add     a8, a8, a10
    srli    a8, a8, 8
    s8i     a8, a3, 3

    addi    a2, a2, 12
    addi    a3, a3, 4
    addi    a4, a4, -4
    j       .L_png_loop

.L_png_tail:
    beqz    a4, .L_png_done
    l8ui    a8, a2, 0
    l8ui    a9, a2, 1
    l8ui    a10, a2, 2
    mull    a8, a8, a11
    mull    a9, a9, a12
    mull    a10, a10, a13
    add     a8, a8, a9
    add     a8, a8, a10
    srli    a8, a8, 8
    s8i     a8, a3, 0
    addi    a2, a2, 3
    addi    a3, a3, 1
    addi    a4, a4, -1
    j       .L_png_tail

.L_png_done:
    retw.n

#endif

    .text
    .align 4

    // void s3_expand_1bpp_to_8bpp(const uint8_t* src, uint8_t* dst, int num_bytes);
    .global s3_expand_1bpp_to_8bpp
    .type   s3_expand_1bpp_to_8bpp,@function
    .align 4
s3_expand_1bpp_to_8bpp:
    entry   a1, 32
    // a2 = src, a3 = dst, a4 = num_bytes
    beqz    a4, .L_expand_done

.L_expand_loop:
    l8ui    a8, a2, 0   // load 1 byte (8 pixels)

    // Bit 7
    extui   a9, a8, 7, 1
    neg     a9, a9
    s8i     a9, a3, 0
    // Bit 6
    extui   a9, a8, 6, 1
    neg     a9, a9
    s8i     a9, a3, 1
    // Bit 5
    extui   a9, a8, 5, 1
    neg     a9, a9
    s8i     a9, a3, 2
    // Bit 4
    extui   a9, a8, 4, 1
    neg     a9, a9
    s8i     a9, a3, 3
    // Bit 3
    extui   a9, a8, 3, 1
    neg     a9, a9
    s8i     a9, a3, 4
    // Bit 2
    extui   a9, a8, 2, 1
    neg     a9, a9
    s8i     a9, a3, 5
    // Bit 1
    extui   a9, a8, 1, 1
    neg     a9, a9
    s8i     a9, a3, 6
    // Bit 0
    extui   a9, a8, 0, 1
    neg     a9, a9
    s8i     a9, a3, 7

    addi    a2, a2, 1
    addi    a3, a3, 8
    addi    a4, a4, -1
    bnez    a4, .L_expand_loop

.L_expand_done:
    retw.n

    // void s3_merge_3x1bpp_to_8bpp_h716(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb, uint8_t* dst, int num_bytes);
    .global s3_merge_3x1bpp_to_8bpp_h716
    .type   s3_merge_3x1bpp_to_8bpp_h716,@function
    .align 4
s3_merge_3x1bpp_to_8bpp_h716:
    entry   a1, 32
    // a2=bw, a3=lsb, a4=msb, a5=dst, a6=num_bytes
    beqz    a6, .L_merge_done

.L_merge_loop:
    l8ui    a8, a2, 0   // bw
    movi    a9, 0
    beqz    a3, .L_m_no_lsb
    l8ui    a9, a3, 0   // lsb
    addi    a3, a3, 1
.L_m_no_lsb:
    movi    a10, 0
    beqz    a4, .L_m_no_msb
    l8ui    a10, a4, 0  // msb
    addi    a4, a4, 1
.L_m_no_msb:

    movi    a11, 8      // bit index
    movi    a12, 0x80   // mask

.L_m_pixel_loop:
    and     a13, a8, a12
    movi    a14, 0
    movi    a15, 3
    movnez  a14, a15, a13

    beqz    a13, .L_m_black
    and     a13, a10, a12
    beqz    a13, .L_m_write
    and     a13, a9, a12
    movi    a14, 2
    movi    a15, 1
    movnez  a14, a15, a13
.L_m_black:
.L_m_write:
    beqi    a14, 0, .L_m_15
    beqi    a14, 1, .L_m_14
    beqi    a14, 2, .L_m_11
    movi    a14, 0
    j       .L_m_store
.L_m_15: movi a14, 15 ; j .L_m_store
.L_m_14: movi a14, 14 ; j .L_m_store
.L_m_11: movi a14, 11

.L_m_store:
    s8i     a14, a5, 0
    addi    a5, a5, 1
    srli    a12, a12, 1
    addi    a11, a11, -1
    bnez    a11, .L_m_pixel_loop

    addi    a2, a2, 1
    addi    a6, a6, -1
    bnez    a6, .L_merge_loop

.L_merge_done:
    retw.n

    // void s3_bayer_dither_8bpp_to_4level_simd(const uint8_t* src, uint8_t* dst, int width, int y);
    .global s3_bayer_dither_8bpp_to_4level_simd
    .type   s3_bayer_dither_8bpp_to_4level_simd,@function
    .align 4
s3_bayer_dither_8bpp_to_4level_simd:
    entry   a1, 32
    // a2=src, a3=dst, a4=width, a5=y
    beqz    a4, .L_d_done

    // Select Bayer matrix row based on y % 4
    extui   a8, a5, 0, 2
    // bayer4x4[4][4] = {
    //   { 0, 128, 32, 160 },
    //   { 192, 64, 224, 96 },
    //   { 48, 176, 16, 144 },
    //   { 240, 112, 208, 80 }
    // }
    // Scaling for 4 levels (0..255 -> 0..3):
    // threshold = bayer[y%4][x%4] >> 2  (roughly)
    // Actually we keep 8-bit thresholds.

    // Load row thresholds into a9, a10, a11, a12
    beqi    a8, 0, .L_d_row0
    beqi    a8, 1, .L_d_row1
    beqi    a8, 2, .L_d_row2
    // row 3
    movi a9, 240 ; movi a10, 112 ; movi a11, 208 ; movi a12, 80 ; j .L_d_loop
.L_d_row0: movi a9, 0   ; movi a10, 128 ; movi a11, 32  ; movi a12, 160 ; j .L_d_loop
.L_d_row1: movi a9, 192 ; movi a10, 64  ; movi a11, 224 ; movi a12, 96  ; j .L_d_loop
.L_d_row2: movi a9, 48  ; movi a10, 176 ; movi a11, 16  ; movi a12, 144

.L_d_loop:
    blti    a4, 4, .L_d_tail

    // Process 4 pixels
    l8ui    a13, a2, 0  // p0
    l8ui    a14, a2, 1  // p1
    l8ui    a15, a2, 2  // p2
    l8ui    a8, a2, 3   // p3

    // Pixel 0 (threshold a9)
    // val = (p + threshold) >> 6
    // clamp to 3
    add     a13, a13, a9
    srli    a13, a13, 6
    blti    a13, 4, .L_d_s0
    movi    a13, 3
.L_d_s0: s8i a13, a3, 0

    // Pixel 1 (threshold a10)
    add     a14, a14, a10
    srli    a14, a14, 6
    blti    a14, 4, .L_d_s1
    movi    a14, 3
.L_d_s1: s8i a14, a3, 1

    // Pixel 2 (threshold a11)
    add     a15, a15, a11
    srli    a15, a15, 6
    blti    a15, 4, .L_d_s2
    movi    a15, 3
.L_d_s2: s8i a15, a3, 2

    // Pixel 3 (threshold a12)
    add     a8, a8, a12
    srli    a8, a8, 6
    blti    a8, 4, .L_d_s3
    movi    a8, 3
.L_d_s3: s8i a8, a3, 3

    addi    a2, a2, 4
    addi    a3, a3, 4
    addi    a4, a4, -4
    bnez    a4, .L_d_loop

.L_d_tail:
.L_d_done:
    retw.n
