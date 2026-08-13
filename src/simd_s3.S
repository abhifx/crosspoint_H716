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
