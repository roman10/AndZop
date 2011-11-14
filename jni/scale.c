/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "scale.h"
#include "cpu_id.h"
#include <assert.h>
#include <string.h>

//#include "row.h"

#if defined(_MSC_VER)
#define ALIGN16(var) __declspec(align(16)) var
#else
#define ALIGN16(var) var __attribute__((aligned(16)))
#endif


// Note: A Neon reference manual
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0204j/CJAJIIGG.html
// Note: Some SSE2 reference manuals
// cpuvol1.pdf agner_instruction_tables.pdf 253666.pdf 253667.pdf


// Set the following flag to 1 to revert to only
// using the reference implementation ScalePlaneBox(), and
// NOT the optimized versions. Useful for debugging and
// when comparing the quality of the resulting YUV planes
// as produced by the optimized and non-optimized versions.

static int use_reference_impl_ = 0;

void SetUseReferenceImpl(int use) {
  use_reference_impl_ = use;
}

/**
 * NEON downscalers with interpolation.
 *
 * Provided by Fritz Koenig
 *
 */

/**roman10: comments
vld2.u8 {q0, q1}, [%0]!
vld1: simples form. loads one to four registers of data from memory, with no interleaving. Use this when processing an array of non-interleaved data.
vld2: loads two or four registers of data, deinterleaving even and odd elements into those registers. Use this to separate stereo audio data into left and right channels.
vld3: loads there three registers and deinterleaves. Useful for splitting RGB pixels into channels.
vld4: loads four registers and deinterleaves. Use it to process ARGB image data.
element type: specify number of bits.
a set of NEON registers to be read or write.
Addressing: 
[]: simplest form. data will be loaded and stored to specified address
[]!: update the pointer after loading or storing, ready to load or store the next elements. The increment is equal to number of bytes read or written by the instruction. 
[],: after memory access, the pointer is incremented by the value in register Rm. Useful when reading or writing groups of elements that are separated by fixed widths, eg. when reading a vertical line of data from an image

NEON has 16 128-bit wide registers (Q0, Q1, etc) or 32 64-bit wide registers (D1, D2, etc)
*/

#if defined(__ARM_NEON__) && !defined(COVERAGE_ENABLED)
#define HAS_SCALEROWDOWN2_NEON
void ScaleRowDown2_NEON(const unsigned char* src_ptr, int src_stride/* src_stride */,
                        unsigned char* dst, int dst_width) {
  asm volatile (
    "1:                                        \n"
    "vld2.u8    {q0,q1}, [%0]!                 \n"  // load even pixels into q0, odd into q1
    "vst1.u8    {q0}, [%1]!                    \n"  // store even pixels
    "subs       %2, %2, #16                    \n"  // 16 processed per loop
    "bhi        1b                             \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst),              // %1
      "+r"(dst_width)         // %2
    :
    : "q0", "q1"              // Clobber List
  );
}

void ScaleRowDown2Int_NEON(const unsigned char* src_ptr, int src_stride,
                           unsigned char* dst, int dst_width) {
  asm volatile (
    "add        %1, %0                         \n"  // change the stride to row 2 pointer
    "1:                                        \n"
    "vld1.u8    {q0,q1}, [%0]!                 \n"  // load row 1 and post increment
    "vld1.u8    {q2,q3}, [%1]!                 \n"  // load row 2 and post increment
    "vpaddl.u8  q0, q0                         \n"  // row 1 add adjacent
    "vpaddl.u8  q1, q1                         \n"
    "vpadal.u8  q0, q2                         \n"  // row 2 add adjacent, add row 1 to row 2
    "vpadal.u8  q1, q3                         \n"
    "vrshrn.u16 d0, q0, #2                     \n"  // downshift, round and pack
    "vrshrn.u16 d1, q1, #2                     \n"
    "vst1.u8    {q0}, [%2]!                    \n"
    "subs       %3, %3, #16                    \n"  // 16 processed per loop
    "bhi        1b                             \n"
    : "+r"(src_ptr),          // %0
      "+r"(src_stride),       // %1
      "+r"(dst),              // %2
      "+r"(dst_width)         // %3
    :
    : "q0", "q1", "q2", "q3"     // Clobber List
   );
}

#define HAS_SCALEROWDOWN4_NEON
static void ScaleRowDown4_NEON(const unsigned char* src_ptr, int src_stride/* src_stride */,
                               unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "1:                                        \n"
    "vld2.u8    {d0, d1}, [%0]!                \n"
    "vtrn.u8    d1, d0                         \n"
    "vshrn.u16  d0, q0, #8                     \n"
    "vst1.u32   {d0[1]}, [%1]!                 \n"

    "subs       %2, #4                         \n"
    "bhi        1b                             \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    :
    : "q0", "q1", "memory", "cc"
  );
}

static void ScaleRowDown4Int_NEON(const unsigned char* src_ptr, int src_stride,
                                  unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "add        r4, %0, %3                     \n"
    "add        r5, r4, %3                     \n"
    "add        %3, r5, %3                     \n"
    "1:                                        \n"
    "vld1.u8    {q0}, [%0]!                    \n"   // load up 16x4 block of input data
    "vld1.u8    {q1}, [r4]!                    \n"
    "vld1.u8    {q2}, [r5]!                    \n"
    "vld1.u8    {q3}, [%3]!                    \n"

    "vpaddl.u8  q0, q0                         \n"
    "vpadal.u8  q0, q1                         \n"
    "vpadal.u8  q0, q2                         \n"
    "vpadal.u8  q0, q3                         \n"

    "vpaddl.u16 q0, q0                         \n"

    "vrshrn.u32 d0, q0, #4                     \n"   // divide by 16 w/rounding

    "vmovn.u16  d0, q0                         \n"
    "vst1.u32   {d0[0]}, [%1]!                 \n"

    "subs       %2, #4                         \n"
    "bhi        1b                             \n"

    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    : "r"(src_stride)         // %3
    : "r4", "r5", "q0", "q1", "q2", "q3", "memory", "cc"
  );
}

#define HAS_SCALEROWDOWN34_NEON
// Down scale from 4 to 3 pixels.  Use the neon multilane read/write
//  to load up the every 4th pixel into a 4 different registers.
// Point samples 32 pixels to 24 pixels.
static void ScaleRowDown34_NEON(const unsigned char* src_ptr, int src_stride/* src_stride */,
                                unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "1:                                        \n"
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n" // src line 0
    "vmov         d2, d3                       \n" // order needs to be d0, d1, d2
    "vst3.u8      {d0, d1, d2}, [%1]!          \n"
    "subs         %2, #24                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    :
    : "d0", "d1", "d2", "d3", "memory", "cc"
  );
}

static void ScaleRowDown34_0_Int_NEON(const unsigned char* src_ptr, int src_stride,
                                      unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "vmov.u8      d24, #3                      \n"
    "add          %3, %0                       \n"
    "1:                                        \n"
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n" // src line 0
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n" // src line 1

    // filter src line 0 with src line 1
    // expand chars to shorts to allow for room
    // when adding lines together
    "vmovl.u8     q8, d4                       \n"
    "vmovl.u8     q9, d5                       \n"
    "vmovl.u8     q10, d6                      \n"
    "vmovl.u8     q11, d7                      \n"

    // 3 * line_0 + line_1
    "vmlal.u8     q8, d0, d24                  \n"
    "vmlal.u8     q9, d1, d24                  \n"
    "vmlal.u8     q10, d2, d24                 \n"
    "vmlal.u8     q11, d3, d24                 \n"

    // (3 * line_0 + line_1) >> 2
    "vqrshrn.u16  d0, q8, #2                   \n"
    "vqrshrn.u16  d1, q9, #2                   \n"
    "vqrshrn.u16  d2, q10, #2                  \n"
    "vqrshrn.u16  d3, q11, #2                  \n"

    // a0 = (src[0] * 3 + s[1] * 1) >> 2
    "vmovl.u8     q8, d1                       \n"
    "vmlal.u8     q8, d0, d24                  \n"
    "vqrshrn.u16  d0, q8, #2                   \n"

    // a1 = (src[1] * 1 + s[2] * 1) >> 1
    "vrhadd.u8    d1, d1, d2                   \n"

    // a2 = (src[2] * 1 + s[3] * 3) >> 2
    "vmovl.u8     q8, d2                       \n"
    "vmlal.u8     q8, d3, d24                  \n"
    "vqrshrn.u16  d2, q8, #2                   \n"

    "vst3.u8      {d0, d1, d2}, [%1]!          \n"

    "subs         %2, #24                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    :
    : "q0", "q1", "q2", "q3", "q8", "q9", "q10", "q11", "d24", "memory", "cc"
  );
}

static void ScaleRowDown34_1_Int_NEON(const unsigned char* src_ptr, int src_stride,
                                      unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "vmov.u8      d24, #3                      \n"
    "add          %3, %0                       \n"
    "1:                                        \n"
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n" // src line 0
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n" // src line 1

    // average src line 0 with src line 1
    "vrhadd.u8    q0, q0, q2                   \n"
    "vrhadd.u8    q1, q1, q3                   \n"

    // a0 = (src[0] * 3 + s[1] * 1) >> 2
    "vmovl.u8     q3, d1                       \n"
    "vmlal.u8     q3, d0, d24                  \n"
    "vqrshrn.u16  d0, q3, #2                   \n"

    // a1 = (src[1] * 1 + s[2] * 1) >> 1
    "vrhadd.u8    d1, d1, d2                   \n"

    // a2 = (src[2] * 1 + s[3] * 3) >> 2
    "vmovl.u8     q3, d2                       \n"
    "vmlal.u8     q3, d3, d24                  \n"
    "vqrshrn.u16  d2, q3, #2                   \n"

    "vst3.u8      {d0, d1, d2}, [%1]!          \n"

    "subs         %2, #24                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    :
    : "r4", "q0", "q1", "q2", "q3", "d24", "memory", "cc"
  );
}

#define HAS_SCALEROWDOWN38_NEON
const unsigned char shuf38[16] __attribute__ ((aligned(16))) =
  { 0, 3, 6, 8, 11, 14, 16, 19, 22, 24, 27, 30, 0, 0, 0, 0 };
const unsigned char shuf38_2[16] __attribute__ ((aligned(16))) =
  { 0, 8, 16, 2, 10, 17, 4, 12, 18, 6, 14, 19, 0, 0, 0, 0 };
const unsigned short mult38_div6[8] __attribute__ ((aligned(16))) =
  { 65536 / 12, 65536 / 12, 65536 / 12, 65536 / 12,
    65536 / 12, 65536 / 12, 65536 / 12, 65536 / 12 };
const unsigned short mult38_div9[8] __attribute__ ((aligned(16))) =
  { 65536 / 18, 65536 / 18, 65536 / 18, 65536 / 18,
    65536 / 18, 65536 / 18, 65536 / 18, 65536 / 18 };

// 32 -> 12
static void ScaleRowDown38_NEON(const unsigned char* src_ptr, int src_stride,
                                unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "vld1.u8      {q3}, [%3]                   \n"
    "1:                                        \n"
    "vld1.u8      {d0, d1, d2, d3}, [%0]!      \n"
    "vtbl.u8      d4, {d0, d1, d2, d3}, d6     \n"
    "vtbl.u8      d5, {d0, d1, d2, d3}, d7     \n"
    "vst1.u8      {d4}, [%1]!                  \n"
    "vst1.u32     {d5[0]}, [%1]!               \n"
    "subs         %2, #12                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width)         // %2
    : "r"(shuf38)             // %3
    : "d0", "d1", "d2", "d3", "d4", "d5", "memory", "cc"
  );
}

// 32x3 -> 12x1
static void ScaleRowDown38_3_Int_NEON(const unsigned char* src_ptr, int src_stride,
                                      unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "vld1.u16     {q13}, [%4]                  \n"
    "vld1.u8      {q14}, [%5]                  \n"
    "vld1.u8      {q15}, [%6]                  \n"
    "add          r4, %0, %3, lsl #1           \n"
    "add          %3, %0                       \n"
    "1:                                        \n"

    // d0 = 00 40 01 41 02 42 03 43
    // d1 = 10 50 11 51 12 52 13 53
    // d2 = 20 60 21 61 22 62 23 63
    // d3 = 30 70 31 71 32 72 33 73
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n"
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n"
    "vld4.u8      {d16, d17, d18, d19}, [r4]!  \n"

    // Shuffle the input data around to get align the data
    //  so adjacent data can be added.  0,1 - 2,3 - 4,5 - 6,7
    // d0 = 00 10 01 11 02 12 03 13
    // d1 = 40 50 41 51 42 52 43 53
    "vtrn.u8      d0, d1                       \n"
    "vtrn.u8      d4, d5                       \n"
    "vtrn.u8      d16, d17                     \n"

    // d2 = 20 30 21 31 22 32 23 33
    // d3 = 60 70 61 71 62 72 63 73
    "vtrn.u8      d2, d3                       \n"
    "vtrn.u8      d6, d7                       \n"
    "vtrn.u8      d18, d19                     \n"

    // d0 = 00+10 01+11 02+12 03+13
    // d2 = 40+50 41+51 42+52 43+53
    "vpaddl.u8    q0, q0                       \n"
    "vpaddl.u8    q2, q2                       \n"
    "vpaddl.u8    q8, q8                       \n"

    // d3 = 60+70 61+71 62+72 63+73
    "vpaddl.u8    d3, d3                       \n"
    "vpaddl.u8    d7, d7                       \n"
    "vpaddl.u8    d19, d19                     \n"

    // combine source lines
    "vadd.u16     q0, q2                       \n"
    "vadd.u16     q0, q8                       \n"
    "vadd.u16     d4, d3, d7                   \n"
    "vadd.u16     d4, d19                      \n"

    // dst_ptr[3] = (s[6 + st * 0] + s[7 + st * 0]
    //             + s[6 + st * 1] + s[7 + st * 1]
    //             + s[6 + st * 2] + s[7 + st * 2]) / 6
    "vqrdmulh.s16 q2, q13                      \n"
    "vmovn.u16    d4, q2                       \n"

    // Shuffle 2,3 reg around so that 2 can be added to the
    //  0,1 reg and 3 can be added to the 4,5 reg.  This
    //  requires expanding from u8 to u16 as the 0,1 and 4,5
    //  registers are already expanded.  Then do transposes
    //  to get aligned.
    // q2 = xx 20 xx 30 xx 21 xx 31 xx 22 xx 32 xx 23 xx 33
    "vmovl.u8     q1, d2                       \n"
    "vmovl.u8     q3, d6                       \n"
    "vmovl.u8     q9, d18                      \n"

    // combine source lines
    "vadd.u16     q1, q3                       \n"
    "vadd.u16     q1, q9                       \n"

    // d4 = xx 20 xx 30 xx 22 xx 32
    // d5 = xx 21 xx 31 xx 23 xx 33
    "vtrn.u32     d2, d3                       \n"

    // d4 = xx 20 xx 21 xx 22 xx 23
    // d5 = xx 30 xx 31 xx 32 xx 33
    "vtrn.u16     d2, d3                       \n"

    // 0+1+2, 3+4+5
    "vadd.u16     q0, q1                       \n"

    // Need to divide, but can't downshift as the the value
    //  isn't a power of 2.  So multiply by 65536 / n
    //  and take the upper 16 bits.
    "vqrdmulh.s16 q0, q15                      \n"

    // Align for table lookup, vtbl requires registers to
    //  be adjacent
    "vmov.u8      d2, d4                       \n"

    "vtbl.u8      d3, {d0, d1, d2}, d28        \n"
    "vtbl.u8      d4, {d0, d1, d2}, d29        \n"

    "vst1.u8      {d3}, [%1]!                  \n"
    "vst1.u32     {d4[0]}, [%1]!               \n"
    "subs         %2, #12                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    : "r"(mult38_div6),       // %4
      "r"(shuf38_2),          // %5
      "r"(mult38_div9)        // %6
    : "r4", "q0", "q1", "q2", "q3", "q8", "q9",
      "q13", "q14", "q15", "memory", "cc"
  );
}

// 32x2 -> 12x1
static void ScaleRowDown38_2_Int_NEON(const unsigned char* src_ptr, int src_stride,
                                      unsigned char* dst_ptr, int dst_width) {
  asm volatile (
    "vld1.u16     {q13}, [%4]                  \n"
    "vld1.u8      {q14}, [%5]                  \n"
    "add          %3, %0                       \n"
    "1:                                        \n"

    // d0 = 00 40 01 41 02 42 03 43
    // d1 = 10 50 11 51 12 52 13 53
    // d2 = 20 60 21 61 22 62 23 63
    // d3 = 30 70 31 71 32 72 33 73
    "vld4.u8      {d0, d1, d2, d3}, [%0]!      \n"
    "vld4.u8      {d4, d5, d6, d7}, [%3]!      \n"

    // Shuffle the input data around to get align the data
    //  so adjacent data can be added.  0,1 - 2,3 - 4,5 - 6,7
    // d0 = 00 10 01 11 02 12 03 13
    // d1 = 40 50 41 51 42 52 43 53
    "vtrn.u8      d0, d1                       \n"
    "vtrn.u8      d4, d5                       \n"

    // d2 = 20 30 21 31 22 32 23 33
    // d3 = 60 70 61 71 62 72 63 73
    "vtrn.u8      d2, d3                       \n"
    "vtrn.u8      d6, d7                       \n"

    // d0 = 00+10 01+11 02+12 03+13
    // d2 = 40+50 41+51 42+52 43+53
    "vpaddl.u8    q0, q0                       \n"
    "vpaddl.u8    q2, q2                       \n"

    // d3 = 60+70 61+71 62+72 63+73
    "vpaddl.u8    d3, d3                       \n"
    "vpaddl.u8    d7, d7                       \n"

    // combine source lines
    "vadd.u16     q0, q2                       \n"
    "vadd.u16     d4, d3, d7                   \n"

    // dst_ptr[3] = (s[6] + s[7] + s[6+st] + s[7+st]) / 4
    "vqrshrn.u16  d4, q2, #2                   \n"

    // Shuffle 2,3 reg around so that 2 can be added to the
    //  0,1 reg and 3 can be added to the 4,5 reg.  This
    //  requires expanding from u8 to u16 as the 0,1 and 4,5
    //  registers are already expanded.  Then do transposes
    //  to get aligned.
    // q2 = xx 20 xx 30 xx 21 xx 31 xx 22 xx 32 xx 23 xx 33
    "vmovl.u8     q1, d2                       \n"
    "vmovl.u8     q3, d6                       \n"

    // combine source lines
    "vadd.u16     q1, q3                       \n"

    // d4 = xx 20 xx 30 xx 22 xx 32
    // d5 = xx 21 xx 31 xx 23 xx 33
    "vtrn.u32     d2, d3                       \n"

    // d4 = xx 20 xx 21 xx 22 xx 23
    // d5 = xx 30 xx 31 xx 32 xx 33
    "vtrn.u16     d2, d3                       \n"

    // 0+1+2, 3+4+5
    "vadd.u16     q0, q1                       \n"

    // Need to divide, but can't downshift as the the value
    //  isn't a power of 2.  So multiply by 65536 / n
    //  and take the upper 16 bits.
    "vqrdmulh.s16 q0, q13                      \n"

    // Align for table lookup, vtbl requires registers to
    //  be adjacent
    "vmov.u8      d2, d4                       \n"

    "vtbl.u8      d3, {d0, d1, d2}, d28        \n"
    "vtbl.u8      d4, {d0, d1, d2}, d29        \n"

    "vst1.u8      {d3}, [%1]!                  \n"
    "vst1.u32     {d4[0]}, [%1]!               \n"
    "subs         %2, #12                      \n"
    "bhi          1b                           \n"
    : "+r"(src_ptr),          // %0
      "+r"(dst_ptr),          // %1
      "+r"(dst_width),        // %2
      "+r"(src_stride)        // %3
    : "r"(mult38_div6),       // %4
      "r"(shuf38_2)           // %5
    : "q0", "q1", "q2", "q3", "q13", "q14", "memory", "cc"
  );
}
#endif



// CPU agnostic row functions
static void ScaleRowDown2_C(const unsigned char* src_ptr, int src_stride,
                            unsigned char* dst, int dst_width) {
  int x;
  for (x = 0; x < dst_width; ++x) {
    *dst++ = *src_ptr;
    src_ptr += 2;
  }
}

static void ScaleRowDown2Int_C(const unsigned char* src_ptr, int src_stride,
                               unsigned char* dst, int dst_width) {
  int x;
  for (x = 0; x < dst_width; ++x) {
    *dst++ = (src_ptr[0] + src_ptr[1] +
              src_ptr[src_stride] + src_ptr[src_stride + 1] + 2) >> 2;
    src_ptr += 2;
  }
}

static void ScaleRowDown4_C(const unsigned char* src_ptr, int src_stride,
                            unsigned char* dst, int dst_width) {
  int x;
  for (x = 0; x < dst_width; ++x) {
    *dst++ = *src_ptr;
    src_ptr += 4;
  }
}

static void ScaleRowDown4Int_C(const unsigned char* src_ptr, int src_stride,
                               unsigned char* dst, int dst_width) {
  int x;
  for (x = 0; x < dst_width; ++x) {
    *dst++ = (src_ptr[0] + src_ptr[1] + src_ptr[2] + src_ptr[3] +
              src_ptr[src_stride + 0] + src_ptr[src_stride + 1] +
              src_ptr[src_stride + 2] + src_ptr[src_stride + 3] +
              src_ptr[src_stride * 2 + 0] + src_ptr[src_stride * 2 + 1] +
              src_ptr[src_stride * 2 + 2] + src_ptr[src_stride * 2 + 3] +
              src_ptr[src_stride * 3 + 0] + src_ptr[src_stride * 3 + 1] +
              src_ptr[src_stride * 3 + 2] + src_ptr[src_stride * 3 + 3] +
              8) >> 4;
    src_ptr += 4;
  }
}

// 640 output pixels is enough to allow 5120 input pixels with 1/8 scale down.
// Keeping the total buffer under 4096 bytes avoids a stackcheck, saving 4% cpu.
static const int kMaxOutputWidth = 640;
static const int kMaxRow12 = 1280; //kMaxOutputWidth * 2;

static void ScaleRowDown8_C(const unsigned char* src_ptr, int src_stride,
                            unsigned char* dst, int dst_width) {
  int x;
  for (x = 0; x < dst_width; ++x) {
    *dst++ = *src_ptr;
    src_ptr += 8;
  }
}

// Note calling code checks width is less than max and if not
// uses ScaleRowDown8_C instead.
static void ScaleRowDown8Int_C(const unsigned char* src_ptr, int src_stride,
                               unsigned char* dst, int dst_width) {
  ALIGN16(unsigned char src_row[kMaxRow12 * 2]);
  assert(dst_width <= kMaxOutputWidth);
  ScaleRowDown4Int_C(src_ptr, src_stride, src_row, dst_width * 2);
  ScaleRowDown4Int_C(src_ptr + src_stride * 4, src_stride,
                     src_row + kMaxOutputWidth,
                     dst_width * 2);
  ScaleRowDown2Int_C(src_row, kMaxOutputWidth, dst, dst_width);
}

static void ScaleRowDown34_C(const unsigned char* src_ptr, int src_stride,
                             unsigned char* dst, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  unsigned char* dend = dst + dst_width;
  do {
    dst[0] = src_ptr[0];
    dst[1] = src_ptr[1];
    dst[2] = src_ptr[3];
    dst += 3;
    src_ptr += 4;
  } while (dst < dend);
}

// Filter rows 0 and 1 together, 3 : 1
static void ScaleRowDown34_0_Int_C(const unsigned char* src_ptr, int src_stride,
                                   unsigned char* d, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  unsigned char* dend = d + dst_width;
  const unsigned char* s = src_ptr;
  const unsigned char* t = src_ptr + src_stride;
  do {
    unsigned char a0 = (s[0] * 3 + s[1] * 1 + 2) >> 2;
    unsigned char a1 = (s[1] * 1 + s[2] * 1 + 1) >> 1;
    unsigned char a2 = (s[2] * 1 + s[3] * 3 + 2) >> 2;
    unsigned char b0 = (t[0] * 3 + t[1] * 1 + 2) >> 2;
    unsigned char b1 = (t[1] * 1 + t[2] * 1 + 1) >> 1;
    unsigned char b2 = (t[2] * 1 + t[3] * 3 + 2) >> 2;
    d[0] = (a0 * 3 + b0 + 2) >> 2;
    d[1] = (a1 * 3 + b1 + 2) >> 2;
    d[2] = (a2 * 3 + b2 + 2) >> 2;
    d += 3;
    s += 4;
    t += 4;
  } while (d < dend);
}

// Filter rows 1 and 2 together, 1 : 1
static void ScaleRowDown34_1_Int_C(const unsigned char* src_ptr, int src_stride,
                                   unsigned char* d, int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  unsigned char* dend = d + dst_width;
  const unsigned char* s = src_ptr;
  const unsigned char* t = src_ptr + src_stride;
  do {
    unsigned char a0 = (s[0] * 3 + s[1] * 1 + 2) >> 2;
    unsigned char a1 = (s[1] * 1 + s[2] * 1 + 1) >> 1;
    unsigned char a2 = (s[2] * 1 + s[3] * 3 + 2) >> 2;
    unsigned char b0 = (t[0] * 3 + t[1] * 1 + 2) >> 2;
    unsigned char b1 = (t[1] * 1 + t[2] * 1 + 1) >> 1;
    unsigned char b2 = (t[2] * 1 + t[3] * 3 + 2) >> 2;
    d[0] = (a0 + b0 + 1) >> 1;
    d[1] = (a1 + b1 + 1) >> 1;
    d[2] = (a2 + b2 + 1) >> 1;
    d += 3;
    s += 4;
    t += 4;
  } while (d < dend);
}

#if defined(HAS_SCALEFILTERROWS_SSE2)
// Filter row to 3/4
static void ScaleFilterCols34_C(unsigned char* dst_ptr, const unsigned char* src_ptr,
                                int dst_width) {
  assert((dst_width % 3 == 0) && (dst_width > 0));
  unsigned char* dend = dst_ptr + dst_width;
  const unsigned char* s = src_ptr;
  do {
    dst_ptr[0] = (s[0] * 3 + s[1] * 1 + 2) >> 2;
    dst_ptr[1] = (s[1] * 1 + s[2] * 1 + 1) >> 1;
    dst_ptr[2] = (s[2] * 1 + s[3] * 3 + 2) >> 2;
    dst_ptr += 3;
    s += 4;
  } while (dst_ptr < dend);
}
#endif

static void ScaleFilterCols_C(unsigned char* dst_ptr, const unsigned char* src_ptr,
                              int dst_width, int dx) {
  int x = 0, j, xi, xf1, xf0;
  for (j = 0; j < dst_width; ++j) {
    xi = x >> 16;
    xf1 = x & 0xffff;
    xf0 = 65536 - xf1;

    *dst_ptr++ = (src_ptr[xi] * xf0 + src_ptr[xi + 1] * xf1) >> 16;
    x += dx;
  }
}

static const int kMaxInputWidth = 2560;
static void ScaleRowDown38_C(const unsigned char* src_ptr, int src_stride,
                             unsigned char* dst, int dst_width) {
  int x;
  assert(dst_width % 3 == 0);
  for (x = 0; x < dst_width; x += 3) {
    dst[0] = src_ptr[0];
    dst[1] = src_ptr[3];
    dst[2] = src_ptr[6];
    dst += 3;
    src_ptr += 8;
  }
}

// 8x3 -> 3x1
static void ScaleRowDown38_3_Int_C(const unsigned char* src_ptr, int src_stride,
                                   unsigned char* dst_ptr, int dst_width) {
  int i;
  assert((dst_width % 3 == 0) && (dst_width > 0));
  for (i = 0; i < dst_width; i+=3) {
    dst_ptr[0] = (src_ptr[0] + src_ptr[1] + src_ptr[2] +
        src_ptr[src_stride + 0] + src_ptr[src_stride + 1] +
        src_ptr[src_stride + 2] + src_ptr[src_stride * 2 + 0] +
        src_ptr[src_stride * 2 + 1] + src_ptr[src_stride * 2 + 2]) *
        (65536 / 9) >> 16;
    dst_ptr[1] = (src_ptr[3] + src_ptr[4] + src_ptr[5] +
        src_ptr[src_stride + 3] + src_ptr[src_stride + 4] +
        src_ptr[src_stride + 5] + src_ptr[src_stride * 2 + 3] +
        src_ptr[src_stride * 2 + 4] + src_ptr[src_stride * 2 + 5]) *
        (65536 / 9) >> 16;
    dst_ptr[2] = (src_ptr[6] + src_ptr[7] +
        src_ptr[src_stride + 6] + src_ptr[src_stride + 7] +
        src_ptr[src_stride * 2 + 6] + src_ptr[src_stride * 2 + 7]) *
        (65536 / 6) >> 16;
    src_ptr += 8;
    dst_ptr += 3;
  }
}

// 8x2 -> 3x1
static void ScaleRowDown38_2_Int_C(const unsigned char* src_ptr, int src_stride,
                                   unsigned char* dst_ptr, int dst_width) {
  int i;
  assert((dst_width % 3 == 0) && (dst_width > 0));
  for (i = 0; i < dst_width; i+=3) {
    dst_ptr[0] = (src_ptr[0] + src_ptr[1] + src_ptr[2] +
        src_ptr[src_stride + 0] + src_ptr[src_stride + 1] +
        src_ptr[src_stride + 2]) * (65536 / 6) >> 16;
    dst_ptr[1] = (src_ptr[3] + src_ptr[4] + src_ptr[5] +
        src_ptr[src_stride + 3] + src_ptr[src_stride + 4] +
        src_ptr[src_stride + 5]) * (65536 / 6) >> 16;
    dst_ptr[2] = (src_ptr[6] + src_ptr[7] +
        src_ptr[src_stride + 6] + src_ptr[src_stride + 7]) *
        (65536 / 4) >> 16;
    src_ptr += 8;
    dst_ptr += 3;
  }
}

// C version 8x2 -> 8x1
static void ScaleFilterRows_C(unsigned char* dst_ptr,
                              const unsigned char* src_ptr, int src_stride,
                              int dst_width, int source_y_fraction) {
  assert(dst_width > 0);
  int y1_fraction = source_y_fraction;
  int y0_fraction = 256 - y1_fraction;
  const unsigned char* src_ptr1 = src_ptr + src_stride;
  unsigned char* end = dst_ptr + dst_width;
  do {
    dst_ptr[0] = (src_ptr[0] * y0_fraction + src_ptr1[0] * y1_fraction) >> 8;
    dst_ptr[1] = (src_ptr[1] * y0_fraction + src_ptr1[1] * y1_fraction) >> 8;
    dst_ptr[2] = (src_ptr[2] * y0_fraction + src_ptr1[2] * y1_fraction) >> 8;
    dst_ptr[3] = (src_ptr[3] * y0_fraction + src_ptr1[3] * y1_fraction) >> 8;
    dst_ptr[4] = (src_ptr[4] * y0_fraction + src_ptr1[4] * y1_fraction) >> 8;
    dst_ptr[5] = (src_ptr[5] * y0_fraction + src_ptr1[5] * y1_fraction) >> 8;
    dst_ptr[6] = (src_ptr[6] * y0_fraction + src_ptr1[6] * y1_fraction) >> 8;
    dst_ptr[7] = (src_ptr[7] * y0_fraction + src_ptr1[7] * y1_fraction) >> 8;
    src_ptr += 8;
    src_ptr1 += 8;
    dst_ptr += 8;
  } while (dst_ptr < end);
  dst_ptr[0] = dst_ptr[-1];
}

void ScaleAddRows_C(const unsigned char* src_ptr, int src_stride,
                    unsigned short* dst_ptr, int src_width, int src_height) {
  int x, sum, y;
  const unsigned char* s;
  assert(src_width > 0);
  assert(src_height > 0);
  for (x = 0; x < src_width; ++x) {
    s = src_ptr + x;
    sum = 0;
    for (y = 0; y < src_height; ++y) {
      sum += s[0];
      s += src_stride;
    }
    dst_ptr[x] = sum;
  }
}

/**
 * Scale plane, 1/2
 *
 * This is an optimized version for scaling down a plane to 1/2 of
 * its original size.
 *
 */
static void ScalePlaneDown2(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const unsigned char* src_ptr, unsigned char* dst_ptr,
                            enum FilterMode filtering) {
  int y;
  assert(src_width % 2 == 0);
  assert(src_height % 2 == 0);
  void (*ScaleRowDown2)(const unsigned char* src_ptr, int src_stride,
                        unsigned char* dst_ptr, int dst_width);

#if defined(HAS_SCALEROWDOWN2_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      (dst_width % 16 == 0)) {
    ScaleRowDown2 = filtering ? ScaleRowDown2Int_NEON : ScaleRowDown2_NEON;
  } else
#endif
  {
    ScaleRowDown2 = filtering ? ScaleRowDown2Int_C : ScaleRowDown2_C;
  }

  for (y = 0; y < dst_height; ++y) {
    ScaleRowDown2(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 1);
    dst_ptr += dst_stride;
  }
}

/**
 * Scale plane, 1/4
 *
 * This is an optimized version for scaling down a plane to 1/4 of
 * its original size.
 */
static void ScalePlaneDown4(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const unsigned char* src_ptr, unsigned char* dst_ptr,
                            enum FilterMode filtering) {
  int y;
  assert(src_width % 4 == 0);
  assert(src_height % 4 == 0);
  void (*ScaleRowDown4)(const unsigned char* src_ptr, int src_stride,
                        unsigned char* dst_ptr, int dst_width);

#if defined(HAS_SCALEROWDOWN4_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      (dst_width % 4 == 0)) {
    ScaleRowDown4 = filtering ? ScaleRowDown4Int_NEON : ScaleRowDown4_NEON;
  } else
#endif
  {
    ScaleRowDown4 = filtering ? ScaleRowDown4Int_C : ScaleRowDown4_C;
  }

  for (y = 0; y < dst_height; ++y) {
    ScaleRowDown4(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 2);
    dst_ptr += dst_stride;
  }
}

/**
 * Scale plane, 1/8
 *
 * This is an optimized version for scaling down a plane to 1/8
 * of its original size.
 *
 */
static void ScalePlaneDown8(int src_width, int src_height,
                            int dst_width, int dst_height,
                            int src_stride, int dst_stride,
                            const unsigned char* src_ptr, unsigned char* dst_ptr,
                            enum FilterMode filtering) {
  int y;
  assert(src_width % 8 == 0);
  assert(src_height % 8 == 0);
  void (*ScaleRowDown8)(const unsigned char* src_ptr, int src_stride,
                        unsigned char* dst_ptr, int dst_width);
  {
    ScaleRowDown8 = filtering && (dst_width <= kMaxOutputWidth) ?
        ScaleRowDown8Int_C : ScaleRowDown8_C;
  }
  for (y = 0; y < dst_height; ++y) {
    ScaleRowDown8(src_ptr, src_stride, dst_ptr, dst_width);
    src_ptr += (src_stride << 3);
    dst_ptr += dst_stride;
  }
}

/**
 * Scale plane down, 3/4
 *
 * Provided by Frank Barchard (fbarchard@google.com)
 *
 */
static void ScalePlaneDown34(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const unsigned char* src_ptr, unsigned char* dst_ptr,
                             enum FilterMode filtering) {
  
  assert(dst_width % 3 == 0);
  void (*ScaleRowDown34_0)(const unsigned char* src_ptr, int src_stride,
                           unsigned char* dst_ptr, int dst_width);
  void (*ScaleRowDown34_1)(const unsigned char* src_ptr, int src_stride,
                           unsigned char* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN34_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      (dst_width % 24 == 0)) {
    if (!filtering) {
      ScaleRowDown34_0 = ScaleRowDown34_NEON;
      ScaleRowDown34_1 = ScaleRowDown34_NEON;
    } else {
      ScaleRowDown34_0 = ScaleRowDown34_0_Int_NEON;
      ScaleRowDown34_1 = ScaleRowDown34_1_Int_NEON;
    }
  } else
#endif
  {
    if (!filtering) {
      ScaleRowDown34_0 = ScaleRowDown34_C;
      ScaleRowDown34_1 = ScaleRowDown34_C;
    } else {
      ScaleRowDown34_0 = ScaleRowDown34_0_Int_C;
      ScaleRowDown34_1 = ScaleRowDown34_1_Int_C;
    }
  }
  int src_row = 0;
  int y;
  for (y = 0; y < dst_height; ++y) {
    switch (src_row) {
      case 0:
        ScaleRowDown34_0(src_ptr, src_stride, dst_ptr, dst_width);
        break;

      case 1:
        ScaleRowDown34_1(src_ptr, src_stride, dst_ptr, dst_width);
        break;

      case 2:
        ScaleRowDown34_0(src_ptr + src_stride, -src_stride,
                         dst_ptr, dst_width);
        break;
    }
    ++src_row;
    src_ptr += src_stride;
    dst_ptr += dst_stride;
    if (src_row >= 3) {
      src_ptr += src_stride;
      src_row = 0;
    }
  }
}

/**
 * Scale plane, 3/8
 *
 * This is an optimized version for scaling down a plane to 3/8
 * of its original size.
 *
 * Reduces 16x3 to 6x1
 */
static void ScalePlaneDown38(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const unsigned char* src_ptr, unsigned char* dst_ptr,
                             enum FilterMode filtering) {
  assert(dst_width % 3 == 0);
  void (*ScaleRowDown38_3)(const unsigned char* src_ptr, int src_stride,
                           unsigned char* dst_ptr, int dst_width);
  void (*ScaleRowDown38_2)(const unsigned char* src_ptr, int src_stride,
                           unsigned char* dst_ptr, int dst_width);
#if defined(HAS_SCALEROWDOWN38_NEON)
  if (TestCpuFlag(kCpuHasNEON) &&
      (dst_width % 12 == 0)) {
    if (!filtering) {
      ScaleRowDown38_3 = ScaleRowDown38_NEON;
      ScaleRowDown38_2 = ScaleRowDown38_NEON;
    } else {
      ScaleRowDown38_3 = ScaleRowDown38_3_Int_NEON;
      ScaleRowDown38_2 = ScaleRowDown38_2_Int_NEON;
    }
  } else
#endif
  {
    if (!filtering) {
      ScaleRowDown38_3 = ScaleRowDown38_C;
      ScaleRowDown38_2 = ScaleRowDown38_C;
    } else {
      ScaleRowDown38_3 = ScaleRowDown38_3_Int_C;
      ScaleRowDown38_2 = ScaleRowDown38_2_Int_C;
    }
  }
  int src_row = 0;
  int y;
  for (y = 0; y < dst_height; ++y) {
    switch (src_row) {
      case 0:
      case 1:
        ScaleRowDown38_3(src_ptr, src_stride, dst_ptr, dst_width);
        src_ptr += src_stride * 3;
        ++src_row;
        break;

      case 2:
        ScaleRowDown38_2(src_ptr, src_stride, dst_ptr, dst_width);
        src_ptr += src_stride * 2;
        src_row = 0;
        break;
    }
    dst_ptr += dst_stride;
  }
}

inline static unsigned int SumBox(int iboxwidth, int iboxheight,
                            int src_stride, const unsigned char* src_ptr) {
  assert(iboxwidth > 0);
  assert(iboxheight > 0);
  unsigned int sum = 0u;
  int y, x;
  for (y = 0; y < iboxheight; ++y) {
    for (x = 0; x < iboxwidth; ++x) {
      sum += src_ptr[x];
    }
    src_ptr += src_stride;
  }
  return sum;
}

static void ScalePlaneBoxRow(int dst_width, int boxheight,
                             int dx, int src_stride,
                             const unsigned char* src_ptr, unsigned char* dst_ptr) {
  int x = 0, i, ix, boxwidth;
  for (i = 0; i < dst_width; ++i) {
    ix = x >> 16;
    x += dx;
    boxwidth = (x >> 16) - ix;
    *dst_ptr++ = SumBox(boxwidth, boxheight, src_stride, src_ptr + ix) /
        (boxwidth * boxheight);
  }
}

inline static unsigned int SumPixels(int iboxwidth, const unsigned short* src_ptr) {
  assert(iboxwidth > 0);
  unsigned int sum = 0u;
  int x;
  for (x = 0; x < iboxwidth; ++x) {
    sum += src_ptr[x];
  }
  return sum;
}

static void ScaleAddCols2_C(int dst_width, int boxheight, int dx,
                            const unsigned short* src_ptr, unsigned char* dst_ptr) {
  int scaletbl[2];
  int minboxwidth = (dx >> 16);
  scaletbl[0] = 65536 / (minboxwidth * boxheight);
  scaletbl[1] = 65536 / ((minboxwidth + 1) * boxheight);
  int *scaleptr = scaletbl - minboxwidth;
  int x = 0, i, ix, boxwidth;
  for (i = 0; i < dst_width; ++i) {
    ix = x >> 16;
    x += dx;
    boxwidth = (x >> 16) - ix;
    *dst_ptr++ = SumPixels(boxwidth, src_ptr + ix) * scaleptr[boxwidth] >> 16;
  }
}

static void ScaleAddCols1_C(int dst_width, int boxheight, int dx,
                            const unsigned short* src_ptr, unsigned char* dst_ptr) {
  int boxwidth = (dx >> 16);
  int scaleval = 65536 / (boxwidth * boxheight);
  int x = 0, i;
  for (i = 0; i < dst_width; ++i) {
    *dst_ptr++ = SumPixels(boxwidth, src_ptr + x) * scaleval >> 16;
    x += boxwidth;
  }
}

/**
 * Scale plane down to any dimensions, with interpolation.
 * (boxfilter).
 *
 * Same method as SimpleScale, which is fixed point, outputting
 * one pixel of destination using fixed point (16.16) to step
 * through source, sampling a box of pixel with simple
 * averaging.
 */
static void ScalePlaneBox(int src_width, int src_height,
                          int dst_width, int dst_height,
                          int src_stride, int dst_stride,
                          const unsigned char* src_ptr, unsigned char* dst_ptr) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  int dy = (src_height << 16) / dst_height;
  int dx = (src_width << 16) / dst_width;
  if ((src_width % 16 != 0) || (src_width > kMaxInputWidth) ||
      dst_height * 2 > src_height) {
    unsigned char* dst = dst_ptr;
    int dy = (src_height << 16) / dst_height;
    int dx = (src_width << 16) / dst_width;
    int y = 0, j;
    for (j = 0; j < dst_height; ++j) {
      int iy = y >> 16;
      const unsigned char* const src = src_ptr + iy * src_stride;
      y += dy;
      if (y > (src_height << 16)) {
        y = (src_height << 16);
      }
      int boxheight = (y >> 16) - iy;
      ScalePlaneBoxRow(dst_width, boxheight,
                       dx, src_stride,
                       src, dst);

      dst += dst_stride;
    }
  } else {
    ALIGN16(unsigned short row[kMaxInputWidth]);
    void (*ScaleAddRows)(const unsigned char* src_ptr, int src_stride,
                         unsigned short* dst_ptr, int src_width, int src_height);
    void (*ScaleAddCols)(int dst_width, int boxheight, int dx,
                         const unsigned short* src_ptr, unsigned char* dst_ptr);
    {
      ScaleAddRows = ScaleAddRows_C;
    }
    if (dx & 0xffff) {
      ScaleAddCols = ScaleAddCols2_C;
    } else {
      ScaleAddCols = ScaleAddCols1_C;
    }

    int y = 0, j;
    for (j = 0; j < dst_height; ++j) {
      int iy = y >> 16;
      const unsigned char* const src = src_ptr + iy * src_stride;
      y += dy;
      if (y > (src_height << 16)) {
        y = (src_height << 16);
      }
      int boxheight = (y >> 16) - iy;
      ScaleAddRows(src, src_stride, row, src_width, boxheight);
      ScaleAddCols(dst_width, boxheight, dx, row, dst_ptr);
      dst_ptr += dst_stride;
    }
  }
}

/**
 * Scale plane to/from any dimensions, with interpolation.
 */
static void ScalePlaneBilinearSimple(int src_width, int src_height,
                                     int dst_width, int dst_height,
                                     int src_stride, int dst_stride,
                                     const unsigned char* src_ptr, unsigned char* dst_ptr) {
  unsigned char* dst = dst_ptr;
  int dx = (src_width << 16) / dst_width;
  int dy = (src_height << 16) / dst_height;
  int maxx = ((src_width - 1) << 16) - 1;
  int maxy = ((src_height - 1) << 16) - 1;
  int y = (dst_height < src_height) ? 32768 :
      (src_height << 16) / dst_height - 32768;
  int i, j;
  for (i = 0; i < dst_height; ++i) {
    int cy = (y < 0) ? 0 : y;
    int yi = cy >> 16;
    int yf = cy & 0xffff;
    const unsigned char* const src = src_ptr + yi * src_stride;
    int x = (dst_width < src_width) ? 32768 :
        (src_width << 16) / dst_width - 32768;
    for (j = 0; j < dst_width; ++j) {
      int cx = (x < 0) ? 0 : x;
      int xi = cx >> 16;
      int xf = cx & 0xffff;
      int r0 = (src[xi] * (65536 - xf) + src[xi + 1] * xf) >> 16;
      int r1 = (src[xi + src_stride] * (65536 - xf) +
          src[xi + src_stride + 1] * xf) >> 16;
      *dst++ = (r0 * (65536 - yf) + r1 * yf) >> 16;
      x += dx;
      if (x > maxx)
        x = maxx;
    }
    dst += dst_stride - dst_width;
    y += dy;
    if (y > maxy)
      y = maxy;
  }
}

/**
 * Scale plane to/from any dimensions, with bilinear
 * interpolation.
 */
static void ScalePlaneBilinear(int src_width, int src_height,
                               int dst_width, int dst_height,
                               int src_stride, int dst_stride,
                               const unsigned char* src_ptr, unsigned char* dst_ptr) {
  assert(dst_width > 0);
  assert(dst_height > 0);
  int dy = (src_height << 16) / dst_height;
  int dx = (src_width << 16) / dst_width;
  if ((src_width % 8 != 0) || (src_width > kMaxInputWidth)) {
    ScalePlaneBilinearSimple(src_width, src_height, dst_width, dst_height,
                             src_stride, dst_stride, src_ptr, dst_ptr);

  } else {
    ALIGN16(unsigned char row[kMaxInputWidth + 1]);
    void (*ScaleFilterRows)(unsigned char* dst_ptr, const unsigned char* src_ptr,
                            int src_stride,
                            int dst_width, int source_y_fraction);
    void (*ScaleFilterCols)(unsigned char* dst_ptr, const unsigned char* src_ptr,
                            int dst_width, int dx);
    {
      ScaleFilterRows = ScaleFilterRows_C;
    }
    ScaleFilterCols = ScaleFilterCols_C;

    int y = 0, j;
    int maxy = ((src_height - 1) << 16) - 1; // max is filter of last 2 rows.
    for (j = 0; j < dst_height; ++j) {
      int iy = y >> 16;
      int fy = (y >> 8) & 255;
      const unsigned char* const src = src_ptr + iy * src_stride;
      ScaleFilterRows(row, src, src_stride, src_width, fy);
      ScaleFilterCols(dst_ptr, row, dst_width, dx);
      dst_ptr += dst_stride;
      y += dy;
      if (y > maxy) {
        y = maxy;
      }
    }
  }
}

/**
 * Scale plane to/from any dimensions, without interpolation.
 * Fixed point math is used for performance: The upper 16 bits
 * of x and dx is the integer part of the source position and
 * the lower 16 bits are the fixed decimal part.
 */
static void ScalePlaneSimple(int src_width, int src_height,
                             int dst_width, int dst_height,
                             int src_stride, int dst_stride,
                             const unsigned char* src_ptr, unsigned char* dst_ptr) {
  unsigned char* dst = dst_ptr;
  int dx = (src_width << 16) / dst_width, y;
  for (y = 0; y < dst_height; ++y) {
    const unsigned char* const src = src_ptr + (y * src_height / dst_height) *
        src_stride;
    // TODO(fbarchard): Round X coordinate by setting x=0x8000.
    int x = 0, i;
    for (i = 0; i < dst_width; ++i) {
      *dst++ = src[x >> 16];
      x += dx;
    }
    dst += dst_stride - dst_width;
  }
}

/**
 * Scale plane to/from any dimensions.
 */
static void ScalePlaneAnySize(int src_width, int src_height,
                              int dst_width, int dst_height,
                              int src_stride, int dst_stride,
                              const unsigned char* src_ptr, unsigned char* dst_ptr,
                              enum FilterMode filtering) {
  if (!filtering) {
    ScalePlaneSimple(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src_ptr, dst_ptr);
  } else {
    // fall back to non-optimized version
    ScalePlaneBilinear(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src_ptr, dst_ptr);
  }
}

/**
 * Scale plane down, any size
 *
 * This is an optimized version for scaling down a plane to any size.
 * The current implementation is ~10 times faster compared to the
 * reference implementation for e.g. XGA->LowResPAL
 *
 */
static void ScalePlaneDown(int src_width, int src_height,
                           int dst_width, int dst_height,
                           int src_stride, int dst_stride,
                           const unsigned char* src_ptr, unsigned char* dst_ptr,
                           enum FilterMode filtering) {
  if (!filtering) {
    ScalePlaneSimple(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src_ptr, dst_ptr);
  } else if (filtering == kFilterBilinear || src_height * 2 > dst_height) {
    // between 1/2x and 1x use bilinear
    ScalePlaneBilinear(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src_ptr, dst_ptr);
  } else {
    ScalePlaneBox(src_width, src_height, dst_width, dst_height,
                  src_stride, dst_stride, src_ptr, dst_ptr);
  }
}

/**
 * Copy plane, no scaling
 *
 * This simply copies the given plane without scaling.
 * The current implementation is ~115 times faster
 * compared to the reference implementation.
 *
 */
static void CopyPlane(int src_width, int src_height,
                      int dst_width, int dst_height,
                      int src_stride, int dst_stride,
                      const unsigned char* src_ptr, unsigned char* dst_ptr) {
  if (src_stride == src_width && dst_stride == dst_width) {
    // All contiguous, so can use REALLY fast path.
    memcpy(dst_ptr, src_ptr, src_width * src_height);
  } else {
    // Not all contiguous; must copy scanlines individually
    const unsigned char* src = src_ptr;
    unsigned char* dst = dst_ptr;
    int i;
    for (i = 0; i < src_height; ++i) {
      memcpy(dst, src, src_width);
      dst += dst_stride;
      src += src_stride;
    }
  }
}

static void ScalePlane(const unsigned char* src, int src_stride,
                       int src_width, int src_height,
                       unsigned char* dst, int dst_stride,
                       int dst_width, int dst_height,
                       enum FilterMode filtering, int use_ref) {
  // Use specialized scales to improve performance for common resolutions.
  // For example, all the 1/2 scalings will use ScalePlaneDown2()
  if (dst_width == src_width && dst_height == src_height) {
    // Straight copy.
    CopyPlane(src_width, src_height, dst_width, dst_height, src_stride,
              dst_stride, src, dst);
  } else if (dst_width <= src_width && dst_height <= src_height) {
    // Scale down.
    if (use_ref) {
      // For testing, allow the optimized versions to be disabled.
      ScalePlaneDown(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src, dst, filtering);
    } else if (4 * dst_width == 3 * src_width &&
               4 * dst_height == 3 * src_height) {
      // optimized, 3/4
      ScalePlaneDown34(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src, dst, filtering);
    } else if (2 * dst_width == src_width && 2 * dst_height == src_height) {
      // optimized, 1/2
      ScalePlaneDown2(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
    // 3/8 rounded up for odd sized chroma height.
    } else if (8 * dst_width == 3 * src_width &&
               dst_height == ((src_height * 3 + 7) / 8)) {
      // optimized, 3/8
      ScalePlaneDown38(src_width, src_height, dst_width, dst_height,
                       src_stride, dst_stride, src, dst, filtering);
    } else if (4 * dst_width == src_width && 4 * dst_height == src_height) {
      // optimized, 1/4
      ScalePlaneDown4(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
    } else if (8 * dst_width == src_width && 8 * dst_height == src_height) {
      // optimized, 1/8
      ScalePlaneDown8(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
    } else {
      // Arbitrary downsample
      ScalePlaneDown(src_width, src_height, dst_width, dst_height,
                     src_stride, dst_stride, src, dst, filtering);
    }
  } else {
    // Arbitrary scale up and/or down.
    ScalePlaneAnySize(src_width, src_height, dst_width, dst_height,
                      src_stride, dst_stride, src, dst, filtering);
  }
}

/**
 * Scale a plane.
 *
 * This function in turn calls a scaling function
 * suitable for handling the desired resolutions.
 *
 */

int I420Scale(const unsigned char* src_y, int src_stride_y,
              const unsigned char* src_u, int src_stride_u,
              const unsigned char* src_v, int src_stride_v,
              int src_width, int src_height,
              unsigned char* dst_y, int dst_stride_y,
              unsigned char* dst_u, int dst_stride_u,
              unsigned char* dst_v, int dst_stride_v,
              int dst_width, int dst_height,
              enum FilterMode filtering) {
  if (!src_y || !src_u || !src_v || src_width <= 0 || src_height == 0 ||
      !dst_y || !dst_u || !dst_v || dst_width <= 0 || dst_height <= 0) {
    return -1;
  }
  // Negative height means invert the image.
  if (src_height < 0) {
    src_height = -src_height;
    int halfheight = (src_height + 1) >> 1;
    src_y = src_y + (src_height - 1) * src_stride_y;
    src_u = src_u + (halfheight - 1) * src_stride_u;
    src_v = src_v + (halfheight - 1) * src_stride_v;
    src_stride_y = -src_stride_y;
    src_stride_u = -src_stride_u;
    src_stride_v = -src_stride_v;
  }
  int halfsrc_width = (src_width + 1) >> 1;
  int halfsrc_height = (src_height + 1) >> 1;
  int halfdst_width = (dst_width + 1) >> 1;
  int halfoheight = (dst_height + 1) >> 1;

  ScalePlane(src_y, src_stride_y, src_width, src_height,
             dst_y, dst_stride_y, dst_width, dst_height,
             filtering, use_reference_impl_);
  ScalePlane(src_u, src_stride_u, halfsrc_width, halfsrc_height,
             dst_u, dst_stride_u, halfdst_width, halfoheight,
             filtering, use_reference_impl_);
  ScalePlane(src_v, src_stride_v, halfsrc_width, halfsrc_height,
             dst_v, dst_stride_v, halfdst_width, halfoheight,
             filtering, use_reference_impl_);
  return 0;
}
