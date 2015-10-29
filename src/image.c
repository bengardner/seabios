/**
 * Copyright (C) 2015 Wabtec Railway Electronics
 *
 * A basic image manipulation library for converting between different
 * pixel types. Used to draw text on the splash screen.
 *
 * This file may be distributed under the terms of the GNU LGPLv3 license.
 */
#include "bregs.h" // struct bregs
#include "config.h" // CONFIG_*
#include "farptr.h" // FLATPTR_TO_SEG
#include "malloc.h" // free
#include "output.h" // dprintf
#include "romfile.h" // romfile_loadfile
#include "stacks.h" // call16_int
#include "std/vbe.h" // struct vbe_info
#include "string.h" // memset
#include "util.h" // bmp_alloc, etc.
#include "image.h"

#define IMAGE_DEBUG_LEVEL 3

static inline int min_int(int a, int b)
{
    return a < b ? a : b;
}

pixel32_t pixel_get8_g(const uint8_t *src)
{
	return PIXEL32_RGB(src[0], src[0], src[0]);
}

pixel32_t pixel_get24_bgr(const uint8_t *src)
{
	return PIXEL32_RGB(src[2], src[1], src[0]);
}

pixel32_t pixel_get24_rgb(const uint8_t *src)
{
	return PIXEL32_RGB(src[0], src[1], src[2]);
}

pixel32_t pixel_get32_bgra(const uint8_t *src)
{
	return PIXEL32_RGBA(src[2], src[1], src[0], src[3]);
}

pixel32_t pixel_get32_rgba(const uint8_t *src)
{
	return PIXEL32_RGBA(src[0], src[1], src[2], src[3]);
}

void pixel_put_nop(uint8_t *dst, pixel32_t pix)
{
}

/* NOTE: grayscale colors should have r=b=g=grayscale */
void pixel_put8_g8(uint8_t *dst, pixel32_t pix)
{
	dst[0] = pix.c.blue;
}

void pixel_put24_bgr(uint8_t *dst, pixel32_t pix)
{
	dst[0] = pix.c.blue;
	dst[1] = pix.c.green;
	dst[2] = pix.c.red;
}

void pixel_put24_rgb(uint8_t *dst, pixel32_t pix)
{
	dst[0] = pix.c.red;
	dst[1] = pix.c.green;
	dst[2] = pix.c.blue;
}

void pixel_put32_bgra(uint8_t *dst, pixel32_t pix)
{
	dst[0] = pix.c.blue;
	dst[1] = pix.c.green;
	dst[2] = pix.c.red;
	dst[3] = pix.c.alpha;
}

void pixel_put32_rgba(uint8_t *dst, pixel32_t pix)
{
	dst[0] = pix.c.red;
	dst[1] = pix.c.green;
	dst[2] = pix.c.blue;
	dst[3] = pix.c.alpha;
}

static const pixel_ops_t pixel_ops[] =
{
	{ PIXFMT_8_P,            pixel_put_nop,      pixel_get8_g     },  // dummy entry
	{ PIXFMT_8_G8,           pixel_put8_g8,      pixel_get8_g     },
	{ PIXFMT_24_R8_G8_B8,    pixel_put24_rgb,    pixel_get24_rgb  },
	{ PIXFMT_24_B8_G8_R8,    pixel_put24_bgr,    pixel_get24_bgr  },
	{ PIXFMT_32_R8_G8_B8,    pixel_put24_rgb,    pixel_get24_rgb  },
	{ PIXFMT_32_B8_G8_R8,    pixel_put24_bgr,    pixel_get24_bgr  },
	{ PIXFMT_32_R8_G8_B8_A8, pixel_put32_rgba,   pixel_get32_rgba },
	{ PIXFMT_32_B8_G8_R8_A8, pixel_put32_bgra,   pixel_get32_bgra },
};

const pixel_ops_t *pixel_ops_find(uint16_t format)
{
	int idx;
	for (idx = 0; idx < ARRAY_SIZE(pixel_ops); idx++) {
		if (format == pixel_ops[idx].format) {
			return &pixel_ops[idx];
		}
	}
	return NULL;
}


/**
 * Convert a VBE mode info structure to a image_t structure.
 *
 * @return 0=Sucess, < 0: Failure
 */
int image_from_vbe_mode_info(image_t *img, const struct vbe_mode_info *mode_info)
{
	const pixel_ops_t *pf;

	if (!img || !mode_info)
		return -1;
	memset(img, 0, sizeof(*img));
	if ((mode_info->bits_per_pixel != 24) && (mode_info->bits_per_pixel != 32))
	{
		dprintf(IMAGE_DEBUG_LEVEL, "image_from_vbe_mode_info: need 24 or 32 bpp\n");
		return -2;
	}
	if ((mode_info->blue_size != 8) ||
		 (mode_info->green_size != 8) ||
		 (mode_info->red_size != 8) ||
		 ((mode_info->alpha_size != 0) && (mode_info->alpha_size != 8)))
	{
		dprintf(IMAGE_DEBUG_LEVEL, "image_from_vbe_mode_info: need 8-bit colors\n");
		return -2;
	}

	/* We have a 24 or 32 bit pixel with 8-bit colors.
	 * Figure out the order and whether an alpha channel is present.
	 */
	int fmt = PIXEL_VALID | PIXEL_RGB | (mode_info->bits_per_pixel >> 3);
	if ((mode_info->red_pos == 0) &&
		 (mode_info->green_pos == 8) &&
		 (mode_info->blue_pos == 16)) {
		fmt |= PIXEL_ORDER_RGB;
	} else if ((mode_info->blue_pos == 0) &&
				  (mode_info->green_pos == 8) &&
				  (mode_info->red_pos == 16)) {
		fmt |= PIXEL_ORDER_BGR;
	}
	if (mode_info->alpha_size == 8) {
		fmt |= PIXEL_ALPHA;
	}

	pf = pixel_ops_find(fmt);
	if (!pf) {
		dprintf(IMAGE_DEBUG_LEVEL,
				  "image_from_vbe_mode_info: unsupported format: %d bpp rgba %d/%d,%d/%d,%d/%d,%d/%d\n",
				  mode_info->bits_per_pixel,
				  mode_info->red_size, mode_info->red_pos,
				  mode_info->green_size, mode_info->green_pos,
				  mode_info->blue_size, mode_info->blue_pos,
				  mode_info->alpha_size, mode_info->alpha_pos);
		return -3;
	}
	img->pf     = *pf;
	img->width  = mode_info->xres;
	img->height = mode_info->yres;
	img->pitch  = mode_info->bytes_per_scanline;
	img->mem    = (uint8_t *)mode_info->phys_base;
	dprintf(IMAGE_DEBUG_LEVEL,
			  "image_from_vbe_mode_info: created f:0x%x w:%d h:%d p:%d m:%p\n",
			  img->pf.format, img->width, img->height, img->pitch, img->mem);
	return 0;
}


int image_from_image(image_t *img, const image_t *src_img,
							uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	if (img && src_img && src_img->mem &&
		 ((x + w) <= src_img->width) &&
		 ((y + h) <= src_img->height)) {
		img->pf     = src_img->pf;
		img->height = h;
		img->width  = w;
		img->pitch  = src_img->pitch;
		img->mem    = image_pixel_ptr(src_img, x, y);
		return 0;
	}
	return -1;
}


int image_init(image_t *img, uint16_t format, uint16_t width, uint16_t height, uint16_t pitch, uint8_t *data)
{
    if (img && width && height && data) {
        const pixel_ops_t *pf = pixel_ops_find(format);

        memset(img, 0, sizeof(*img));
        if (pf){
            img->pf = *pf;
        } else {
            img->pf.format = format;
            dprintf(2, "image_init: unsupported format 0x%X\n", format);
        }

        img->width  = width;
        img->height = height;
        img->pitch  = pitch;
        img->mem    = data;
        return 0;
    }
    return -1;
}


int image_init_ro(image_t *img, uint16_t format, uint16_t width, uint16_t height, uint16_t pitch, const uint8_t *data)
{
	int rv = image_init(img,format,width,height,pitch, (uint8_t *)data);
	if (rv == 0) {
		img->pf.pixel_put = pixel_put_nop;
	}
	return rv;
}


int image_copy(image_t *dst, const image_t *src)
{
    if (dst && image_valid(src)) {
        dst->pf     = src->pf;
        dst->height = src->height;
        dst->width  = src->width;
        dst->pitch  = src->pitch;
        dst->mem    = src->mem;
        return 0;
    }
    return -1;
}

int image_blit(image_t *dst_img, const image_t *src_img, int flip_src)
{
    if (!dst_img || !dst_img->mem || !src_img || !src_img->mem) {
        return -1;
    }
    int cp_rows = min_int(src_img->height, dst_img->height);
    int cp_cols = min_int(src_img->width, dst_img->width);
    if ((cp_rows < 1) || (cp_cols < 1)) {
        /* nothing to copy */
        return 0;
    }

    int src_psz = image_pixel_size(src_img);
    int dst_psz = image_pixel_size(dst_img);

    dprintf(3, "image_blit: 0x%x %d %d %d %d %p -> 0x%x %d %d %d %d %p (%d)\n",
            src_img->pf.format,
            src_img->width,
            src_img->height,
            src_img->pitch,
            src_psz,
            src_img->mem,

            dst_img->pf.format,
            dst_img->width,
            dst_img->height,
            dst_img->pitch,
            dst_psz,
            dst_img->mem,
            flip_src);

    uint8_t *src_row = image_pixel_ptr(src_img, 0, flip_src ? (src_img->height - 1) : 0);
    uint8_t *dst_row = image_pixel_ptr(dst_img, 0, 0);
    int     src_rowp = flip_src ? -src_img->pitch : src_img->pitch;

    /* if we have the exact same pixel format, we can use memcpy() */
    if (src_img->pf.format == dst_img->pf.format) {
        int cp_bytes = cp_cols * src_psz;
        dprintf(3, "image_blit: using memcpy(%d)\n", cp_bytes);
        while (cp_rows-- > 0) {
            memcpy(dst_row, src_row, cp_bytes);
            src_row += src_rowp;
            dst_row += dst_img->pitch;
        }
    }
    else
    {
        /* need to copy & convert pixel by pixel */
        dprintf(3, "image_blit: using get(%d/%d)/put(%d/%d) %d/%d\n",
                src_psz, src_rowp,
                dst_psz, dst_img->pitch,
                cp_rows, cp_cols);

        while (cp_rows-- > 0) {
            uint8_t *src_ptr = src_row;
            uint8_t *dst_ptr = dst_row;
            int     cols     = cp_cols;
            pixel32_t pix;

            while (cols-- > 0) {
                pix = src_img->pf.pixel_get(src_ptr);
                dst_img->pf.pixel_put(dst_ptr, pix);
                dst_ptr += dst_psz;
                src_ptr += src_psz;
            }
            src_row += src_rowp;
            dst_row += dst_img->pitch;
        }
    }
    return 0;
}

static const uint8_t *font_get_char(font_t *font, uint8_t ch)
{
    if (font->get_char)
        return font->get_char(font, ch);
    if ((ch > font->max_char) || (ch < font->min_char))
        ch = font->def_char;
    return &font->img.mem[(ch - font->min_char) * font->delta];
}

//static const uint8_t *font8x8x1_get_char(font_t *font, uint8_t ch)
//{
//    return &font->img.mem[((ch < 128) ? ch : 0) * 8];
//}

int font_get_8x8x1(font_t *font)
{
    extern const uint8_t font8x8x1[];
    if (font &&
        (image_init_ro(&font->img, PIXFMT_8_P, 8, 128 * 8, 1, (uint8_t *)font8x8x1) == 0))
    {
        font->height   = 8;
        font->width    = 8;
        font->min_char = 0;
        font->max_char = 128;
        font->def_char = 0;
        font->delta    = 8;
        font->get_char = NULL; //font8x8x1_get_char;
        return 0;
    }
    return -1;
}

//const uint8_t *font8x16x1_get_char(font_t *font, uint8_t ch)
//{
//    return &font->img.mem[ch * 16];
//}

int font_get_8x16x1(font_t *font)
{
    extern const uint8_t font8x16x1[];
    if (font &&
        (image_init_ro(&font->img, PIXFMT_8_P, 8, 256 * 16, 1, (uint8_t *)font8x16x1) == 0))
    {
        font->height   = 16;
        font->width    = 8;
        font->min_char = 0;
        font->max_char = 255;
        font->def_char = 0;
        font->delta    = 16;
        font->get_char = NULL; //font8x16x1_get_char;
        return 0;
    }
    return -1;
}


int font_copy(font_t *dst, const font_t *src)
{
    if (dst && font_valid(src)) {
        image_copy(&dst->img, &src->img);
        dst->height   = src->height;
        dst->width    = src->width;
        dst->delta    = src->delta;
        dst->min_char = src->min_char;
        dst->max_char = src->max_char;
        dst->def_char = src->def_char;
        dst->get_char = src->get_char;
        return 0;
    }
    return -1;
}


int textbox_in_image(textbox_t *tb, const font_t *font, const image_t *img,
                     uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height)
{
    /* validate input */
    if (tb && image_valid(img) && font_valid(font)) {
        memset(tb, 0, sizeof(*tb));
        if (image_from_image(&tb->img, img, x, y, width, height) == 0) {
            tb->col_cnt = width / font->width;
            tb->row_cnt = height / font->height;
            tb->c.bg    = PIXEL32_WHITE;
            tb->c.fg    = PIXEL32_BLACK;
            font_copy(&tb->font, font);
            return 0;
        }
    }
    return -1;
}

void textbox_ctx_save(const textbox_t *tb, textbox_ctx_t *ctx)
{
    if (tb && ctx) {
        *ctx = tb->c;
    }
}

void textbox_ctx_restore(textbox_t *tb, const textbox_ctx_t *ctx)
{
    if (tb && ctx) {
        tb->c = *ctx;
    }
}

void textbox_draw_char_at(textbox_t *tb, uint16_t row, uint16_t col, uint8_t ch)
{
   if ((col < tb->col_cnt) && (row < tb->row_cnt)) {
      image_t       *img      = &tb->img;
      font_t        *font     = &tb->font;
      uint8_t       *img_rowp = image_pixel_ptr(img, col * font->width, row * font->height);
      const uint8_t *fnt_rowp = font_get_char(font, ch);
      int           img_psz   = image_pixel_size(img);
      int           fnt_psz   = image_pixel_size(&font->img);
      int           frows     = font->height;

      /* TODO: handle grayscale formats, colorize using fg/bg */

      if (tb->font.img.pf.format == PIXFMT_8_P) {
          /* special handling for packed pixel format */
          while (frows-- > 0) {
              uint8_t fontbits = *fnt_rowp;
              uint8_t *img_pix = img_rowp;
              uint8_t mask;

              for (mask = 0x80; mask > 0; mask >>= 1) {
                  if (fontbits & mask) {
                      img->pf.pixel_put(img_pix, tb->c.fg);
                  } else if (tb->c.bg.c.alpha) {
                      img->pf.pixel_put(img_pix, tb->c.bg);
                  }
                  img_pix += img_psz;
              }
              fnt_rowp += 1;
              img_rowp += img->pitch;
          }
      } else if (tb->font.img.pf.format == tb->img.pf.format) {
          /* special handling for same pixel format */
          int cp_bytes = tb->font.width * img_psz;
          while (frows-- > 0) {
              memcpy(img_rowp, fnt_rowp, cp_bytes);
              fnt_rowp += font->img.pitch;
              img_rowp += tb->img.pitch;
          }
      } else {
          /* use get/put pixel */
          while (frows-- > 0) {
              const uint8_t *fnt_pix = fnt_rowp;
              uint8_t       *img_pix = img_rowp;
              int           cols     = tb->font.width;

              while (cols-- > 0) {
                  img->pf.pixel_put(img_pix, font->img.pf.pixel_get(fnt_pix));
                  img_pix += img_psz;
                  fnt_pix += fnt_psz;
              }
              fnt_rowp += font->img.pitch;
              img_rowp += img->pitch;
          }
      }
   }
}

void textbox_draw_char(textbox_t *tb, uint8_t ch)
{
    if (textbox_valid(tb)) {
        if (ch == '\r') {
            tb->c.col = 0;
        } else if (ch == '\n') {
            tb->c.col = 0;
            tb->c.row++;
        } else {
           textbox_draw_char_at(tb, tb->c.row, tb->c.col, ch);
           tb->c.col++;
        }
    }
}

void textbox_draw_text(textbox_t *tb, const char *text)
{
    if (textbox_valid(tb) && text) {
        while (*text != 0) {
            textbox_draw_char(tb, *text);
            text++;
        }
    }
}

int textbox_clear_lines(textbox_t *tb, uint16_t row, uint16_t row_cnt)
{
    if (textbox_valid(tb) && (row < tb->row_cnt)) {
        if ((row + row_cnt) > tb->row_cnt) {
            row_cnt = tb->row_cnt - row;
        }
        uint8_t *row_ptr = image_pixel_ptr(&tb->img, 0, row * tb->font.height);
        int     rows     = tb->font.height * row_cnt;
        int     psz      = image_pixel_size(&tb->img);

        while (rows-- > 0) {
            uint8_t *pix_ptr = row_ptr;
            int     cols = tb->img.width;

            while (cols-- > 0) {
                tb->img.pf.pixel_put(pix_ptr, tb->c.bg);
                pix_ptr += psz;
            }
            row_ptr += tb->img.pitch;
        }
        return 0;
    }
    return -1;
}

void textbox_clear(textbox_t *tb)
{
    if (textbox_clear_lines(tb, 0, tb->row_cnt) == 0) {
        tb->c.col = 0;
        tb->c.row = 0;
    }
}
