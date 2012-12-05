/*
 *  Copyright (c) 2011 The LibYuv project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef INCLUDE_LIBYUV_SCALE_H_
#define INCLUDE_LIBYUV_SCALE_H_

// Supported filtering
enum FilterMode {
  kFilterNone = 0,  // Point sample; Fastest
  kFilterBilinear = 1,  // Faster than box, but lower quality scaling down.
  kFilterBox = 2  // Highest quality
};

// Scales a YUV 4:2:0 image from the src width and height to the
// dst width and height.
// If filtering is kFilterNone, a simple nearest-neighbor algorithm is
// used. This produces basic (blocky) quality at the fastest speed.
// If filtering is kFilterBilinear, interpolation is used to produce a better
// quality image, at the expense of speed.
// If filtering is kFilterBox, averaging is used to produce ever better
// quality image, at further expense of speed.
// Returns 0 if successful.

int I420Scale(const unsigned char* src_y, int src_stride_y,
              const unsigned char* src_u, int src_stride_u,
              const unsigned char* src_v, int src_stride_v,
              int src_width, int src_height,
              unsigned char* dst_y, int dst_stride_y,
              unsigned char* dst_u, int dst_stride_u,
              unsigned char* dst_v, int dst_stride_v,
              int dst_width, int dst_height,
              enum FilterMode filtering);

// For testing, allow disabling of optimizations.
void SetUseReferenceImpl(int use);

#endif // INCLUDE_LIBYUV_SCALE_H_
