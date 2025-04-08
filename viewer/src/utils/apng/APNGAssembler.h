/* APNG Assembler 2.91
 *
 * This program creates APNG animation from PNG/TGA image sequence.
 *
 * http://apngasm.sourceforge.net/
 *
 * Copyright (c) 2009-2016 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */

#pragma once

#include <string>
#include "image.h"
#include "zlib.h"

class APNGAssembler {
 public:
  typedef struct {
    Image* image;
    unsigned int size;
    int x, y, w, h, valid, filters;
  } OP;
  APNGAssembler();
  ~APNGAssembler();

  int exportFromPNGSequence(const std::string& outPath, const std::string& firstPNGPath,
                            int frameRate);

 private:
  void write_chunk(FILE* f, const char* name, unsigned char* data, unsigned int length);
  void write_IDATs(FILE* f, unsigned int frame, unsigned char* data, unsigned int length,
                   unsigned int idat_size);
  void deflate_rect_op(Image* image, int x, int y, int w, int h, int bpp, int zbuf_size, int n);
  void deflate_rect_fin(int deflate_method, int iter, unsigned char* zbuf, unsigned int* zsize,
                        int bpp, unsigned char* dest, int zbuf_size, int n);
  void get_rect(unsigned int w, unsigned int h, Image* image1, Image* image2, Image* temp,
                unsigned int bpp, int zbuf_size, unsigned int has_tcolor, unsigned int tcolor,
                int n);
  void process_rect(Image* image, int xbytes, int rowbytes, int y, int h, int bpp,
                    unsigned char* dest);
  int load_image_sequence(char* szImage, unsigned int first, std::vector<Image>& img, int delay_num,
                          int delay_den, unsigned char* coltype);
  int save_apng(const char* szOut, std::vector<Image>& img, unsigned int loops, unsigned int first,
                int deflate_method, int iter);

 private:
  unsigned char* op_zbuf1 = nullptr;
  unsigned char* op_zbuf2 = nullptr;
  unsigned char* row_buf = nullptr;
  unsigned char* sub_row = nullptr;
  unsigned char* up_row = nullptr;
  unsigned char* avg_row = nullptr;
  unsigned char* paeth_row = nullptr;
  unsigned char coltype = 6;
  unsigned char png_sign[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  unsigned char png_Software[28] = {83, 111, 102, 116, 119, 97, 114, 101, '\0', 65, 80, 78, 71, 32,
                                    65, 115, 115, 101, 109, 98, 108, 101, 114,  32, 50, 46, 57, 49};

  unsigned int first = 0;
  unsigned int loops = 0;
  unsigned int next_seq_num = 0;
  int iter = 15;
  int delay_num = 1;
  int delay_den = -1;
  int keep_palette = 0;
  int keep_coltype = 1;
  int deflate_method = 0;
  z_stream op_zstream1{};
  z_stream op_zstream2{};
  OP op[6]{};
  std::vector<Image> img;
};
