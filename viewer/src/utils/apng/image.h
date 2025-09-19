/* Image library
 *
 * Copyright (c) 2016 Max Stepin
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

#ifndef IMAGE_H
#define IMAGE_H
#include <string.h>
#include <vector>

struct rgb {
  unsigned char r, g, b;
};

struct Image {
  typedef unsigned char* ROW;
  unsigned int w, h, bpp, type;
  int ps, ts;
  rgb pl[256];
  unsigned char transparency[256];
  unsigned int delay_num, delay_den;
  unsigned char* p;
  ROW* rows;
  Image() : w(0), h(0), bpp(0), type(0), ps(0), ts(0), delay_num(1), delay_den(10), p(0), rows(0) {
    memset(pl, 255, sizeof(pl));
    memset(transparency, 255, sizeof(transparency));
  }
  ~Image() {
  }
  void init(unsigned int w1, unsigned int h1, unsigned int bpp1, unsigned int type1) {
    w = w1;
    h = h1;
    bpp = bpp1;
    type = type1;
    int rowbytes = w * bpp;
    delete[] rows;
    delete[] p;
    rows = new ROW[h];
    rows[0] = p = new unsigned char[h * rowbytes];
    for (unsigned int j = 1; j < h; j++) {
      rows[j] = rows[j - 1] + rowbytes;
    }
  }
  void init(unsigned int w, unsigned int h, Image* image) {
    init(w, h, image->bpp, image->type);
    if ((ps = image->ps) != 0) {
      memcpy(&pl[0], &image->pl[0], ps * 3);
    }
    if ((ts = image->ts) != 0) {
      memcpy(&transparency[0], &image->transparency[0], ts);
    }
  }
  void init(Image* image) {
    init(image->w, image->h, image);
  }
  void free() {
    delete[] rows;
    delete[] p;
  }
};

int load_image(char* szName, Image* image);
unsigned char find_common_coltype(std::vector<Image>& images);
void optim_upconvert(Image* image, unsigned char coltype);
void optim_duplicates(std::vector<Image>& images, unsigned int first);
void optim_dirty_transp(Image* image);
void optim_downconvert(std::vector<Image>& images);
void optim_palette(std::vector<Image>& images);
void optim_add_transp(std::vector<Image>& images);

#endif /* IMAGE_H */
