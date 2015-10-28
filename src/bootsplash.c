// Initialize the VGA console and possibly show a boot splash image.
//
// Copyright (C) 2009-2010  coresystems GmbH
// Copyright (C) 2010  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "bregs.h" // struct bregs
#include "config.h" // CONFIG_*
#include "farptr.h" // FLATPTR_TO_SEG
#include "malloc.h" // free
#include "output.h" // dprintf
#include "romfile.h" // romfile_loadfile
#include "stacks.h" // call16_int
#include "std/vbe.h" // struct vbe_info
#include "string.h" // memset
#include "util.h" // enable_bootsplash
#include "hw/pci.h"
#include "hw/pci_ids.h"
#include "hw/pci_regs.h"
#include "image.h"
#include "std/vbe.h" // VBE_CAPABILITY_8BIT_DAC
#include "std/smbios.h"
#include "hw/wabtec-cpu1900.h"

extern u32 CpuKHz VARFSEG;
extern u64 FirstTimestamp VARFSEG;
extern u64 BootTimestamp VARFSEG;

/****************************************************************
 * Helper functions
 ****************************************************************/

// Call int10 vga handler.
static void
call16_int10(struct bregs *br)
{
    br->flags = F_IF;
    start_preempt();
    call16_int(0x10, br);
    finish_preempt();
}


/****************************************************************
 * VGA text / graphics console
 ****************************************************************/

void
enable_vga_console(void)
{
    dprintf(1, "Turning on vga text mode console\n");
    struct bregs br;

    /* Enable VGA text mode */
    memset(&br, 0, sizeof(br));
    br.ax = 0x0003;
    call16_int10(&br);

    // Write to screen.
    printf("SeaBIOS (version %s)\n", VERSION);
}

static int
find_videomode(struct vbe_info *vesa_info, struct vbe_mode_info *mode_info
               , int width, int height, int bpp_req)
{
    dprintf(3, "Finding vesa mode with dimensions %d x %d (%d bpp)\n",
            width, height, bpp_req);
    u16 *videomodes = SEGOFF_TO_FLATPTR(vesa_info->video_mode);
    for (;; videomodes++) {
        u16 videomode = *videomodes;
        if (videomode == 0xffff) {
            dprintf(1, "Unable to find vesa video mode with dimensions %d x %d (%d bpp)\n",
                    width, height, bpp_req);
            return -1;
        }
        struct bregs br;
        memset(&br, 0, sizeof(br));
        br.ax = 0x4f01;
        br.cx = videomode;
        br.di = FLATPTR_TO_OFFSET(mode_info);
        br.es = FLATPTR_TO_SEG(mode_info);
        call16_int10(&br);
        if (br.ax != 0x4f) {
            dprintf(3, "get_mode failed asking for mode %x.\n", videomode);
            continue;
        }
        if (mode_info->xres != width
            || mode_info->yres != height)
            continue;
        u8 depth = mode_info->bits_per_pixel;
        if (bpp_req == 0) {
            if (depth != 16 && depth != 24 && depth != 32)
                continue;
        } else {
            if (depth < bpp_req)
                continue;
        }
        return videomode;
    }
}

static int BootsplashActive;

//#if CONFIG_BOOTSPLASH_DYNAMIC_TEXT
/*
 * Convert a timestamp counter value to milliseconds-since-boot, based on
 * the CPU clock frequency already calculated.
 */
static u32 GetTimestampMilliseconds(u64 tsc) {
    u32 khz = CpuKHz;

    while (tsc > 0xFFFFFFFFULL) {
        tsc >>= 1;
        khz >>= 1;
    }
    if (khz != 0) {
        return(u32) tsc / (u32) khz;
    }
    return 1;   // tell user this only took one millisecond (avoids further
                // divide-by-zero issues)
}

static textbox_t g_textbox;


#define MSR_IA32_THERM_STATUS       0x0000019c
#define MSR_IA32_TEMPERATURE_TARGET 0x000001a2

static int coretemp_read(void)
{
	int tjmax, temp;

	tjmax = (rdmsr(MSR_IA32_TEMPERATURE_TARGET) >> 16) & 0xff;
	temp  = tjmax - ((rdmsr(MSR_IA32_THERM_STATUS) >> 16) & 0x7f);

	return temp;
}

/**
 * Read the I210 MAC from the RAL0/RAH0 registers.
 * @return 0=OK, 1=Not initialized, -1=error (shouldn't happen!)
 */
static int i210_get_mac(struct pci_device *pci, uint8_t mac[6])
{
    // Enable memory access.
    u16 old_val = pci_config_readw(pci->bdf, PCI_COMMAND);
    pci_config_maskw(pci->bdf, PCI_COMMAND, 0, PCI_COMMAND_MEMORY);

    // get HW address
    u8 *hwaddr = (void *)(pci_config_readl(pci->bdf, PCI_BASE_ADDRESS_0) & PCI_BASE_ADDRESS_MEM_MASK);
    u32 ral0 = readl(hwaddr + 0x5400);
    u32 rah0 = readl(hwaddr + 0x5404);

    // restore old access
    pci_config_writew(pci->bdf, PCI_COMMAND, old_val);

    if (rah0 != 0xffffffff) {
        /* check valid bit */
        int rv = ((rah0 & (1 << 31)) != 0) ? 0 : 1;
        mac[0] = ral0 & 0xff;
        mac[1] = (ral0 >> 8) & 0xff;
        mac[2] = (ral0 >> 16) & 0xff;
        mac[3] = (ral0 >> 24) & 0xff;
        mac[4] = rah0 & 0xff;
        mac[5] = (rah0 >> 8) & 0xff;
        return rv;
    }
    return -1;
}

static const char *cpu1900_get_reset_cause(void)
{
    static const char *reset_cause_text[8] = {
        "[0] Cold Boot",
        "[1] Watchdog Reset",
        "[2] Backplane Sleep",
        "[3] Power Failure",
        "[4] Software Reset",
        "[5] Button",
        "[6] Timeout",
        "[7] Invalid"
    };
    return reset_cause_text[inb(CPU1900_REG_RESET_CAUSE) & CPU1900_REG_RESET_CAUSE__MASK];
}

static void print_bios_info(void)
{
    const struct smbios_type_0 *tbl_0 = smbios_get_table(0, sizeof(*tbl_0));
    if (tbl_0) {
        const char *str_arr = ((const char *)tbl_0) + sizeof(struct smbios_type_0);
        bs_printf("Coreboot:   %s, %s [%s]\n",
                  smbios_str_get(str_arr, tbl_0->vendor_str),
                  smbios_str_get(str_arr, tbl_0->bios_version_str),
                  smbios_str_get(str_arr, tbl_0->bios_release_date_str));
    }

    const struct smbios_type_1 *tbl_1 = smbios_get_table(1, sizeof(*tbl_1));
    if (tbl_1) {
        const char *str_arr = (const char *)&tbl_1[1];
        if (tbl_1->product_name_str) {
            bs_printf("Product:    %s\n", smbios_str_get(str_arr, tbl_1->product_name_str));
        }
        if (tbl_1->serial_number_str) {
            bs_printf("Serial:     %s\n", smbios_str_get(str_arr, tbl_1->serial_number_str));
        }
        if (tbl_1->sku_number_str) {
            bs_printf("SKU:        %s\n", smbios_str_get(str_arr, tbl_1->sku_number_str));
        }
    }

    bs_printf("RAM:        %d MB\n", estimateRamSize_MB());
    bs_printf("Slot ID:    %d\n", (inb(CPU1900_REG_SLOTID) & CPU1900_REG_SLOTID__ID));

    /* log what is in the PCIe slots (8086:0f48=BP Eth, 8086:0f4c=ExpSlot, 8086:0f4e=FP Eth)

        PCI: BDF:e0 root:0 8086:0f48 class:0604
        PCI: BDF:e2 root:0 8086:0f4c class:0604
        PCI: BDF:e3 root:0 8086:0f4e class:0604

     */
    struct pci_device *pci_exp = NULL;
    struct pci_device *pci;
    uint8_t mac[6];
    foreachpci(pci) {
        if (pci->parent)
        {
            if ((pci->class == PCI_CLASS_NETWORK_ETHERNET) &&
                (pci->vendor == 0x8086))
            {
                /* assuming an i210, since that is what we have */
                int rv = i210_get_mac(pci, mac);
                if (rv >= 0) {
                    bs_printf("%s Eth:  %04x:%04x MAC=%02x%02x.%02x%02x.%02x%02x%s%s\n",
                              (pci->parent->device == 0x0f48) ? "Front" : "Back ",
                              pci->vendor, pci->device,
                              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                              (rv != 0) ? " [Invalid]" : "",
                              (pci->device != 0x157b) ? " [Not Initialzed]" : "");
                }
            } else {
                pci_exp = pci;
            }
        }
    }

    // log what is in the PCIe slot
    if (pci_exp) {
        bs_printf("Expansion:  %04x:%04x\n", pci_exp->vendor, pci_exp->device);
    } else {
        bs_printf("Expansion:  None\n");
    }
    bs_printf("FPGA Info:  Rev:%d.%d HW:0x%02x Opt:0x%02x\n",
              inb(CPU1900_REG_FPGA_MAJOR_REV),
              inb(CPU1900_REG_FPGA_MINOR_REV),
              inb(CPU1900_REG_HW_REV),
              inb(CPU1900_REG_FPGA_OPTIONS));
    bs_printf("Last Reset: %s\n", cpu1900_get_reset_cause());
    //TODO: CFast Size/detected?
    //TODO: MMC size/detected?

    bs_printf("Core Temp: %d deg C\n", coretemp_read());

    //bs_print("\nPlatform initialization completed in ");
    //u32 boot_time_ms = GetTimestampMilliseconds(FirstTimestamp);
    //u32 boot_time_sec = boot_time_ms/1000;
    //boot_time_ms -= (boot_time_sec * 1000);
    //bs_printf("%u.%03u seconds.\n", boot_time_sec, boot_time_ms);
}

static void
bootsplash_enable_dynamic_text(struct vbe_mode_info *mode_info)
{
    image_t img;
    font_t  font;

    /* TODO: make textbox size/location configurable? */
    if ((image_from_vbe_mode_info(&img, mode_info) != 0) ||
        (font_get_8x16x1(&font) != 0) ||
        (textbox_in_image(&g_textbox, &font, &img,
                          font.width * 2,
                          img.height / 2,
                          img.width - (4 * font.width),
                          (img.height / 2)) != 0))
        return;
}
//#endif // #if CONFIG_BOOTSPLASH_DYNAMIC_TEXT


void bs_print(const char *text)
{
    if (BootsplashActive)
        textbox_draw_text(&g_textbox, text);
    dprintf(1, "%s", text);
}

void bs_printf(const char *fmt, ...)
{
    char    *str = NULL;
    va_list args;

    va_start(args, fmt);
    vasprintf(&str, fmt, args);
    va_end(args);

    if (str) {
        bs_print(str);
        free(str);
    }
}

/* The status line is the last line in the textbox */
void bs_status_print(const char *text)
{
    if (BootsplashActive) {
        /* show on both the framebuffer and console */
        textbox_ctx_t ctx;
        textbox_ctx_save(&g_textbox, &ctx);
        g_textbox.c.row = g_textbox.row_cnt - 1;
        g_textbox.c.col = 0; // TODO: center line?
        textbox_clear_line(&g_textbox, g_textbox.c.row);
        textbox_draw_text(&g_textbox, text);
        textbox_ctx_restore(&g_textbox, &ctx);
    }
    dprintf(1, "\n%s\n", text);
}

void bs_wait_loop(u32 tick_left)
{
    if (BootsplashActive && textbox_valid(&g_textbox)) {
        static u32 last_tick, last_sec;

        if (tick_left != last_tick) {
            last_tick = tick_left;
            if (tick_left == 0) {
                textbox_clear_line(&g_textbox, g_textbox.row_cnt - 2);
            } else {
                u32 sec_left = (ticks_to_ms(tick_left) + 500) / 1000;
                if (sec_left != last_sec) {
                    last_sec = sec_left;
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Remaining: %2d sec ", sec_left);
                    textbox_ctx_t ctx;
                    textbox_ctx_save(&g_textbox, &ctx);
                    g_textbox.c.row = g_textbox.row_cnt - 2;
                    g_textbox.c.col = 0; // TODO: center line?
                    g_textbox.c.bg.c.alpha = 255;
                    //textbox_clear_line(&g_textbox, g_textbox.c.row);
                    textbox_draw_text(&g_textbox, buf);
                    textbox_ctx_restore(&g_textbox, &ctx);
                    dprintf(1, "%s\n", buf);
                }
            }
        }
    }
}

void bs_status_printf(const char *fmt, ...)
{
    char    *str = NULL;
    va_list args;

    va_start(args, fmt);
    vasprintf(&str, fmt, args);
    va_end(args);

    if (str) {
        bs_status_print(str);
        free(str);
    }
}


void bs_clear(void)
{
    textbox_clear(&g_textbox);
}


static void
_enable_bootsplash(void)
{
    if (!CONFIG_BOOTSPLASH)
        return;
    /* splash picture can be bmp or jpeg file */
    dprintf(3, "Checking for bootsplash\n");
    u8 type = 0; /* 0 means jpg, 1 means bmp, default is 0=jpg */
    int filesize;
    u8 *filedata = romfile_loadfile("bootsplash.jpg", &filesize);
    if (!filedata) {
        filedata = romfile_loadfile("bootsplash.bmp", &filesize);
        if (!filedata)
            return;
        type = 1;
    }
    dprintf(3, "start showing bootsplash\n");

    u8 *picture = NULL; /* data buff used to be flushed to the video buf */
    struct jpeg_decdata *jpeg = NULL;
    struct bmp_decdata *bmp = NULL;
    struct vbe_info *vesa_info = malloc_tmplow(sizeof(*vesa_info));
    struct vbe_mode_info *mode_info = malloc_tmplow(sizeof(*mode_info));
    if (!vesa_info || !mode_info) {
        warn_noalloc();
        goto done;
    }

    /* Check whether we have a VESA 2.0 compliant BIOS */
    memset(vesa_info, 0, sizeof(struct vbe_info));
    vesa_info->signature = VBE2_SIGNATURE;
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.ax = 0x4f00;
    br.di = FLATPTR_TO_OFFSET(vesa_info);
    br.es = FLATPTR_TO_SEG(vesa_info);
    call16_int10(&br);
    if (vesa_info->signature != VESA_SIGNATURE) {
        dprintf(1,"No VBE2 found.\n");
        goto done;
    }

    /* Print some debugging information about our card. */
    char *vendor = SEGOFF_TO_FLATPTR(vesa_info->oem_vendor_string);
    char *product = SEGOFF_TO_FLATPTR(vesa_info->oem_product_string);
    dprintf(3, "VESA %d.%d\nVENDOR: %s\nPRODUCT: %s\n",
            vesa_info->version>>8, vesa_info->version&0xff,
            vendor, product);

    int ret, width, height, bpp;
    int bpp_require = 0;
    if (type == 0) {
        jpeg = jpeg_alloc();
        if (!jpeg) {
            warn_noalloc();
            goto done;
        }
        /* Parse jpeg and get image size. */
        dprintf(5, "Decoding bootsplash.jpg\n");
        ret = jpeg_decode(jpeg, filedata);
        if (ret) {
            dprintf(1, "jpeg_decode failed with return code %d...\n", ret);
            goto done;
        }
        jpeg_get_size(jpeg, &width, &height);
    } else {
        bmp = bmp_alloc();
        if (!bmp) {
            warn_noalloc();
            goto done;
        }
        /* Parse bmp and get image size. */
        dprintf(5, "Decoding bootsplash.bmp\n");
        ret = bmp_decode(bmp, filedata, filesize);
        if (ret) {
            dprintf(1, "bmp_decode failed with return code %d...\n", ret);
            goto done;
        }
        bmp_get_size(bmp, &width, &height, &bpp);
        bpp_require = 24;
        dprintf(3, "bootsplash.bmp is %d x %d (%d bpp)\n",
                width, height, bpp);
    }
    /* jpeg would use 16 or 24 bpp video mode, BMP use 24bpp mode only */

    // Try to find a graphics mode with the corresponding dimensions.
    int videomode = find_videomode(vesa_info, mode_info, width, height,
                                   bpp_require);
    if (videomode < 0) {
        dprintf(1, "failed to find a videomode with %dx%d %dbpp (0=any).\n",
                width, height, bpp_require);
        goto done;
    }
    void *framebuffer = (void *)mode_info->phys_base;
    int depth = mode_info->bits_per_pixel;
    dprintf(3, "mode: %04x\n", videomode);
    dprintf(3, "framebuffer: %p\n", framebuffer);
    dprintf(3, "bytes per scanline: %d\n", mode_info->bytes_per_scanline);
    dprintf(3, "bits per pixel: %d\n", depth);

    // Allocate space for image and decompress it.
    int imagesize = height * mode_info->bytes_per_scanline;
    picture = malloc_tmphigh(imagesize);
    if (!picture) {
        warn_noalloc();
        goto done;
    }

    /* create a matching image with our own memory */
    image_t img;
    if (image_from_vbe_mode_info(&img, mode_info)) {
        goto done;
    }
    img.mem = picture;

    if (type == 0) {
        dprintf(5, "Decompressing bootsplash.jpg\n");
        ret = jpeg_copy_to_image(jpeg, &img);
        if (ret) {
            dprintf(1, "jpeg_show failed with return code %d...\n", ret);
            goto done;
        }
    } else {
        dprintf(5, "Decompressing bootsplash.bmp\n");
        ret = bmp_copy_to_image(bmp, &img);
        if (ret) {
            dprintf(1, "bmp_copy_to_image failed with return code %d...\n", ret);
            goto done;
        }
    }

    /* Switch to graphics mode */
    dprintf(5, "Switching to graphics mode\n");
    memset(&br, 0, sizeof(br));
    br.ax = 0x4f02;
    br.bx = videomode | VBE_MODE_LINEAR_FRAME_BUFFER;
    call16_int10(&br);
    if (br.ax != 0x4f) {
        dprintf(1, "set_mode failed.\n");
        goto done;
    }

    /* Show the picture */
    dprintf(5, "Showing bootsplash picture\n");
    iomemcpy(framebuffer, picture, imagesize);
    dprintf(5, "Bootsplash copy complete\n");
    BootsplashActive = 1;

    bootsplash_enable_dynamic_text(mode_info);

done:
    free(filedata);
    free(picture);
    free(vesa_info);
    free(mode_info);
    free(jpeg);
    free(bmp);
    return;
}

void enable_bootsplash(const char *menukey_text)
{
    _enable_bootsplash();
    print_bios_info();
    bs_status_printf("Press %s to select a boot device.%s", menukey_text,
                     BootsplashActive ? " Hit F1 to freeze this screen." : "");
}

void
disable_bootsplash(void)
{
    if (!CONFIG_BOOTSPLASH || !BootsplashActive)
        return;
    BootsplashActive = 0;
    enable_vga_console();
}

int get_bootsplash_active(void)
{
    return BootsplashActive;
}

void bootsplash_show_paused(void)
{
    bs_status_print("Screen frozen. Press a key to reboot.");
}
