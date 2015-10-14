/*
* Basic BMP data process and Raw picture data handle functions.
* Could be used to adjust pixel data format, get information, etc.
*
* Copyright (C) 2011 Wayne Xia <xiawenc@cn.ibm.com>
*
* This work is licensed under the terms of the GNU LGPLv3.
*/
#include "malloc.h" // malloc_tmphigh
#include "string.h" // memcpy
#include "util.h" // struct bmp_decdata

struct bmp_decdata {
    struct tagRGBQUAD *quadp;
    image_t img;
    unsigned char *datap;
};

#define bmp_load4byte(addr) (*(u32 *)(addr))
#define bmp_load2byte(addr) (*(u16 *)(addr))

typedef struct tagBITMAPFILEHEADER {
    u8 bfType[2];
    u8 bfSize[4];
    u8 bfReserved1[2];
    u8 bfReserved2[2];
    u8 bfOffBits[4];
} BITMAPFILEHEADER, tagBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
    u8 biSize[4];
    u8 biWidth[4];
    u8 biHeight[4];
    u8 biPlanes[2];
    u8 biBitCount[2];
    u8 biCompression[4];
    u8 biSizeImage[4];
    u8 biXPelsPerMeter[4];
    u8 biYPelsPerMeter[4];
    u8 biClrUsed[4];
    u8 biClrImportant[4];
} BITMAPINFOHEADER, tagBITMAPINFOHEADER;

typedef struct tagRGBQUAD {
    u8 rgbBlue;
    u8 rgbGreen;
    u8 rgbRed;
    u8 rgbReserved;
} RGBQUAD, tagRGBQUAD;


/* allocate decdata struct */
struct bmp_decdata *bmp_alloc(void)
{
    struct bmp_decdata *bmp = malloc_tmphigh(sizeof(*bmp));
    return bmp;
}

/* extract information from bmp file data */
int bmp_decode(struct bmp_decdata *bmp, uint8_t *data, int data_size)
{
    if (data_size < 54)
        return 1;

    u16 bmp_filehead = bmp_load2byte(data + 0);
    if (bmp_filehead != 0x4d42)
        return 2;
    u32 bmp_recordsize = bmp_load4byte(data + 2);
    if (bmp_recordsize != data_size)
        return 3;
    u32 bmp_dataoffset = bmp_load4byte(data + 10);

    int width  = bmp_load4byte(data + 18);
    int height = bmp_load4byte(data + 22);
    int bpp    = bmp_load2byte(data + 28);

    if (bpp != 24) {
        /* only support 24 bit BGR */
        return 4;
    }

    if (image_init(&bmp->img, PIXFMT_24_B8_G8_R8, width, height, width * 3,
                   data + bmp_dataoffset) != 0) {
        return 5;
    }
    return 0;
}

/* get bmp properties */
void bmp_get_size(struct bmp_decdata *bmp, int *width, int *height, int *bpp)
{
    *width  = bmp->img.width;
    *height = bmp->img.height;
    *bpp    = image_pixel_size(&bmp->img) * 8;
}


/* extract the bitmap to an image surface */
int bmp_copy_to_image(struct bmp_decdata *bmp, image_t *dst_img)
{
    return image_blit(dst_img, &bmp->img, 1);
}
