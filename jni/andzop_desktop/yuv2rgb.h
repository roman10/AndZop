/* YUV-> RGB conversion code.
 *
 * Copyright (C) 2008-9 Robin Watts (robin@wss.co.uk) for Pinknoise
 * Productions Ltd.
 *
 * Licensed under the GNU GPL. If you need it under another license, contact
 * me and ask.
 *
 *  This program is free software ; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation ; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY ; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program ; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef YUV2RGB_H

#define YUV2RGB_H

/* Define these to something appropriate in your build */
//typedef unsigned int   unsigned int;
//typedef signed   int   signed int;
//typedef unsigned short unsigned short;
//typedef unsigned char  unsigned char;

extern const unsigned int yuv2rgb565_table[];
//extern const unsigned int yuv2bgr565_table[];

/*void yuv420_2_rgb565(unsigned char  *dst_ptr,
               const unsigned char  *y_ptr,
               const unsigned char  *u_ptr,
               const unsigned char  *v_ptr,
                     signed int   width,
                     signed int   height,
                     signed int   y_span,
                     signed int   uv_span,
                     signed int   dst_span,
               const unsigned int *tables,
                     signed int   dither);

void yuv422_2_rgb565(unsigned char  *dst_ptr,
               const unsigned char  *y_ptr,
               const unsigned char  *u_ptr,
               const unsigned char  *v_ptr,
                     signed int   width,
                     signed int   height,
                     signed int   y_span,
                     signed int   uv_span,
                     signed int   dst_span,
               const unsigned int *tables,
                     signed int   dither);

void yuv444_2_rgb565(unsigned char  *dst_ptr,
               const unsigned char  *y_ptr,
               const unsigned char  *u_ptr,
               const unsigned char  *v_ptr,
                     signed int   width,
                     signed int   height,
                     signed int   y_span,
                     signed int   uv_span,
                     signed int   dst_span,
               const unsigned int *tables,
                     signed int   dither);


void yuv420_2_rgb888(unsigned char  *dst_ptr,

               const unsigned char  *y_ptr,
               const unsigned char  *u_ptr,
               const unsigned char  *v_ptr,
                     signed int   width,
                     signed int   height,
                     signed int   y_span,
                     signed int   uv_span,
                     signed int   dst_span,
               const unsigned int *tables,
                     signed int   dither);

void yuv422_2_rgb888(unsigned char  *dst_ptr,
               const unsigned char  *y_ptr,
               const unsigned char  *u_ptr,
               const unsigned char  *v_ptr,
                     signed int   width,
                     signed int   height,
                     signed int   y_span,
                     signed int   uv_span,
                     signed int   dst_span,
               const unsigned int *tables,
                     signed int   dither);

void yuv444_2_rgb888(unsigned char  *dst_ptr,
               const unsigned char  *y_ptr,
               const unsigned char  *u_ptr,
               const unsigned char  *v_ptr,
                     signed int   width,
                     signed int   height,
                     signed int   y_span,
                     signed int   uv_span,
                     signed int   dst_span,
               const unsigned int *tables,
                     signed int   dither);
*/
extern void yuv420_2_rgb8888(unsigned char  *dst_ptr,
                const unsigned char  *y_ptr,
                const unsigned char  *u_ptr,
                const unsigned char  *v_ptr,
                      signed int   width,
                      signed int   height,
                      signed int   y_span,
                      signed int   uv_span,
                      signed int   dst_span,
                const unsigned int *tables,
                      signed int   dither);
/*
void yuv422_2_rgb8888(unsigned char  *dst_ptr,
                const unsigned char  *y_ptr,
                const unsigned char  *u_ptr,
                const unsigned char  *v_ptr,
                      signed int   width,
                      signed int   height,
                      signed int   y_span,
                      signed int   uv_span,
                      signed int   dst_span,
                const unsigned int *tables,
                      signed int   dither);

void yuv444_2_rgb8888(unsigned char  *dst_ptr,
                const unsigned char  *y_ptr,
                const unsigned char  *u_ptr,
                const unsigned char  *v_ptr,
                      signed int   width,
                      signed int   height,
                      signed int   y_span,
                      signed int   uv_span,
                      signed int   dst_span,
                const unsigned int *tables,
                      signed int   dither);*/

#ifdef __cplusplus
extern "C" {
#endif

extern void  _yuv420_2_rgb8888(unsigned char  *dst_ptr,

        const unsigned char  *y_ptr,
        const unsigned char  *u_ptr,
        const unsigned char  *v_ptr,
              signed int   width,
              signed int   height,
              signed int   y_span,
              signed int   uv_span,
              signed int   dst_span,
        const unsigned int *tables,
              signed int   dither);

#ifdef __cplusplus
}
#endif

#endif /* YUV2RGB_H */
