// Code to load disk image and start system boot.
//
// Copyright (C) 2008-2013  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "block.h" // struct drive_s
#include "bregs.h" // struct bregs
#include "config.h" // CONFIG_*
#include "fw/paravirt.h" // qemu_cfg_show_boot_menu
#include "hw/pci.h" // pci_bdf_to_*
#include "hw/pcidevice.h" // struct pci_device
#include "hw/rtc.h" // rtc_read
#include "hw/usb.h" // struct usbdevice_s
#include "list.h" // hlist_node
#include "malloc.h" // free
#include "output.h" // dprintf
#include "romfile.h" // romfile_loadint
#include "std/disk.h" // struct mbr_s
#include "string.h" // memset
#include "util.h" // irqtimer_calc
#include "tcgbios.h" // tpm_*
#include "hw/wabtec-cpu1900-io.h"
#include "hw/serialio.h"

// scan codes for get_keystroke()
// these should be elsewhere in a header file.
#define RAWKEY_ESC               0x01
#define RAWKEY_1                 0x02
#define RAWKEY_2                 0x03
#define RAWKEY_3                 0x04
#define RAWKEY_4                 0x05
#define RAWKEY_5                 0x06
#define RAWKEY_6                 0x07
#define RAWKEY_7                 0x08
#define RAWKEY_8                 0x09
#define RAWKEY_9                 0x0a
#define RAWKEY_ENTER             0x1c
#define RAWKEY_F1                0x3B
#define RAWKEY_F2                0x3C
#define RAWKEY_F3                0x3D
#define RAWKEY_F4                0x3E
#define RAWKEY_F5                0x3f
#define RAWKEY_F6                0x40
#define RAWKEY_F7                0x41
#define RAWKEY_F8                0x42
#define RAWKEY_F9                0x43
#define RAWKEY_F10               0x44
#define RAWKEY_F11               0x85
#define RAWKEY_F12               0x86


/**
 * The menukey is encoded such that
 *    b15- 0 : the 16-bit scan code
 *    b63-16 : text chars (LSB first)
 * NOTE: this assumes little-endian, the first char in the text is at
 * the menukey address +2 bytes. The last byte must be 0.
 */
#define MENUKEY_MAKE(_scan, _c1, _c2, _c3, _c4, _c5) \
    (((u64)(_scan) & 0xffff) \
     | (((u64)((_c1) & 0xff)) << 16) \
     | (((u64)((_c2) & 0xff)) << 24) \
     | (((u64)((_c3) & 0xff)) << 32) \
     | (((u64)((_c4) & 0xff)) << 40) \
     | (((u64)((_c5) & 0xff)) << 48))
#define MENUKEY_ESC              MENUKEY_MAKE(RAWKEY_ESC, 'E', 'S', 'C', 0, 0)
#define MENUKEY_F1               MENUKEY_MAKE(RAWKEY_F1,  'F', '1', 0, 0, 0)
#define MENUKEY_F11              MENUKEY_MAKE(RAWKEY_F11, 'F', '1', '1', 0, 0)
#define MENUKEY_F12              MENUKEY_MAKE(RAWKEY_F12, 'F', '1', '2', 0, 0)
#define MENUKEY_TEXT(_m)         (((char *)&(_m)) + 2)
#define MENUKEY_CODE(_m)         ((int)(_m) & 0xffff)


/****************************************************************
 * Boot priority ordering
 ****************************************************************/

static char **Bootorder VARVERIFY32INIT;
static int BootorderCount;

static void
loadBootOrder(void)
{
    if (!CONFIG_BOOTORDER)
        return;

    char *f = romfile_loadfile("bootorder", NULL);
    if (!f)
        return;

    int i = 0;
    BootorderCount = 1;
    while (f[i]) {
        if (f[i] == '\n')
            BootorderCount++;
        i++;
    }
    Bootorder = malloc_tmphigh(BootorderCount*sizeof(char*));
    if (!Bootorder) {
        warn_noalloc();
        free(f);
        BootorderCount = 0;
        return;
    }

    dprintf(1, "boot order:\n");
    i = 0;
    do {
        Bootorder[i] = f;
        f = strchr(f, '\n');
        if (f)
            *(f++) = '\0';
        Bootorder[i] = nullTrailingSpace(Bootorder[i]);
        dprintf(1, "%d: %s\n", i+1, Bootorder[i]);
        i++;
    } while (f);
}

// See if 'str' starts with 'glob' - if glob contains an '*' character
// it will match any number of characters in str that aren't a '/' or
// the next glob character.
static char *
glob_prefix(const char *glob, const char *str)
{
    for (;;) {
        if (!*glob && (!*str || *str == '/'))
            return (char*)str;
        if (*glob == '*') {
            if (!*str || *str == '/' || *str == glob[1])
                glob++;
            else
                str++;
            continue;
        }
        if (*glob != *str)
            return NULL;
        glob++;
        str++;
    }
}

// Search the bootorder list for the given glob pattern.
static int
find_prio(const char *glob)
{
    dprintf(1, "Searching bootorder for: %s\n", glob);
    int i;
    for (i = 0; i < BootorderCount; i++)
        if (glob_prefix(glob, Bootorder[i]) || glob_prefix(Bootorder[i], glob))
            return i+1;
    return -1;
}

#define FW_PCI_DOMAIN "/pci@i0cf8"

static char *
build_pci_path(char *buf, int max, const char *devname, struct pci_device *pci)
{
    // Build the string path of a bdf - for example: /pci@i0cf8/isa@1,2
    char *p = buf;
    if (pci->parent) {
        p = build_pci_path(p, max, "pci-bridge", pci->parent);
    } else {
        p += snprintf(p, buf+max-p, "%s", FW_PCI_DOMAIN);
        if (pci->rootbus)
            p += snprintf(p, buf+max-p, ",%x", pci->rootbus);
    }

    int dev = pci_bdf_to_dev(pci->bdf), fn = pci_bdf_to_fn(pci->bdf);
    p += snprintf(p, buf+max-p, "/%s@%x", devname, dev);
    if (fn)
        p += snprintf(p, buf+max-p, ",%x", fn);
    return p;
}

int bootprio_find_pci_device(struct pci_device *pci)
{
    if (CONFIG_CSM)
        return csm_bootprio_pci(pci);
    if (!CONFIG_BOOTORDER)
        return -1;
    // Find pci device - for example: /pci@i0cf8/ethernet@5
    char desc[256];
    build_pci_path(desc, sizeof(desc), "*", pci);
    return find_prio(desc);
}

int bootprio_find_scsi_device(struct pci_device *pci, int target, int lun)
{
    if (!CONFIG_BOOTORDER)
        return -1;
    if (!pci)
        // support only pci machine for now
        return -1;
    // Find scsi drive - for example: /pci@i0cf8/scsi@5/channel@0/disk@1,0
    char desc[256], *p;
    p = build_pci_path(desc, sizeof(desc), "*", pci);
    snprintf(p, desc+sizeof(desc)-p, "/*@0/*@%x,%x", target, lun);
    return find_prio(desc);
}

int bootprio_find_ata_device(struct pci_device *pci, int chanid, int slave)
{
    if (CONFIG_CSM)
        return csm_bootprio_ata(pci, chanid, slave);
    if (!CONFIG_BOOTORDER)
        return -1;
    if (!pci)
        // support only pci machine for now
        return -1;
    // Find ata drive - for example: /pci@i0cf8/ide@1,1/drive@1/disk@0
    char desc[256], *p;
    p = build_pci_path(desc, sizeof(desc), "*", pci);
    snprintf(p, desc+sizeof(desc)-p, "/drive@%x/disk@%x", chanid, slave);
    return find_prio(desc);
}

int bootprio_find_fdc_device(struct pci_device *pci, int port, int fdid)
{
    if (CONFIG_CSM)
        return csm_bootprio_fdc(pci, port, fdid);
    if (!CONFIG_BOOTORDER)
        return -1;
    if (!pci)
        // support only pci machine for now
        return -1;
    // Find floppy - for example: /pci@i0cf8/isa@1/fdc@03f1/floppy@0
    char desc[256], *p;
    p = build_pci_path(desc, sizeof(desc), "isa", pci);
    snprintf(p, desc+sizeof(desc)-p, "/fdc@%04x/floppy@%x", port, fdid);
    return find_prio(desc);
}

int bootprio_find_pci_rom(struct pci_device *pci, int instance)
{
    if (!CONFIG_BOOTORDER)
        return -1;
    // Find pci rom - for example: /pci@i0cf8/scsi@3:rom2
    char desc[256], *p;
    p = build_pci_path(desc, sizeof(desc), "*", pci);
    if (instance)
        snprintf(p, desc+sizeof(desc)-p, ":rom%x", instance);
    return find_prio(desc);
}

int bootprio_find_named_rom(const char *name, int instance)
{
    if (!CONFIG_BOOTORDER)
        return -1;
    // Find named rom - for example: /rom@genroms/linuxboot.bin
    char desc[256], *p;
    p = desc + snprintf(desc, sizeof(desc), "/rom@%s", name);
    if (instance)
        snprintf(p, desc+sizeof(desc)-p, ":rom%x", instance);
    return find_prio(desc);
}

static char *
build_usb_path(char *buf, int max, struct usbhub_s *hub)
{
    if (!hub->usbdev)
        // Root hub - nothing to add.
        return buf;
    char *p = build_usb_path(buf, max, hub->usbdev->hub);
    p += snprintf(p, buf+max-p, "/hub@%x", hub->usbdev->port+1);
    return p;
}

int bootprio_find_usb(struct usbdevice_s *usbdev, int lun)
{
    if (!CONFIG_BOOTORDER)
        return -1;
    // Find usb - for example: /pci@i0cf8/usb@1,2/storage@1/channel@0/disk@0,0
    char desc[256], *p;
    p = build_pci_path(desc, sizeof(desc), "usb", usbdev->hub->cntl->pci);
    p = build_usb_path(p, desc+sizeof(desc)-p, usbdev->hub);
    snprintf(p, desc+sizeof(desc)-p, "/storage@%x/*@0/*@0,%x"
             , usbdev->port+1, lun);
    int ret = find_prio(desc);
    if (ret >= 0)
        return ret;
    // Try usb-host/redir - for example: /pci@i0cf8/usb@1,2/usb-host@1
    snprintf(p, desc+sizeof(desc)-p, "/usb-*@%x", usbdev->port+1);
    return find_prio(desc);
}


/****************************************************************
 * Boot setup
 ****************************************************************/

static int BootRetryTime;
static int CheckFloppySig = 1;

#define DEFAULT_PRIO           9999

static int DefaultFloppyPrio = 101;
static int DefaultCDPrio     = 102;
static int DefaultHDPrio     = 103;
static int DefaultBEVPrio    = 104;

void
boot_init(void)
{
    if (! CONFIG_BOOT)
        return;

    if (CONFIG_QEMU) {
        // On emulators, get boot order from nvram.
        if (rtc_read(CMOS_BIOS_BOOTFLAG1) & 1)
            CheckFloppySig = 0;
        u32 bootorder = (rtc_read(CMOS_BIOS_BOOTFLAG2)
                         | ((rtc_read(CMOS_BIOS_BOOTFLAG1) & 0xf0) << 4));
        DefaultFloppyPrio = DefaultCDPrio = DefaultHDPrio
            = DefaultBEVPrio = DEFAULT_PRIO;
        int i;
        for (i=101; i<104; i++) {
            u32 val = bootorder & 0x0f;
            bootorder >>= 4;
            switch (val) {
            case 1: DefaultFloppyPrio = i; break;
            case 2: DefaultHDPrio = i;     break;
            case 3: DefaultCDPrio = i;     break;
            case 4: DefaultBEVPrio = i;    break;
            }
        }
    }

    BootRetryTime = romfile_loadint("etc/boot-fail-wait", 60*1000);

    loadBootOrder();
}


/****************************************************************
 * BootList handling
 ****************************************************************/

struct bootentry_s {
    int type;
    union {
        u32 data;
        struct segoff_s vector;
        struct drive_s *drive;
    };
    int priority;
    const char *description;
    struct hlist_node node;
};
static struct hlist_head BootList VARVERIFY32INIT;

#define IPL_TYPE_FLOPPY      0x01
#define IPL_TYPE_HARDDISK    0x02
#define IPL_TYPE_CDROM       0x03
#define IPL_TYPE_CBFS        0x20
#define IPL_TYPE_BEV         0x80
#define IPL_TYPE_BCV         0x81
#define IPL_TYPE_HALT        0xf0

static void
bootentry_add(int type, int prio, u32 data, const char *desc)
{
    if (! CONFIG_BOOT)
        return;
    struct bootentry_s *be = malloc_tmp(sizeof(*be));
    if (!be) {
        warn_noalloc();
        return;
    }
    be->type = type;
    be->priority = prio;
    be->data = data;
    be->description = desc ?: "?";
    dprintf(3, "Registering bootable: %s (type:%d prio:%d data:%x)\n"
            , be->description, type, prio, data);

    // Add entry in sorted order.
    struct hlist_node **pprev;
    struct bootentry_s *pos;
    hlist_for_each_entry_pprev(pos, pprev, &BootList, node) {
        if (be->priority < pos->priority)
            break;
        if (be->priority > pos->priority)
            continue;
        if (be->type < pos->type)
            break;
        if (be->type > pos->type)
            continue;
        if (be->type <= IPL_TYPE_CDROM
            && (be->drive->type < pos->drive->type
                || (be->drive->type == pos->drive->type
                    && be->drive->cntl_id < pos->drive->cntl_id)))
            break;
    }
    hlist_add(&be->node, pprev);
}

// Return the given priority if it's set - defaultprio otherwise.
static inline int defPrio(int priority, int defaultprio) {
    return (priority < 0) ? defaultprio : priority;
}

// Add a BEV vector for a given pnp compatible option rom.
void
boot_add_bev(u16 seg, u16 bev, u16 desc, int prio)
{
    bootentry_add(IPL_TYPE_BEV, defPrio(prio, DefaultBEVPrio)
                  , SEGOFF(seg, bev).segoff
                  , desc ? MAKE_FLATPTR(seg, desc) : "Unknown");
    DefaultBEVPrio = DEFAULT_PRIO;
}

// Add a bcv entry for an expansion card harddrive or legacy option rom
void
boot_add_bcv(u16 seg, u16 ip, u16 desc, int prio)
{
    bootentry_add(IPL_TYPE_BCV, defPrio(prio, DefaultHDPrio)
                  , SEGOFF(seg, ip).segoff
                  , desc ? MAKE_FLATPTR(seg, desc) : "Legacy option rom");
}

void
boot_add_floppy(struct drive_s *drive_g, const char *desc, int prio)
{
    bootentry_add(IPL_TYPE_FLOPPY, defPrio(prio, DefaultFloppyPrio)
                  , (u32)drive_g, desc);
}

void
boot_add_hd(struct drive_s *drive_g, const char *desc, int prio)
{
    bootentry_add(IPL_TYPE_HARDDISK, defPrio(prio, DefaultHDPrio)
                  , (u32)drive_g, desc);
}

void
boot_add_cd(struct drive_s *drive_g, const char *desc, int prio)
{
    bootentry_add(IPL_TYPE_CDROM, defPrio(prio, DefaultCDPrio)
                  , (u32)drive_g, desc);
}

// Add a CBFS payload entry
void
boot_add_cbfs(void *data, const char *desc, int prio)
{
    bootentry_add(IPL_TYPE_CBFS, defPrio(prio, DEFAULT_PRIO), (u32)data, desc);
}


/****************************************************************
 * Keyboard calls
 ****************************************************************/

// See if a keystroke is pending in the keyboard buffer.
static int
check_for_keystroke(void)
{
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF|F_ZF;
    br.ah = 1;
    call16_int(0x16, &br);
    return !(br.flags & F_ZF);
}

// Return a keystroke - waiting forever if necessary.
static int
get_raw_keystroke(void)
{
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF;
    call16_int(0x16, &br);
    return br.ah;
}

/* only care about 1-9 and enter right now */
static int
translate_char_to_keystroke(int val)
{
    /* translate '1'..'9' to 0x02..0x0a */
    if ((val >= '1') && (val <= '9')) {
        return RAWKEY_1 + val - '1';
    }
    if ((val == '\r') || (val == '\n')) {
        return RAWKEY_ENTER;
    }
    return -1;
}

// Read a keystroke - waiting up to 'msec' milliseconds.
// if msec is -1, wait forever for a keystroke
int
get_keystroke(int msec)
{
    u32 end = irqtimer_calc(msec);
    int val;

    for (;;) {
        if (check_for_keystroke())
            return get_raw_keystroke();

        val = translate_char_to_keystroke(serial_debug_getc());
        if (val != -1)
            return val;

        if ((msec != -1) && irqtimer_check(end))
            return -1;

        if (msec == -1) {
            bs_wait_loop(0);
        } else {
            u32 tick_left = TICKS_PER_DAY - ((GET_BDA(timer_counter) + TICKS_PER_DAY - end) % TICKS_PER_DAY);
            bs_wait_loop(tick_left);
        }
        yield_toirq();
    }
}


/****************************************************************
 * Boot menu and BCV execution
 ****************************************************************/

#define DEFAULT_BOOTMENU_WAIT 2500

/**
 * Convert the bootentry_s.description to a boot source type and set
 * BIOS_BOOT_SOURCE.
 * The BootList has already been sorted such that the selected item is first.
 */
static void
bootmenu_update_type(int boot_idx)
{
    struct bootentry_s *pos;
    int val = CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__NONE;

    hlist_for_each_entry(pos, &BootList, node) {
        if (memcmp(pos->description, "USB ", 4) == 0) {
            val = CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__USB;
        } else if (memcmp(pos->description, "AHCI", 4) == 0) {
            val = CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__SATA;
        } else if (memcmp(pos->description, "MMC ", 4) == 0) {
            val = CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__MMC;
        } else {
            val = CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__OTHER;
        }
        break;
    }
    if (boot_idx < 1)
        boot_idx = 1;
    if (boot_idx > CPU1900_REG_BIOS_BOOT_SOURCE__IDX)
        boot_idx = CPU1900_REG_BIOS_BOOT_SOURCE__IDX;
    fpga_write_u8(CPU1900_REG_BIOS_BOOT_SOURCE, val | boot_idx);
}

/**
 * Select a bootmenu item.
 *
 * @param choice menu selection, starting at 1
 */
static void
bootmenu_select(int choice)
{
    struct bootentry_s *pos;

    if (choice > 0) {
        int tmp = choice;
        hlist_for_each_entry(pos, &BootList, node) {
            tmp--;
             if (tmp == 0)
                  break;
        }
        if (tmp == 0) {
            bs_printf("\nChose bootmenu item %d: %s\n\n", choice, pos->description);
            hlist_del(&pos->node);
            pos->priority = 0;
            hlist_add_head(&pos->node, &BootList);
        }
    }
    bootmenu_update_type(choice);
}

//static void
//cpu1900_bootmenu_default(void)
//{
//    int choice = fpga_read_u8(CPU1900_REG_SCRATCH) & 0x0f;
//    bs_printf("CPU1900: scratch=0x%02x\n", choice);
//    bootmenu_select(choice);
//}

/**
 * This handles the auto-select or recovery boot menu selection.
 *
 * The fully-populated boot menu will be something like:
 *   Bootmenu:
 *     1. USB MSC Drive Verbatim STORE N GO 5.00
 *     2. AHCI/0: Delkin Devices BE08TGPZZ-XN000-D ATA-8 Hard-Disk (7
 *     3. MMC drive P1XXXX 3688MiB
 *     4. Payload [memtest]
 *     5. Ramdisk [iPXE]
 *     6. Payload [coreinfo]
 *
 *   Bootmenu:
 *     1. USB MSC Drive CENTON  8.00
 *     2. USB MSC Drive Verbatim STORE N GO 5.00
 *     3. AHCI/0: Delkin Devices BE08TGPZZ-XN000-D ATA-8 Hard-Disk (7
 *     4. MMC drive P1XXXX 3688MiB
 *     5. Payload [recovery]
 *     6. Payload [memtest]
 *     7. Ramdisk [iPXE]
 *     8. Payload [coreinfo]
 *
 * We have 8 bits of NV storage to use.
 *
 * If the Reset Cause is not SW or WD, then boot the default entry.
 * If CPU1900_REG_BIOS_LAST_STAGE >= 0x20, then boot the default entry.
 * We need 2 WD reboots or 4 SW reboots in a row to attempt recovery. (?)
 *
 *
 */
static void
bootmenu_autoselect(void)
{
    u8 last_reset = fpga_read_u8(CPU1900_REG_RESET_CAUSE) & CPU1900_REG_RESET_CAUSE__M;
    u8 last_stage = fpga_read_u8(CPU1900_REG_BIOS_LAST_STAGE);
    u8 last_boots = fpga_read_u8(CPU1900_REG_BIOS_BOOT_SOURCE);
    u8 last_menu  = last_boots & 0x0f;
    u8 bbc        = fpga_read_u8(CPU1900_REG_BIOS_BOOT_COUNT);
    u8 reset_cnt  = bbc & CPU1900_REG_BIOS_BOOT_COUNT__COUNT;
    u8 clear_cnt  = 0;

    bs_printf("RECOVERY: cause=0x%02x stage=0x%02x boots=0x%02x\n",
              last_reset, last_stage, last_boots);

    /* FIXME:
     * We really should have the app set LAST_STAGE to a greater value.
     * FT currently does not do that, so the final value is 0x20 (OS Driver)
     * Factory Test will need to be fixed first.
     * This check should be (last_stage >= CPU1900_BOOT_STAGE_APP_HAPPY).
     */
    if (last_stage < CPU1900_BOOT_STAGE_SB_PAYLOAD) {
        /* we didn't attempt to boot a payload, so there cannot be an issue with the payload */
        bs_printf("RECOVERY: SKIP stage 0x%02x < 0x%02x\n", last_stage, CPU1900_BOOT_STAGE_SB_PAYLOAD);
        last_menu = 1;
        clear_cnt = 1;
    }
    else if (last_stage >= CPU1900_BOOT_STAGE_OS_DRIVER) {
        bs_printf("RECOVERY: CLEAR stage 0x%02x >= 0x%02x\n", last_stage, CPU1900_BOOT_STAGE_OS_DRIVER);
        last_menu = 1;
        clear_cnt = 1;
    }
    else if ((last_reset != CPU1900_REG_RESET_CAUSE__M__SW_RESET) &&
             (last_reset != CPU1900_REG_RESET_CAUSE__M__WD)) {
        bs_printf("RECOVERY: CLEAR not SW or WD\n");

        /* Not a recoverable reset reason */
        last_menu = 1;
        clear_cnt = 1;
    }
    else if (reset_cnt < 3) {
        bs_printf("RECOVERY: WAIT reset_cnt=%d\n", reset_cnt);
    }
    else {
        last_menu++;
        bs_printf("RECOVERY: FAIL, booting %d\n", last_menu);
    }

    /* Clear the boot count
     * This will look odd in the coreboot romstage log, but whatever.
     */
    if (clear_cnt) {
       fpga_write_u8(CPU1900_REG_BIOS_BOOT_COUNT,
                     bbc & ~CPU1900_REG_BIOS_BOOT_COUNT__COUNT);
    }

    bootmenu_select(last_menu);
}

static void cpu1900_bios_happy(void)
{
   /* TEST: Skip setting the Happy bit */
   u8 bct = fpga_read_u8(CPU1900_REG_BIOS_BOOT_COUNT);

   if ((bct & CPU1900_REG_BIOS_BOOT_COUNT__TEST_HAPPY) != 0) {
      dprintf(1, "CPU1900: TEST Happy\n");
   } else {
      dprintf(1, "CPU1900: Set BIOS Happy bit\n");
      fpga_write_u8(CPU1900_REG_BIOS_BOOT,
                    fpga_read_u8(CPU1900_REG_BIOS_BOOT) | CPU1900_REG_BIOS_BOOT__HAPPY);
   }
}

// Show IPL option menu.
void
interactive_bootmenu(void)
{
    u8 boot_idx = 0;

    cpu1900_bios_happy();

    // XXX - show available drives?

    if (!CONFIG_BOOTMENU || !romfile_loadint("etc/show-boot-menu", 1)) {
        bootmenu_autoselect();
        return;
    }

    /* Only show the boot menu if the Watchdog Disable jumper is set */
    if ((fpga_read_u8(CPU1900_REG_DBG) & CPU1900_REG_DBG_MSK) != CPU1900_REG_DBG_VAL) {
        print_bios_info();
        dprintf(1, "\n");
        bootmenu_autoselect();
        return;
    }

    // FIXME: remove this - for testing only!
    bootmenu_autoselect();

    fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_SB_SPLASH);

    // Show menu items
    struct bootentry_s *pos;
    int maxmenu = 0;
    bs_printf("\nBootmenu:\n");
    hlist_for_each_entry(pos, &BootList, node) {
        char desc[60];
        maxmenu++;
        bs_printf("  %d. %s\n", maxmenu, strtcpy(desc, pos->description, ARRAY_SIZE(desc)));
    }

    while (get_keystroke(0) >= 0)
        ;

    char *bootmsg = romfile_loadfile("etc/boot-menu-message", NULL);
    u64 boot_menu_key = romfile_loadint("etc/boot-menu-key", MENUKEY_F12);
    int menukey_code = MENUKEY_CODE(boot_menu_key);
    const char *menukey_text = MENUKEY_TEXT(boot_menu_key);
    if (bootmsg)
        bs_print(bootmsg);
    free(bootmsg);

    u32 menutime = romfile_loadint("etc/boot-menu-wait", DEFAULT_BOOTMENU_WAIT);
    enable_bootsplash(menukey_text);

    waitforinput_start();
    int scan_code = get_keystroke(menutime);
    waitforinput_stop();

    /* F1 will freeze the bootsplash and reboot after the next keypress */
    if ((scan_code == RAWKEY_F1) && get_bootsplash_active()) {
        bootsplash_show_paused();
        waitforinput_start();
        scan_code = get_keystroke(-1);
        waitforinput_stop();
        dprintf(1, "Rebooting.\n");
        tryReboot();
        // shouldn't get here
        goto bootsplash_off;
    }

    if (scan_code < 0) {
        //cpu1900_bootmenu_default();
        goto bootsplash_off;
    }

    if ((scan_code != menukey_code) && (scan_code != RAWKEY_ENTER)) {
        goto bootsplash_off;
    }

    while (get_keystroke(0) >= 0)
        ;

    bs_clear();
    bs_print("Select boot device:\n\n");
    wait_threads();

    // Show menu items
    maxmenu = 0;
    hlist_for_each_entry(pos, &BootList, node) {
        char desc[60];
        maxmenu++;
        bs_printf("%d. %s\n", maxmenu, strtcpy(desc, pos->description, ARRAY_SIZE(desc)));
    }
    if (tpm_can_show_menu()) {
        bs_printf("\nt. TPM Configuration\n");
    }

    bs_status_printf("Hit 1 - %d or F1 - F%d to boot or ESC to continue", maxmenu, maxmenu);
    dprintf(1, "\nHit 1 - %d to boot or ENTER to continue\n", maxmenu);

    // Get key press.  If the menu key is ESC, do not restart boot unless
    // 1.5 seconds have passed.  Otherwise users (trained by years of
    // repeatedly hitting keys to enter the BIOS) will end up hitting ESC
    // multiple times and immediately booting the primary boot device.
    int esc_accepted_time = irqtimer_calc(menukey_code == RAWKEY_ESC ? 1500 : 0);
    for (;;) {
        waitforinput_start();
        scan_code = get_keystroke(15000);
        waitforinput_stop();
        if (scan_code < 0) // timeout
            goto bootsplash_off;
        if ((scan_code == RAWKEY_ESC) && !irqtimer_check(esc_accepted_time))
            continue;
        if (scan_code == RAWKEY_ENTER)
            break;
        if (tpm_can_show_menu() && scan_code == 20 /* t */) {
            printf("\n");
            tpm_menu();
        }
        // map F1-F9 to 1-9
        if ((scan_code >= RAWKEY_F1) && (scan_code <= RAWKEY_F9))
            scan_code = RAWKEY_1 + (scan_code - RAWKEY_F1);
        if (scan_code >= 1 && scan_code <= maxmenu+1)
            break;
    }
    printf("\n");

    // Find entry and make top priority.
    if (scan_code >= 1 && scan_code <= maxmenu + 1)
        boot_idx = scan_code - 1;

bootsplash_off:
    bootmenu_select(boot_idx);
    fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_SB_SPLASH_OFF);
    disable_bootsplash();
}

// BEV (Boot Execution Vector) list
struct bev_s {
    int type;
    u32 vector;
};
static struct bev_s BEV[20];
static int BEVCount;
static int HaveFDBoot;

static void
add_bev(int type, u32 vector)
{
    if (type == IPL_TYPE_FLOPPY && HaveFDBoot++)
        return;
    if (BEVCount >= ARRAY_SIZE(BEV))
        return;
    struct bev_s *bev = &BEV[BEVCount++];
    bev->type = type;
    bev->vector = vector;
}

// Prepare for boot - show menu and run bcvs.
void
bcv_prepboot(void)
{
    if (! CONFIG_BOOT)
        return;

    int haltprio = find_prio("HALT");
    if (haltprio >= 0)
        bootentry_add(IPL_TYPE_HALT, haltprio, 0, "HALT");

    // Map drives and populate BEV list
    struct bootentry_s *pos;
    hlist_for_each_entry(pos, &BootList, node) {
        switch (pos->type) {
        case IPL_TYPE_BCV:
            call_bcv(pos->vector.seg, pos->vector.offset);
            add_bev(IPL_TYPE_HARDDISK, 0);
            break;
        case IPL_TYPE_FLOPPY:
            map_floppy_drive(pos->drive);
            add_bev(IPL_TYPE_FLOPPY, 0);
            break;
        case IPL_TYPE_HARDDISK:
            add_bev(IPL_TYPE_HARDDISK, map_hd_drive(pos->drive));
            break;
        case IPL_TYPE_CDROM:
            map_cd_drive(pos->drive);
            // NO BREAK
        default:
            add_bev(pos->type, pos->data);
            break;
        }
    }

    // If nothing added a floppy/hd boot - add it manually.
    add_bev(IPL_TYPE_FLOPPY, 0);
    add_bev(IPL_TYPE_HARDDISK, 0);
}


/****************************************************************
 * Boot code (int 18/19)
 ****************************************************************/

// Jump to a bootup entry point.
static void
call_boot_entry(struct segoff_s bootsegip, u8 bootdrv)
{
    dprintf(1, "Booting from %04x:%04x\n", bootsegip.seg, bootsegip.offset);
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF;
    br.code = bootsegip;
    // Set the magic number in ax and the boot drive in dl.
    br.dl = bootdrv;
    br.ax = 0xaa55;
    farcall16(&br);
}

// Boot from a disk (either floppy or harddrive)
static void
boot_disk(u8 bootdrv, int checksig)
{
    u16 bootseg = 0x07c0;

    // Read sector
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF;
    br.dl = bootdrv;
    br.es = bootseg;
    br.ah = 2;
    br.al = 1;
    br.cl = 1;
    call16_int(0x13, &br);

    if (br.flags & F_CF) {
        printf("Boot failed: could not read the boot disk\n\n");
        return;
    }

    struct mbr_s *mbr = (void*)0;
    uint32_t code = *(uint32_t *)&GET_FARVAR(bootseg, mbr->code);
    if ((code == 0) || (code == 0xffffffff) ||
        (checksig && (GET_FARVAR(bootseg, mbr->signature) != MBR_SIGNATURE))) {
        printf("Boot failed: not a bootable disk\n\n");
        return;
    }

    tpm_add_bcv(bootdrv, MAKE_FLATPTR(bootseg, 0), 512);

    /* Canonicalize bootseg:bootip */
    u16 bootip = (bootseg & 0x0fff) << 4;
    bootseg &= 0xf000;

    call_boot_entry(SEGOFF(bootseg, bootip), bootdrv);
}

// Boot from a CD-ROM
static void
boot_cdrom(struct drive_s *drive_g)
{
    if (! CONFIG_CDROM_BOOT)
        return;
    printf("Booting from DVD/CD...\n");

    int status = cdrom_boot(drive_g);
    if (status) {
        printf("Boot failed: Could not read from CDROM (code %04x)\n", status);
        return;
    }

    u8 bootdrv = CDEmu.emulated_drive;
    u16 bootseg = CDEmu.load_segment;

    tpm_add_cdrom(bootdrv, MAKE_FLATPTR(bootseg, 0), 512);

    /* Canonicalize bootseg:bootip */
    u16 bootip = (bootseg & 0x0fff) << 4;
    bootseg &= 0xf000;

    call_boot_entry(SEGOFF(bootseg, bootip), bootdrv);
}

// Boot from a CBFS payload
static void
boot_cbfs(struct cbfs_file *file)
{
    if (!CONFIG_COREBOOT_FLASH)
        return;
    printf("Booting from CBFS...\n");
    cbfs_run_payload(file);
}

// Boot from a BEV entry on an optionrom.
static void
boot_rom(u32 vector)
{
    printf("Booting from ROM...\n");
    struct segoff_s so;
    so.segoff = vector;
    call_boot_entry(so, 0);
}

// Unable to find bootable device - warn user and eventually retry.
static void
boot_fail(void)
{
    outb(0xef, 0x80);
    if (BootRetryTime == (u32)-1)
        printf("No bootable device.\n");
    else
        printf("No bootable device.  Retrying in %d seconds.\n"
               , BootRetryTime/1000);
    // Wait for 'BootRetryTime' milliseconds and then reboot.
    u32 end = irqtimer_calc(BootRetryTime);
    for (;;) {
        if (BootRetryTime != (u32)-1 && irqtimer_check(end))
            break;
        yield_toirq();
    }
    printf("Rebooting.\n");
    reset();
}

// Determine next boot method and attempt a boot using it.
static void
do_boot(int seq_nr)
{
    if (! CONFIG_BOOT)
        panic("Boot support not compiled in.\n");

    if (seq_nr >= BEVCount)
        boot_fail();

    // Boot the given BEV type.
    struct bev_s *ie = &BEV[seq_nr];

    outb(0xeb, 0x80);
    outb(seq_nr, 0x80);
    outb(ie->type, 0x80);
    outb(0xec, 0x80);

    fpga_write_u8(CPU1900_REG_BIOS_BOOT_STAGE, CPU1900_BOOT_STAGE_SB_PAYLOAD);

    switch (ie->type) {
    case IPL_TYPE_FLOPPY:
        printf("Booting from Floppy...\n");
        boot_disk(0x00, CheckFloppySig);
        break;
    case IPL_TYPE_HARDDISK:
        printf("Booting from Hard Disk...\n");
        setSwapHdId(ie->vector);
        boot_disk(0x80, 1);
        break;
    case IPL_TYPE_CDROM:
        boot_cdrom((void*)ie->vector);
        break;
    case IPL_TYPE_CBFS:
        boot_cbfs((void*)ie->vector);
        break;
    case IPL_TYPE_BEV:
        boot_rom(ie->vector);
        break;
    case IPL_TYPE_HALT:
        boot_fail();
        break;
    }

    // Boot failed: invoke the boot recovery function
    struct bregs br;
    memset(&br, 0, sizeof(br));
    br.flags = F_IF;
    call16_int(0x18, &br);
}

int BootSequence VARLOW = -1;

// Boot Failure recovery: try the next device.
void VISIBLE32FLAT
handle_18(void)
{
    outb(0xee, 0x80);

    debug_enter(NULL, DEBUG_HDL_18);
    int seq = BootSequence + 1;
    BootSequence = seq;
    do_boot(seq);
}

// INT 19h Boot Load Service Entry Point
void VISIBLE32FLAT
handle_19(void)
{
    debug_enter(NULL, DEBUG_HDL_19);

    /* set the Status LED back to the default */
    fpga_write_u8(CPU1900_REG_STATUS_LED_DUTY, CPU1900_LED_GREEN_BLINK);
    fpga_write_u8(CPU1900_REG_STATUS_LED_RATE, CPU1900_LED_2_HZ);

    BootSequence = 0;
    do_boot(0);
}
