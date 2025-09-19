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

#include "image.h"
#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "zlib.h"

int load_png(char* szName, Image* image) {
  FILE* f;
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  png_infop info_ptr = png_create_info_struct(png_ptr);

  if (!png_ptr || !info_ptr) {
    return 1;
  }

  if ((f = fopen(szName, "rb")) == 0) {
    printf("Error: can't open '%s'\n", szName);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return 1;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    printf("Error: can't load '%s'\n", szName);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(f);
    return 1;
  }

  png_init_io(png_ptr, f);
  png_read_info(png_ptr, info_ptr);
  unsigned int depth = png_get_bit_depth(png_ptr, info_ptr);
  if (depth < 8) {
    if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE) {
      png_set_packing(png_ptr);
    } else {
      png_set_expand(png_ptr);
    }
  } else if (depth > 8) {
    png_set_expand(png_ptr);
    png_set_strip_16(png_ptr);
  }
  (void)png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  unsigned int w = png_get_image_width(png_ptr, info_ptr);
  unsigned int h = png_get_image_height(png_ptr, info_ptr);
  unsigned int bpp = png_get_channels(png_ptr, info_ptr);
  unsigned int type = png_get_color_type(png_ptr, info_ptr);
  image->init(w, h, bpp, type);

  png_colorp palette;
  png_color_16p trans_color;
  png_bytep trans_alpha;

  if (png_get_PLTE(png_ptr, info_ptr, &palette, &image->ps)) {
    memcpy(image->pl, palette, image->ps * 3);
  }

  if (png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &image->ts, &trans_color) && image->ts > 0) {
    if (type == PNG_COLOR_TYPE_GRAY) {
      image->transparency[0] = 0;
      image->transparency[1] = trans_color->gray & 0xFF;
      image->ts = 2;
    } else if (type == PNG_COLOR_TYPE_RGB) {
      image->transparency[0] = 0;
      image->transparency[1] = trans_color->red & 0xFF;
      image->transparency[2] = 0;
      image->transparency[3] = trans_color->green & 0xFF;
      image->transparency[4] = 0;
      image->transparency[5] = trans_color->blue & 0xFF;
      image->ts = 6;
    } else if (type == PNG_COLOR_TYPE_PALETTE) {
      memcpy(image->transparency, trans_alpha, image->ts);
    }
  }

  png_read_image(png_ptr, image->rows);
  png_read_end(png_ptr, info_ptr);
  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  fclose(f);
  return 0;
}

int load_tga(char* szName, Image* image) {
  FILE* f;
  unsigned int i, j, k, n;
  unsigned int w, h;
  unsigned int compressed, top_bottom;
  unsigned char c;
  unsigned char col[4];
  unsigned char header[18];

  if ((f = fopen(szName, "rb")) == 0) {
    printf("Error: can't open '%s'\n", szName);
    return 1;
  }

  if (fread(&header, 1, 18, f) != 18) {
    goto fail;
  }

  w = header[12] + header[13] * 256;
  h = header[14] + header[15] * 256;
  compressed = header[2] & 8;
  top_bottom = header[17] & 0x20;

  if ((header[2] & 7) == 1 && header[16] == 8 && header[1] == 1 && header[7] == 24) {
    image->init(w, h, 1, 3);
  } else if ((header[2] & 7) == 3 && header[16] == 8) {
    image->init(w, h, 1, 0);
  } else if ((header[2] & 7) == 2 && header[16] == 24) {
    image->init(w, h, 3, 2);
  } else if ((header[2] & 7) == 2 && header[16] == 32) {
    image->init(w, h, 4, 6);
  } else {
    goto fail;
  }

  if (header[0] != 0) {
    fseek(f, header[0], SEEK_CUR);
  }

  if (header[1] == 1) {
    unsigned int start = header[3] + header[4] * 256;
    unsigned int size = header[5] + header[6] * 256;
    for (i = start; i < start + size && i < 256; i++) {
      if (fread(&col, 1, 3, f) != 3) {
        goto fail;
      }
      image->pl[i].r = col[2];
      image->pl[i].g = col[1];
      image->pl[i].b = col[0];
    }
    image->ps = i;
    if (start + size > 256) {
      fseek(f, (start + size - 256) * 3, SEEK_CUR);
    }
  }

  for (j = 0; j < h; j++) {
    unsigned char* row = image->rows[(top_bottom) ? j : h - 1 - j];
    if (compressed == 0) {
      if (image->bpp >= 3) {
        for (i = 0; i < w; i++) {
          if (fread(&col, 1, image->bpp, f) != image->bpp) {
            goto fail;
          }
          *row++ = col[2];
          *row++ = col[1];
          *row++ = col[0];
          if (image->bpp == 4) {
            *row++ = col[3];
          }
        }
      } else {
        if (fread(row, 1, w, f) != w) {
          goto fail;
        }
      }
    } else {
      i = 0;
      while (i < w) {
        if (fread(&c, 1, 1, f) != 1) {
          goto fail;
        }
        n = (c & 0x7F) + 1;

        if ((c & 0x80) != 0) {
          if (image->bpp >= 3) {
            if (fread(&col, 1, image->bpp, f) != image->bpp) {
              goto fail;
            }
            for (k = 0; k < n; k++) {
              *row++ = col[2];
              *row++ = col[1];
              *row++ = col[0];
              if (image->bpp == 4) {
                *row++ = col[3];
              }
            }
          } else {
            if (fread(&col, 1, 1, f) != 1) {
              goto fail;
            }
            memset(row, col[0], n);
            row += n;
          }
        } else {
          if (image->bpp >= 3) {
            for (k = 0; k < n; k++) {
              if (fread(&col, 1, image->bpp, f) != image->bpp) {
                goto fail;
              }
              *row++ = col[2];
              *row++ = col[1];
              *row++ = col[0];
              if (image->bpp == 4) {
                *row++ = col[3];
              }
            }
          } else {
            if (fread(row, 1, n, f) != n) {
              goto fail;
            }
            row += n;
          }
        }
        i += n;
      }
    }
  }
  fclose(f);
  return 0;
fail:
  printf("Error: can't load '%s'\n", szName);
  fclose(f);
  return 1;
}

int load_image(char* szName, Image* image) {
  FILE* f;

  if ((f = fopen(szName, "rb")) != 0) {
    unsigned int sign;
    size_t res = fread(&sign, sizeof(sign), 1, f);
    fclose(f);

    if (res == 1) {
      if (sign == 0x474E5089) {
        return load_png(szName, image);
      } else {
        return load_tga(szName, image);
      }
    }
  }

  printf("Error: can't load '%s'\n", szName);
  return 1;
}

unsigned char find_common_coltype(std::vector<Image>& images) {
  unsigned char coltype = images[0].type;

  for (size_t i = 1; i < images.size(); i++) {
    if (images[0].ps != images[i].ps || memcmp(images[0].pl, images[i].pl, images[0].ps * 3) != 0) {
      coltype = 6;
    } else if (images[0].ts != images[i].ts ||
               memcmp(images[0].transparency, images[i].transparency, images[0].ts) != 0) {
      coltype = 6;
    } else if (images[i].type != 3) {
      if (coltype != 3) {
        coltype |= images[i].type;
      } else {
        coltype = 6;
      }
    } else if (coltype != 3) {
      coltype = 6;
    }
  }
  return coltype;
}

void up0to6(Image* image) {
  image->type = 6;
  image->bpp = 4;
  unsigned int x, y;
  unsigned char g, a;
  unsigned char *sp, *dp;
  unsigned int rowbytes = image->w * image->bpp;
  unsigned char* dst = new unsigned char[image->h * rowbytes];

  for (y = 0; y < image->h; y++) {
    sp = image->rows[y];
    image->rows[y] = (y == 0) ? dst : image->rows[y - 1] + rowbytes;
    dp = image->rows[y];
    for (x = 0; x < image->w; x++) {
      g = *sp++;
      a = (image->ts > 0 && image->transparency[1] == g) ? 0 : 255;
      *dp++ = g;
      *dp++ = g;
      *dp++ = g;
      *dp++ = a;
    }
  }
  delete[] image->p;
  image->p = dst;
}

void up2to6(Image* image) {
  image->type = 6;
  image->bpp = 4;
  unsigned int x, y;
  unsigned char r, g, b, a;
  unsigned char *sp, *dp;
  unsigned int rowbytes = image->w * image->bpp;
  unsigned char* dst = new unsigned char[image->h * rowbytes];

  for (y = 0; y < image->h; y++) {
    sp = image->rows[y];
    image->rows[y] = (y == 0) ? dst : image->rows[y - 1] + rowbytes;
    dp = image->rows[y];
    for (x = 0; x < image->w; x++) {
      r = *sp++;
      g = *sp++;
      b = *sp++;
      a = (image->ts > 0 && image->transparency[1] == r && image->transparency[3] == g &&
           image->transparency[5] == b)
              ? 0
              : 255;
      *dp++ = r;
      *dp++ = g;
      *dp++ = b;
      *dp++ = a;
    }
  }
  delete[] image->p;
  image->p = dst;
}

void up3to6(Image* image) {
  image->type = 6;
  image->bpp = 4;
  unsigned int x, y;
  unsigned char *sp, *dp;
  unsigned int rowbytes = image->w * image->bpp;
  unsigned char* dst = new unsigned char[image->h * rowbytes];

  for (y = 0; y < image->h; y++) {
    sp = image->rows[y];
    image->rows[y] = (y == 0) ? dst : image->rows[y - 1] + rowbytes;
    dp = image->rows[y];
    for (x = 0; x < image->w; x++) {
      *dp++ = image->pl[*sp].r;
      *dp++ = image->pl[*sp].g;
      *dp++ = image->pl[*sp].b;
      *dp++ = image->transparency[*sp++];
    }
  }
  delete[] image->p;
  image->p = dst;
}

void up4to6(Image* image) {
  image->type = 6;
  image->bpp = 4;
  unsigned int x, y;
  unsigned char *sp, *dp;
  unsigned int rowbytes = image->w * image->bpp;
  unsigned char* dst = new unsigned char[image->h * rowbytes];

  for (y = 0; y < image->h; y++) {
    sp = image->rows[y];
    image->rows[y] = (y == 0) ? dst : image->rows[y - 1] + rowbytes;
    dp = image->rows[y];
    for (x = 0; x < image->w; x++) {
      *dp++ = *sp;
      *dp++ = *sp;
      *dp++ = *sp++;
      *dp++ = *sp++;
    }
  }
  delete[] image->p;
  image->p = dst;
}

void up0to4(Image* image) {
  image->type = 4;
  image->bpp = 2;
  unsigned int x, y;
  unsigned char *sp, *dp;
  unsigned int rowbytes = image->w * image->bpp;
  unsigned char* dst = new unsigned char[image->h * rowbytes];

  for (y = 0; y < image->h; y++) {
    sp = image->rows[y];
    image->rows[y] = (y == 0) ? dst : image->rows[y - 1] + rowbytes;
    dp = image->rows[y];
    for (x = 0; x < image->w; x++) {
      *dp++ = *sp++;
      *dp++ = 255;
    }
  }
  delete[] image->p;
  image->p = dst;
}

void up0to2(Image* image) {
  image->type = 2;
  image->bpp = 3;
  unsigned int x, y;
  unsigned char *sp, *dp;
  unsigned int rowbytes = image->w * image->bpp;
  unsigned char* dst = new unsigned char[image->h * rowbytes];

  for (y = 0; y < image->h; y++) {
    sp = image->rows[y];
    image->rows[y] = (y == 0) ? dst : image->rows[y - 1] + rowbytes;
    dp = image->rows[y];
    for (x = 0; x < image->w; x++) {
      *dp++ = *sp;
      *dp++ = *sp;
      *dp++ = *sp++;
    }
  }
  delete[] image->p;
  image->p = dst;
}

void optim_upconvert(Image* image, unsigned char coltype) {
  if (image->type == 0 && coltype == 6) {
    up0to6(image);
  } else if (image->type == 2 && coltype == 6) {
    up2to6(image);
  } else if (image->type == 3 && coltype == 6) {
    up3to6(image);
  } else if (image->type == 4 && coltype == 6) {
    up4to6(image);
  } else if (image->type == 0 && coltype == 4) {
    up0to4(image);
  } else if (image->type == 0 && coltype == 2) {
    up0to2(image);
  }
}

void optim_dirty_transp(Image* image) {
  unsigned int x, y;
  if (image->type == 6) {
    for (y = 0; y < image->h; y++) {
      unsigned char* sp = image->rows[y];
      for (x = 0; x < image->w; x++, sp += 4) {
        if (sp[3] == 0) {
          sp[0] = sp[1] = sp[2] = 0;
        }
      }
    }
  } else if (image->type == 4) {
    for (y = 0; y < image->h; y++) {
      unsigned char* sp = image->rows[y];
      for (x = 0; x < image->w; x++, sp += 2) {
        if (sp[1] == 0) {
          sp[0] = 0;
        }
      }
    }
  }
}

int different(Image* image1, Image* image2) {
  return memcmp(image1->p, image2->p, image1->w * image1->h * image1->bpp);
}

void optim_duplicates(std::vector<Image>& images, unsigned int first) {
  unsigned int i = first;

  while (++i < images.size()) {
    if (different(&images[i - 1], &images[i])) {
      continue;
    }

    i--;
    unsigned int num = images[i].delay_num;
    unsigned int den = images[i].delay_den;

    images[i].free();
    images.erase(images.begin() + i);

    if (images[i].delay_den == den) {
      images[i].delay_num += num;
    } else {
      images[i].delay_num = num = num * images[i].delay_den + den * images[i].delay_num;
      images[i].delay_den = den = den * images[i].delay_den;
      while (num && den) {
        if (num > den) {
          num = num % den;
        } else {
          den = den % num;
        }
      }
      num += den;
      images[i].delay_num /= num;
      images[i].delay_den /= num;
    }
  }
}

typedef struct {
  unsigned int num;
  unsigned char r, g, b, a;
} COLORS;

int cmp_colors(const void* arg1, const void* arg2) {
  if (((COLORS*)arg1)->a != ((COLORS*)arg2)->a) {
    return (int)(((COLORS*)arg1)->a) - (int)(((COLORS*)arg2)->a);
  }
  if (((COLORS*)arg1)->num != ((COLORS*)arg2)->num) {
    return (int)(((COLORS*)arg2)->num) - (int)(((COLORS*)arg1)->num);
  }
  if (((COLORS*)arg1)->r != ((COLORS*)arg2)->r) {
    return (int)(((COLORS*)arg1)->r) - (int)(((COLORS*)arg2)->r);
  }
  if (((COLORS*)arg1)->g != ((COLORS*)arg2)->g) {
    return (int)(((COLORS*)arg1)->g) - (int)(((COLORS*)arg2)->g);
  }
  return (int)(((COLORS*)arg1)->b) - (int)(((COLORS*)arg2)->b);
}

void down6(std::vector<Image>& images) {
  unsigned int i, k, x, y;
  unsigned char* sp;
  unsigned char* dp;
  unsigned char r, g, b, a;
  int simple_transp = 1;
  int full_transp = 0;
  int grayscale = 1;
  unsigned char cube[4096];
  unsigned char gray[256];
  COLORS col[256];
  unsigned int colors = 0;
  Image* image = &images[0];

  memset(&cube, 0, sizeof(cube));
  memset(&gray, 0, sizeof(gray));

  for (i = 0; i < 256; i++) {
    col[i].num = 0;
    col[i].r = col[i].g = col[i].b = i;
    col[i].a = image->transparency[i] = 255;
  }

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        a = *sp++;

        if (a != 0) {
          if (a != 255) {
            simple_transp = 0;
          } else if (((r | g | b) & 15) == 0) {
            cube[(r << 4) + g + (b >> 4)] = 1;
          }

          if (r != g || g != b) {
            grayscale = 0;
          } else {
            gray[r] = 1;
          }
        } else {
          full_transp = 1;
        }

        if (colors <= 256) {
          int found = 0;
          for (k = 0; k < colors; k++)
            if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a) {
              found = 1;
              col[k].num++;
              break;
            }
          if (found == 0) {
            if (colors < 256) {
              col[colors].num++;
              col[colors].r = r;
              col[colors].g = g;
              col[colors].b = b;
              col[colors].a = a;
            }
            colors++;
          }
        }
      }
    }
  }

  if (colors <= 256) {
    if (grayscale && simple_transp && colors > 128) /* 6 -> 0 */
    {
      image->type = 0;
      image->bpp = 1;
      unsigned char t = 0;

      for (i = 0; i < 256; i++)
        if (gray[i] == 0) {
          image->transparency[0] = 0;
          image->transparency[1] = t = i;
          image->ts = 2;
          break;
        }

      for (i = 0; i < images.size(); i++) {
        for (y = 0; y < image->h; y++) {
          sp = dp = images[i].rows[y];
          for (x = 0; x < image->w; x++, sp += 4) {
            *dp++ = (sp[3] == 0) ? t : sp[0];
          }
        }
      }
    } else /* 6 -> 3 */
    {
      image->type = 3;
      image->bpp = 1;

      if (full_transp == 0 && colors < 256) {
        col[colors++].a = 0;
      }

      qsort(&col[0], colors, sizeof(COLORS), cmp_colors);

      for (i = 0; i < images.size(); i++) {
        for (y = 0; y < image->h; y++) {
          sp = dp = images[i].rows[y];
          for (x = 0; x < image->w; x++) {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            a = *sp++;
            for (k = 0; k < colors; k++) {
              if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a) {
                break;
              }
            }
            *dp++ = k;
          }
        }
      }

      image->ps = colors;
      for (i = 0; i < colors; i++) {
        image->pl[i].r = col[i].r;
        image->pl[i].g = col[i].g;
        image->pl[i].b = col[i].b;
        image->transparency[i] = col[i].a;
        if (image->transparency[i] != 255) {
          image->ts = i + 1;
        }
      }
    }
  } else if (grayscale) /* 6 -> 4 */
  {
    image->type = 4;
    image->bpp = 2;
    for (i = 0; i < images.size(); i++) {
      for (y = 0; y < image->h; y++) {
        sp = dp = images[i].rows[y];
        for (x = 0; x < image->w; x++, sp += 4) {
          *dp++ = sp[2];
          *dp++ = sp[3];
        }
      }
    }
  } else if (simple_transp) /* 6 -> 2 */
  {
    for (i = 0; i < 4096; i++)
      if (cube[i] == 0) {
        image->transparency[0] = 0;
        image->transparency[1] = (i >> 4) & 0xF0;
        image->transparency[2] = 0;
        image->transparency[3] = i & 0xF0;
        image->transparency[4] = 0;
        image->transparency[5] = (i << 4) & 0xF0;
        image->ts = 6;
        break;
      }
    if (image->ts != 0) {
      image->type = 2;
      image->bpp = 3;
      for (i = 0; i < images.size(); i++) {
        for (y = 0; y < image->h; y++) {
          sp = dp = images[i].rows[y];
          for (x = 0; x < image->w; x++) {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            a = *sp++;
            if (a == 0) {
              *dp++ = image->transparency[1];
              *dp++ = image->transparency[3];
              *dp++ = image->transparency[5];
            } else {
              *dp++ = r;
              *dp++ = g;
              *dp++ = b;
            }
          }
        }
      }
    }
  }
}

void down2(std::vector<Image>& images) {
  unsigned int i, k, x, y;
  unsigned char* sp;
  unsigned char* dp;
  unsigned char r, g, b, a;
  int full_transp = 0;
  int grayscale = 1;
  unsigned char gray[256];
  COLORS col[256];
  unsigned int colors = 0;
  Image* image = &images[0];

  memset(&gray, 0, sizeof(gray));

  for (i = 0; i < 256; i++) {
    col[i].num = 0;
    col[i].r = col[i].g = col[i].b = i;
    col[i].a = 255;
  }

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        a = (image->ts > 0 && image->transparency[1] == r && image->transparency[3] == g &&
             image->transparency[5] == b)
                ? 0
                : 255;

        if (a != 0) {
          if (r != g || g != b) {
            grayscale = 0;
          } else {
            gray[r] = 1;
          }
        } else {
          full_transp = 1;
        }

        if (colors <= 256) {
          int found = 0;
          for (k = 0; k < colors; k++) {
            if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a) {
              found = 1;
              col[k].num++;
              break;
            }
          }
          if (found == 0) {
            if (colors < 256) {
              col[colors].num++;
              col[colors].r = r;
              col[colors].g = g;
              col[colors].b = b;
              col[colors].a = a;
            }
            colors++;
          }
        }
      }
    }
  }

  if (colors <= 256) {
    if (grayscale && colors > 128) /* 2 -> 0 */
    {
      image->type = 0;
      image->bpp = 1;
      unsigned char t = 0;
      int ts = 0;

      for (i = 0; i < 256; i++) {
        if (gray[i] == 0) {
          t = i;
          ts = 2;
          break;
        }
      }
      for (i = 0; i < images.size(); i++) {
        for (y = 0; y < image->h; y++) {
          sp = dp = images[i].rows[y];
          for (x = 0; x < image->w; x++, sp += 3) {
            *dp++ = (image->ts > 0 && image->transparency[1] == sp[0] &&
                     image->transparency[3] == sp[1] && image->transparency[5] == sp[2])
                        ? t
                        : sp[0];
          }
        }
      }
      if (ts > 0) {
        image->transparency[0] = 0;
        image->transparency[1] = t;
        image->ts = ts;
      }
    } else /* 2 -> 3 */
    {
      image->type = 3;
      image->bpp = 1;

      if (full_transp == 0 && colors < 256) {
        col[colors++].a = 0;
      }

      qsort(&col[0], colors, sizeof(COLORS), cmp_colors);

      for (i = 0; i < images.size(); i++) {
        for (y = 0; y < image->h; y++) {
          sp = dp = images[i].rows[y];
          for (x = 0; x < image->w; x++) {
            r = *sp++;
            g = *sp++;
            b = *sp++;
            a = (image->ts > 0 && image->transparency[1] == r && image->transparency[3] == g &&
                 image->transparency[5] == b)
                    ? 0
                    : 255;
            for (k = 0; k < colors; k++) {
              if (col[k].r == r && col[k].g == g && col[k].b == b && col[k].a == a) {
                break;
              }
            }
            *dp++ = k;
          }
        }
      }

      image->ps = colors;
      for (i = 0; i < colors; i++) {
        image->pl[i].r = col[i].r;
        image->pl[i].g = col[i].g;
        image->pl[i].b = col[i].b;
        image->transparency[i] = col[i].a;
        if (image->transparency[i] != 255) {
          image->ts = i + 1;
        }
      }
    }
  }
}

void down4(std::vector<Image>& images) {
  unsigned int i, k, x, y;
  unsigned char* sp;
  unsigned char* dp;
  unsigned char g, a;
  int simple_transp = 1;
  int full_transp = 0;
  unsigned char gray[256];
  COLORS col[256];
  unsigned int colors = 0;
  Image* image = &images[0];

  memset(&gray, 0, sizeof(gray));

  for (i = 0; i < 256; i++) {
    col[i].num = 0;
    col[i].r = col[i].g = col[i].b = i;
    col[i].a = image->transparency[i] = 255;
  }

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        g = *sp++;
        a = *sp++;

        if (a != 0) {
          if (a != 255) {
            simple_transp = 0;
          } else {
            gray[g] = 1;
          }
        } else {
          full_transp = 1;
        }

        if (colors <= 256) {
          int found = 0;
          for (k = 0; k < colors; k++) {
            if (col[k].g == g && col[k].a == a) {
              found = 1;
              col[k].num++;
              break;
            }
          }
          if (found == 0) {
            if (colors < 256) {
              col[colors].num++;
              col[colors].r = g;
              col[colors].g = g;
              col[colors].b = g;
              col[colors].a = a;
            }
            colors++;
          }
        }
      }
    }
  }

  if (simple_transp && colors <= 256) /* 4 -> 0 */
  {
    image->type = 0;
    image->bpp = 1;
    unsigned char t = 0;

    for (i = 0; i < 256; i++)
      if (gray[i] == 0) {
        image->transparency[0] = 0;
        image->transparency[1] = t = i;
        image->ts = 2;
        break;
      }

    for (i = 0; i < images.size(); i++) {
      for (y = 0; y < image->h; y++) {
        sp = dp = images[i].rows[y];
        for (x = 0; x < image->w; x++, sp += 2) {
          *dp++ = (sp[1] == 0) ? t : sp[0];
        }
      }
    }
  } else if (colors <= 256) /* 4 -> 3 */
  {
    image->type = 3;
    image->bpp = 1;

    if (full_transp == 0 && colors < 256) {
      col[colors++].a = 0;
    }

    qsort(&col[0], colors, sizeof(COLORS), cmp_colors);

    for (i = 0; i < images.size(); i++) {
      for (y = 0; y < image->h; y++) {
        sp = dp = images[i].rows[y];
        for (x = 0; x < image->w; x++) {
          g = *sp++;
          a = *sp++;
          for (k = 0; k < colors; k++) {
            if (col[k].g == g && col[k].a == a) {
              break;
            }
          }
          *dp++ = k;
        }
      }
    }

    image->ps = colors;
    for (i = 0; i < colors; i++) {
      image->pl[i].r = col[i].r;
      image->pl[i].g = col[i].g;
      image->pl[i].b = col[i].b;
      image->transparency[i] = col[i].a;
      if (image->transparency[i] != 255) {
        image->ts = i + 1;
      }
    }
  }
}

void down3(std::vector<Image>& images) {
  unsigned int i, x, y;
  unsigned char* sp;
  unsigned char* dp;
  int c;
  int simple_transp = 1;
  int grayscale = 1;
  unsigned char gray[256];
  COLORS col[256];
  Image* image = &images[0];

  memset(&gray, 0, sizeof(gray));

  for (c = 0; c < 256; c++) {
    col[c].num = 0;
    if (c < image->ps) {
      col[c].r = image->pl[c].r;
      col[c].g = image->pl[c].g;
      col[c].b = image->pl[c].b;
      col[c].a = image->transparency[c];
    } else {
      col[c].r = col[c].g = col[c].b = c;
      col[c].a = 255;
    }
  }

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        col[*sp++].num++;
      }
    }
  }

  for (i = 0; i < 256; i++)
    if (col[i].num != 0) {
      if (col[i].a != 0) {
        if (col[i].a != 255) {
          simple_transp = 0;
        } else if (col[i].r != col[i].g || col[i].g != col[i].b) {
          grayscale = 0;
        } else {
          gray[col[i].g] = 1;
        }
      }
    }

  if (grayscale && simple_transp) /* 3 -> 0 */
  {
    image->type = 0;
    image->bpp = 1;
    unsigned char t = 0;
    int ts = 0;

    for (i = 0; i < 256; i++) {
      if (gray[i] == 0) {
        t = i;
        ts = 2;
        break;
      }
    }

    for (i = 0; i < images.size(); i++) {
      for (y = 0; y < image->h; y++) {
        dp = images[i].rows[y];
        for (x = 0; x < image->w; x++, dp++) {
          *dp = (col[*dp].a == 0) ? t : image->pl[*dp].g;
        }
      }
    }
    image->ps = 0;
    image->ts = 0;
    if (ts > 0) {
      image->transparency[0] = 0;
      image->transparency[1] = t;
      image->ts = ts;
    }
  }
}

void optim_downconvert(std::vector<Image>& images) {
  if (images[0].type == 6) {
    down6(images);
  } else if (images[0].type == 2) {
    down2(images);
  } else if (images[0].type == 4) {
    down4(images);
  } else if (images[0].type == 3) {
    down3(images);
  }
}

void optim_palette(std::vector<Image>& images) {
  unsigned int i, x, y;
  unsigned char* sp;
  unsigned char r, g, b, a;
  int c;
  int full_transp = 0;
  COLORS col[256];
  Image* image = &images[0];

  for (c = 0; c < 256; c++) {
    col[c].num = 0;
    if (c < image->ps) {
      col[c].r = image->pl[c].r;
      col[c].g = image->pl[c].g;
      col[c].b = image->pl[c].b;
      col[c].a = image->transparency[c];
    } else {
      col[c].r = col[c].g = col[c].b = c;
      col[c].a = 255;
    }
  }

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        col[*sp++].num++;
      }
    }
  }

  for (i = 0; i < 256; i++) {
    if (col[i].num != 0 && col[i].a == 0) {
      full_transp = 1;
      break;
    }
  }

  for (i = 0; i < 256; i++) {
    if (col[i].num == 0) {
      col[i].a = 255;
      if (full_transp == 0) {
        col[i].a = 0;
        full_transp = 1;
      }
    }
  }

  qsort(&col[0], 256, sizeof(COLORS), cmp_colors);

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        r = image->pl[*sp].r;
        g = image->pl[*sp].g;
        b = image->pl[*sp].b;
        a = image->transparency[*sp];
        for (c = 0; c < image->ps; c++) {
          if (col[c].r == r && col[c].g == g && col[c].b == b && col[c].a == a) {
            break;
          }
        }
        *sp++ = c;
      }
    }
  }

  for (i = 0; i < 256; i++) {
    image->pl[i].r = col[i].r;
    image->pl[i].g = col[i].g;
    image->pl[i].b = col[i].b;
    image->transparency[i] = col[i].a;
    if (col[i].num != 0) {
      image->ps = i + 1;
    }
    if (image->transparency[i] != 255) {
      image->ts = i + 1;
    }
  }
}

void add_transp2(std::vector<Image>& images) {
  unsigned int i, x, y;
  unsigned char* sp;
  unsigned char r, g, b;
  unsigned char cube[4096];
  Image* image = &images[0];

  memset(&cube, 0, sizeof(cube));

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        r = *sp++;
        g = *sp++;
        b = *sp++;
        if (((r | g | b) & 15) == 0) {
          cube[(r << 4) + g + (b >> 4)] = 1;
        }
      }
    }
  }

  for (i = 0; i < 4096; i++) {
    if (cube[i] == 0) {
      image->transparency[0] = 0;
      image->transparency[1] = (i >> 4) & 0xF0;
      image->transparency[2] = 0;
      image->transparency[3] = i & 0xF0;
      image->transparency[4] = 0;
      image->transparency[5] = (i << 4) & 0xF0;
      image->ts = 6;
      break;
    }
  }
}

void add_transp0(std::vector<Image>& images) {
  unsigned int i, x, y;
  unsigned char* sp;
  unsigned char gray[256];
  Image* image = &images[0];

  memset(&gray, 0, sizeof(gray));

  for (i = 0; i < images.size(); i++) {
    for (y = 0; y < image->h; y++) {
      sp = images[i].rows[y];
      for (x = 0; x < image->w; x++) {
        gray[*sp++] = 1;
      }
    }
  }

  for (i = 0; i < 256; i++) {
    if (gray[i] == 0) {
      image->transparency[0] = 0;
      image->transparency[1] = i;
      image->ts = 2;
      break;
    }
  }
}

void optim_add_transp(std::vector<Image>& images) {
  if (images[0].ts == 0) {
    if (images[0].type == 2) {
      add_transp2(images);
    } else if (images[0].type == 0) {
      add_transp0(images);
    }
  }
}
