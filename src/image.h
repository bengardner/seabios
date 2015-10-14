/**
 * Copyright (C) 2015 Wabtec Railway Electronics
 *
 * A basic image manipulation library for converting between different
 * pixel types. Used to draw text on the splash screen.
 *
 * This file may be distributed under the terms of the GNU LGPLv3 license.
 */
#ifndef IMAGE_H_INCLUDED
#define IMAGE_H_INCLUDED

#include <stdint.h>
#include "std/vbe.h" // struct vbe_mode_info


/**
 * The 32-bit pixel format
 */
typedef struct
{
   uint8_t blue;
   uint8_t green;
   uint8_t red;
   uint8_t alpha;
} pixel32_rgba_t;


/**
 * A pixel union to ease converting between the RGB/RGBA and 24/32-bit pixels.
 */
typedef union
{
   pixel32_rgba_t c;
   uint32_t       p;
} pixel32_t;


/* macros to ease creating a pixel */
#define COLOR_RGBA(_r,_g,_b,_a)     ((((_a) & 0xff) << 24) | (((_r) & 0xff) << 16) | (((_g) & 0xff) << 8) | ((_b) & 0xff))
#define COLOR_RGB(_r,_g,_b)         COLOR_RGBA((_r), (_g), (_b), 255)
#define COLOR_BLACK                 COLOR_RGB(0, 0, 0)
#define COLOR_WHITE                 COLOR_RGB(255, 255, 255)
#define PIXEL32_RGBA(_r,_g,_b,_a)   (pixel32_t){ .p = COLOR_RGBA((_r), (_g), (_b), (_a)) }
#define PIXEL32_RGB(_r,_g,_b)       PIXEL32_RGBA((_r), (_g), (_b), 255)
#define PIXEL32_COLOR(_c)           (pixel32_t){ .p = (_c) }

#define PIXEL32_BLACK               PIXEL32_COLOR(COLOR_BLACK)
#define PIXEL32_WHITE               PIXEL32_COLOR(COLOR_WHITE)

/* flags to build the pixel format field */
#define PIXEL_SIZE_MASK    0x000f
#define PIXEL_RGB          0x0010   /* has RGB components */
#define PIXEL_ALPHA        0x0020   /* has Alpha channel */
#define PIXEL_GRAY         0x0040   /* has Gray channel */
#define PIXEL_PACKED       0x0080   /* each bit contains 8 pixels */
#define PIXEL_ORDER_MASK   0x0f00   /* different RGB order */
#define PIXEL_ORDER_RGB    0x0000   /* R G B */
#define PIXEL_ORDER_BGR    0x0100   /* B G R */
#define PIXEL_REV          0x4000   /* flip reverse byte order (if a color is split between bytes) */
#define PIXEL_VALID        0x8000   /* valid format */

/**
 * Pixel formats:
 *  Byte 0   Byte 1   Byte 2   Byte 3
 * 76543210 76543210 76543210 76543210
 * abcdefgh    -        -        -     - PIXFMT_8_P (8 pixels packed in a byte)
 * aaaaaaaa    -        -        -     - PIXFMT_8_A8
 * gggggggg    -        -        -     - PIXFMT_8_G8
 * gggggggg aaaaaaaa    -        -     - PIXFMT_16_G8_A8
 * gggbbbbb rrrrrggg    -        -     - PIXFMT_16_R5_G6_B5 (reversed: >rrrrrggg gggbbbbb<)
 * bbbbbbbb gggggggg rrrrrrrr    -     - PIXFMT_24_B8_G8_R8    [8/16, 8/8, 8/0, 0/0]
 * rrrrrrrr gggggggg bbbbbbbb    -     - PIXFMT_24_R8_G8_B8    [8/16, 8/8, 8/0, 0/0]
 * bbbbbbbb gggggggg rrrrrrrr aaaaaaaa - PIXFMT_32_R8_G8_B8_A8 [8/16, 8/8, 8/0, 8/24]
 * bbbbbbbb gggggggg rrrrrrrr xxxxxxxx - PIXFMT_32_R8_G8_B8_X8 [8/16, 8/8, 8/0, 0/0]
 */
#define PIXFMT_NONE           0,
#define PIXFMT_8_P            (1 | PIXEL_VALID | PIXEL_PACKED)
#define PIXFMT_8_A8           (1 | PIXEL_VALID | PIXEL_ALPHA)
#define PIXFMT_8_G8           (1 | PIXEL_VALID | PIXEL_GRAY)
#define PIXFMT_16_G8_A8       (2 | PIXEL_VALID | PIXEL_GRAY | PIXEL_ALPHA)
#define PIXFMT_16_R5_G6_B5    (2 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_RGB | PIXEL_REV)
#define PIXFMT_24_R8_G8_B8    (3 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_RGB)
#define PIXFMT_24_B8_G8_R8    (3 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_BGR)
#define PIXFMT_32_R8_G8_B8    (4 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_RGB)
#define PIXFMT_32_B8_G8_R8    (4 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_BGR)
#define PIXFMT_32_R8_G8_B8_A8 (4 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_RGB | PIXEL_ALPHA)
#define PIXFMT_32_B8_G8_R8_A8 (4 | PIXEL_VALID | PIXEL_RGB | PIXEL_ORDER_BGR | PIXEL_ALPHA)

/* combines a format, getter, and putter */
typedef struct pixel_ops_t
{
	uint16_t format; /* see PIXFMT_* */
	void      (*pixel_put)(uint8_t *mem, pixel32_t pix);
	pixel32_t (*pixel_get)(const uint8_t *mem);
} pixel_ops_t;

/* pixel getters */
pixel32_t pixel_get8_g(const uint8_t *src);
pixel32_t pixel_get24_bgr(const uint8_t *src);
pixel32_t pixel_get24_rgb(const uint8_t *src);
pixel32_t pixel_get32_bgra(const uint8_t *src);
pixel32_t pixel_get32_rgba(const uint8_t *src);

/* pixel putters */
void pixel_put8_g8(uint8_t *dst, pixel32_t pix);
void pixel_put24_bgr(uint8_t *dst, pixel32_t pix);
void pixel_put24_rgb(uint8_t *dst, pixel32_t pix);
void pixel_put32_bgra(uint8_t *dst, pixel32_t pix);
void pixel_put32_rgba(uint8_t *dst, pixel32_t pix);

/* look up an ops entry. Not all formats are supported. */
const pixel_ops_t *pixel_ops_find(uint16_t format);

/**
 * A simple wrapper around a framebuffer
 */
typedef struct image_t
{
	pixel_ops_t pf;
	uint16_t    width;  /* width in pixels*/
	uint16_t    height; /* number of scanlines */
	uint16_t    pitch;  /* bytes in a scanline */
	uint8_t     *mem;   /* pointer to the upper-left corner of the image */
} image_t;

static inline void pixel_put(image_t *img, uint8_t *dst, pixel32_t pix)
{
    if (img && img->pf.pixel_put) {
        img->pf.pixel_put(dst, pix);
    }
}

static inline pixel32_t pixel_get(const image_t *img, const uint8_t *src)
{
    if (img && img->pf.pixel_get) {
        return img->pf.pixel_get(src);
    }
    return PIXEL32_BLACK;
}

/**
 * By default, fonts are stored as 256 characters stacked vertically.
 * font_ptr = img.mem + (ch * font_height * img.pitch)
 * An override can be supplied with the get_char() function, which should return
 * the upper left
 */
typedef struct font_t
{
	image_t       img;      // backing image (PIXFMT_8_P, PIXFMT_8_G8, or any RGB)
	uint16_t      width;    // width of font in pixels
	uint16_t      height;   // height of font in pixels
	const uint8_t *(*get_char)(uint8_t ch);
} font_t;


typedef struct textbox_ctx_t
{
    uint16_t  row;
    uint16_t  col;
    pixel32_t fg;           // text color
    pixel32_t bg;           // background color. if bg.c.alpha = 0, don't draw.
} textbox_ctx_t;

typedef struct textbox_t
{
    image_t       img;          // backing image
    font_t        font;
    uint16_t      row_cnt;      // img.height / font_height (16)
    uint16_t      col_cnt;      // img.width  / font_width (8)
    textbox_ctx_t c;
} textbox_t;


static inline int image_valid(const image_t *img)
{
    return img && img->mem && (img->pf.format & PIXEL_VALID) != 0;
}

static inline int font_valid(const font_t *font)
{
    return font && image_valid(&font->img) && font->get_char;
}

static inline int textbox_valid(const textbox_t *tb)
{
    return tb && image_valid(&tb->img);
}

/**
 * Get the pixel size in bytes
 */
static inline int image_pixel_size(const image_t *img)
{
	return img->pf.format & PIXEL_SIZE_MASK;
}


/**
 * Get the pointer to the start of the pixel.
 */
static inline uint8_t *image_pixel_ptr(const image_t *img, uint16_t x, uint16_t y)
{
	return img->mem + (y * img->pitch) + (x * image_pixel_size(img));
}


/**
 * Convert a VBE mode info structure to a image_t structure.
 *
 * @return 0=Sucess, <0: Failure
 */
int image_from_vbe_mode_info(image_t *img, const struct vbe_mode_info *mode_info);


/**
 * Initialize an image structure.
 */
int image_init(image_t *img, uint16_t format, uint16_t width, uint16_t height, uint16_t pitch, uint8_t *data);
int image_init_ro(image_t *img, uint16_t format, uint16_t width, uint16_t height, uint16_t pitch, const uint8_t *data);
int image_copy(image_t *dst, const image_t *src);


/**
 * Create another image that is a subset of the current image.
 * Does a basic check to ensure the new image really is inside the src_img.
 * @return 0:Success, <0:Failure
 */
int image_from_image(image_t *img, const image_t *src_img,
                     uint16_t x, uint16_t y, uint16_t w, uint16_t h);


/**
 * Copy src to dst, flipping the image while copying.
 */
int image_blit(image_t *dst_img, const image_t *src_img, int flip_src);

int font_get_8x16x1(font_t *font);
int font_copy(font_t *dst, const font_t *src);


/**
 * Create a textbox on an image.
 * This directly draws on the image with no memory of the characters drawn.
 */
int textbox_in_image(textbox_t *tb, const font_t *font, const image_t *img,
                     uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height);

void textbox_ctx_save(const textbox_t *tb, textbox_ctx_t *ctx);
void textbox_ctx_restore(textbox_t *tb, const textbox_ctx_t *ctx);

void textbox_draw_char_at(textbox_t *tb, uint16_t row, uint16_t col, uint8_t ch);
void textbox_draw_char(textbox_t *tb, uint8_t ch);
void textbox_draw_text(textbox_t *tb, const char *text);
void textbox_clear(textbox_t *tb);
int textbox_clear_lines(textbox_t *tb, uint16_t row, uint16_t row_cnt);
static inline void textbox_clear_line(textbox_t *tb, uint16_t row)
{
    textbox_clear_lines(tb, row, 1);
}


#endif /* IMAGE_H_INCLUDED */
