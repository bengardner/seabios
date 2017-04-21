/**
 * FPGA register definitions for the Wabtec CPU-1900.
 *
 * Register definitions start with CPU1900_REG_.
 * Multi-byte registers should end in '_#', where the number is the byte order.
 * This defines a virtual multi-byte register without the '_#'.
 * Fields can be defined against that virtual register (see WD_TC).
 *
 * Field definitions start with the register name, followed by '__' and the
 * field name. The field is a mask or a reference to another field.
 * Common fields can be defined without a real register (see W1__SEQ).
 *
 * Enumeration values start with the field name, followed by '__' and the enum
 * name. The field is the enum value AS IT APPEARS IN THE REGISTER.
 * In other words, or'ing the enum to the write value should work.
 */
#ifndef WABTEC_CPU1900_H_INCLUDED
#define WABTEC_CPU1900_H_INCLUDED

/* fixed base I/O address and reserved size */
#define CPU1900_FPGA_REG_BASE                   0x1100
#define CPU1900_FPGA_REG_SIZE                   0x0200

/* register offset from the BAR */
#define CPU1900_REG_FPGA_MAJOR_REV              (0x00)
#define CPU1900_REG_FPGA_MINOR_REV              (0x01)
#define CPU1900_REG_SCRATCH                     (0x02)
#define CPU1900_REG_HW_REV                      (0x03)
#define CPU1900_REG_SLOTID                      (0x04)
#define  CPU1900_REG_SLOTID__VIN1                       0x40
#define  CPU1900_REG_SLOTID__VIN2                       0x20
#define  CPU1900_REG_SLOTID__ID                         0x1f
#define CPU1900_REG_FPGA_OPTIONS                (0x05)
#define  CPU1900_REG_FPGA_OPTIONS__JUMPERS              0x70
#define  CPU1900_REG_FPGA_OPTIONS__OPTIONS              0x0f
#define CPU1900_REG_DEBUG_LED                   (0x06)
#define  CPU1900_REG_DEBUG_LED__MUX                     0x80
#define  CPU1900_REG_DEBUG_LED__EXP                     0x10
#define  CPU1900_REG_DEBUG_LED__LED3                    0x08
#define  CPU1900_REG_DEBUG_LED__LED2                    0x04
#define  CPU1900_REG_DEBUG_LED__LED1                    0x02
#define  CPU1900_REG_DEBUG_LED__LED0                    0x01
#define CPU1900_REG_STATUS_LED_RATE             (0x07)
#define  CPU1900_REG_STATUS_LED_RATE__RATE              0x7f
#define CPU1900_REG_STATUS_LED_DUTY             (0x08)
#define  CPU1900_REG_STATUS_LED_DUTY__RED               0x80
#define  CPU1900_REG_STATUS_LED_DUTY__DUTY              0x7f
#define CPU1900_REG_DTE_LED_RATE                (0x09)
#define  CPU1900_REG_DTE_LED_RATE__RATE                 0x7f
#define CPU1900_REG_DTE_LED_DUTY                (0x0a)
#define  CPU1900_REG_DTE_LED_DUTY__RED                  0x80
#define  CPU1900_REG_DTE_LED_DUTY__DUTY                 0x7f
#define CPU1900_REG_RESET_1                     (0x0b)
#define  CPU1900_REG_RESET_1__PCIE_DISABLE              0x80
#define  CPU1900_REG_RESET_1__PCIE_RESET                0x40
#define  CPU1900_REG_RESET_1__FP_ETH_DISABLE            0x20
#define  CPU1900_REG_RESET_1__FP_ETH_RESET              0x10
#define  CPU1900_REG_RESET_1__BP_ETH_DISABLE            0x08
#define  CPU1900_REG_RESET_1__BP_ETH_RESET              0x04
#define  CPU1900_REG_RESET_1__USB3_DISABLE              0x02
#define CPU1900_REG_RESET_2                     (0x0c)
#define  CPU1900_REG_RESET_2__EXP_CARD_RESET            0x04
#define  CPU1900_REG_RESET_2__DISPLAY_DISABLE           0x02
#define  CPU1900_REG_RESET_2__DISPLAY_RESET             0x01
#define CPU1900_REG_FPGA_REBOOT                 (0x0d)
#define CPU1900_REG_RESET_CAUSE                 (0x0e)
#define  CPU1900_REG_RESET_CAUSE__M                     0x07
#define   CPU1900_REG_RESET_CAUSE__M__COLD                      0x00
#define   CPU1900_REG_RESET_CAUSE__M__WD                        0x01
#define   CPU1900_REG_RESET_CAUSE__M__SLEEP                     0x02
#define   CPU1900_REG_RESET_CAUSE__M__POWER                     0x03
#define   CPU1900_REG_RESET_CAUSE__M__SW_RESET                  0x04
#define CPU1900_REG_LPC_ABORT_COUNT             (0x0f)
#define CPU1900_REG_PSS_1                       (0x10)
#define CPU1900_REG_PSS_2                       (0x11)
#define CPU1900_REG_PSS_3                       (0x12)
#define CPU1900_REG_WD_CSR                      (0x13)
#define  CPU1900_REG_WD_CSR__HW_DISABLE                 0x04
#define  CPU1900_REG_WD_CSR__SW_DISABLE                 0x02
#define  CPU1900_REG_WD_CSR__KICK                       0x01
#define CPU1900_REG_WD_TC_0                     (0x14)
#define CPU1900_REG_WD_TC_1                     (0x15)
#define  CPU1900_REG_WD_TC__MASK                        0x0fff
#define CPU1900_REG_BIOS_WRITE                  (0x16)
#define CPU1900_REG_BIOS_BOOT                   (0x17)
#define  CPU1900_REG_BIOS_BOOT__ALIVE                   0x20
#define  CPU1900_REG_BIOS_BOOT__HAPPY                   0x10
#define  CPU1900_REG_BIOS_BOOT__FD_OFF                  0x08
#define  CPU1900_REG_BIOS_BOOT__FAILED                  0x04
#define  CPU1900_REG_BIOS_BOOT__NEXT                    0x02
#define  CPU1900_REG_BIOS_BOOT__BOOT                    0x01
#define CPU1900_REG_BIOS_BOOT_STAGE             (0x18)
#define CPU1900_REG_BIOS_LAST_STAGE             (0x19)
#define CPU1900_REG_BIOS_BOOT_SOURCE            (0x1a)
#define  CPU1900_REG_BIOS_BOOT_SOURCE__IDX              0x0f
#define  CPU1900_REG_BIOS_BOOT_SOURCE__TYPE             0x70
#define   CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__NONE              0x00
#define   CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__USB               0x10
#define   CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__SATA              0x20
#define   CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__MMC               0x30
#define   CPU1900_REG_BIOS_BOOT_SOURCE__TYPE__OTHER             0x40
#define CPU1900_REG_BIOS_BOOT_COUNT             (0x1b)
#define CPU1900_REG_BIOS_BOOT_COUNT__TEST_REBOOT        0x80   /* reboot before setting HAPPY */
#define CPU1900_REG_BIOS_BOOT_COUNT__TEST_HAPPY         0x40   /* do not set happy */
#define CPU1900_REG_BIOS_BOOT_COUNT__TEST_ALIVE         0x20   /* do not set alive */
#define CPU1900_REG_BIOS_BOOT_COUNT__TEST_FAILED        0x10   /* copy of CPU1900_REG_BIOS_BOOT__FAILED */
#define CPU1900_REG_BIOS_BOOT_COUNT__COUNT              0x0f
#define CPU1900_REG_BIOS_SELECT                 (0x1c)
#define  CPU1900_REG_BIOS_SELECT__BUSY                  0x08
#define  CPU1900_REG_BIOS_SELECT__TOGGLE                0x04
#define  CPU1900_REG_BIOS_SELECT__BOOT                  0x02
#define  CPU1900_REG_BIOS_SELECT__CUR                   0x01
#define CPU1900_REG_I2C_INFO                    (0x1d)
#define  CPU1900_REG_I2C_INFO__PRESENT                  0x80
#define  CPU1900_REG_I2C_INFO__REVISION                 0x7f
#define   CPU1900_REG_I2C_INFO__REVISION__PCA9539               0x01
#define CPU1900_REG_MISC                        (0x1e)
#define  CPU1900_REG_MISC__NOT_ALIVE_COUNT              0xc0
#define  CPU1900_REG_MISC__NOT_HAPPY_COUNT              0x38
#define  CPU1900_REG_MISC__SLEEP_LEVEL                  0x02
#define  CPU1900_REG_MISC__SLEEP_DISABLE                0x01

#define CPU1900_REG_ADC_BASE                    (0x20)

#define CPU1900_REG_I2C_IN_0                    (0x60)
#define CPU1900_REG_I2C_IN_1                    (0x61)
#define CPU1900_REG_I2C_OUT_0                   (0x62)
#define CPU1900_REG_I2C_OUT_1                   (0x63)
#define CPU1900_REG_I2C_CFG_0                   (0x64)
#define CPU1900_REG_I2C_CFG_1                   (0x65)
#define CPU1900_REG_I2C_CS                      (0x66)
#define  CPU1900_REG_I2C_CS__RESET                      0x80
#define  CPU1900_REG_I2C_CS__ERROR                      0x40
#define  CPU1900_REG_I2C_CS__BUSY                       0x20
#define  CPU1900_REG_I2C_CS__RW                         0x10
#define  CPU1900_REG_I2C_CS__BANK                       0x03
#define   CPU1900_REG_I2C_CS__BANK__IN                          0x00
#define   CPU1900_REG_I2C_CS__BANK__OUT                         0x01
#define   CPU1900_REG_I2C_CS__BANK__INV                         0x02
#define   CPU1900_REG_I2C_CS__BANK__CFG                         0x03

// W1__SEQ is used for both status and control
#define  CPU1900_REG_W1__SEQ                            0x70
#define   CPU1900_REG_W1__SEQ__NONE                             0x00
#define   CPU1900_REG_W1__SEQ__RESET                            0x10
#define   CPU1900_REG_W1__SEQ__TOUCH                            0x20
#define   CPU1900_REG_W1__SEQ__READ8                            0x30
#define   CPU1900_REG_W1__SEQ__WRITE8                           0x40
#define   CPU1900_REG_W1__SEQ__TRIPLET                          0x50

#define CPU1900_REG_W1_CONTROL                  (0x80)
#define  CPU1900_REG_W1_CONTROL__SEQ                    CPU1900_REG_W1__SEQ
#define  CPU1900_REG_W1_CONTROL__PU_STRONG              0x08
#define  CPU1900_REG_W1_CONTROL__PU_ACTIVE              0x04
#define  CPU1900_REG_W1_CONTROL__WRITE                  0x01
#define CPU1900_REG_W1_STATUS                   (0x81)
#define  CPU1900_REG_W1_STATUS__BUSY                    0x80
#define  CPU1900_REG_W1_STATUS__SEQ                     CPU1900_REG_W1__SEQ
#define  CPU1900_REG_W1_STATUS__ERROR                   0x04
#define  CPU1900_REG_W1_STATUS__LEVEL                   0x02
#define  CPU1900_REG_W1_STATUS__RESULT                  0x01
#define CPU1900_REG_W1_DATA                     (0x82)

enum cpu1900_boot_stage {
	CPU1900_BOOT_STAGE_COLDBOOT      = 0x00, // only on power cycle
	CPU1900_BOOT_STAGE_CB_BOOTBLOCK  = 0x01,
	CPU1900_BOOT_STAGE_CB_ROMSTAGE   = 0x02,
	CPU1900_BOOT_STAGE_CB_RAMSTAGE   = 0x03,
	CPU1900_BOOT_STAGE_CB_PAYLOAD    = 0x04,
	CPU1900_BOOT_STAGE_SB_SPLASH     = 0x10,
	CPU1900_BOOT_STAGE_SB_SPLASH_OFF = 0x11,
	CPU1900_BOOT_STAGE_SB_PAYLOAD    = 0x1f,
	CPU1900_BOOT_STAGE_OS_DRIVER     = 0x20,
	CPU1900_BOOT_STAGE_ULP_START     = 0x30,
	CPU1900_BOOT_STAGE_ULP_INSTALL   = 0x32, // downloaded something
	CPU1900_BOOT_STAGE_ULP_REINSTALL = 0x33, // did not download something
	CPU1900_BOOT_STAGE_ULP_DONE      = 0x3f, // end of ULP script
	CPU1900_BOOT_STAGE_APP_START     = 0x40,
	CPU1900_BOOT_STAGE_APP_HAPPY     = 0x4f,
};

/* ADC registers defined */
enum cpu1900_adc {
	CPU1900_ADC_1_0V_S     = 0x00,
	CPU1900_ADC_1_0V_SX    = 0x01,
	CPU1900_ADC_VCC_1_0V_S = 0x02,
	CPU1900_ADC_VNN_1_0V_S = 0x03,
	CPU1900_ADC_VDDQ_1_35V = 0x04,
	CPU1900_ADC_VSFR_SX    = 0x05,
	CPU1900_ADC_VTT_DDR    = 0x06,
	CPU1900_ADC_1_35V_S    = 0x07,
	CPU1900_ADC_1_05V_S    = 0x08,
	CPU1900_ADC_1_5V_S     = 0x09,
	CPU1900_ADC_1_8V_S     = 0x0a,
	CPU1900_ADC_3_3V_S     = 0x0b,
	CPU1900_ADC_5V_S       = 0x0c,
	CPU1900_ADC_1_0V_A     = 0x0d,
	CPU1900_ADC_1_8V_A     = 0x0e,
	CPU1900_ADC_3_3V_A     = 0x0f,
	CPU1900_ADC_VIN1       = 0x10,
	CPU1900_ADC_VIN2       = 0x11,
	CPU1900_ADC_5V_SYS     = 0x12,
	CPU1900_ADC_3_3V_SYS   = 0x13,
	CPU1900_ADC_2_5V_SYS   = 0x14,
	CPU1900_ADC_1_8V_SYS   = 0x15,
	CPU1900_ADC_1_2V_SYS   = 0x16,
	CPU1900_ADC_1_8V_IFSUP = 0x17,

	CPU1900_ADC_COUNT // must be last
};

#if defined(WANT_CPU1900_ADC_NAMES) || defined(DEFINE_CPU1900_ADC_NAMES)
#ifdef DEFINE_CPU1900_ADC_NAMES

/* names for the sysfs thingy */
const char *cpu1900_adc_names[] = {
	[CPU1900_ADC_1_0V_S]     = "1.0V_S",
	[CPU1900_ADC_1_0V_SX]    = "1.0V_SX",
	[CPU1900_ADC_VCC_1_0V_S] = "VCC_1.0V_S",
	[CPU1900_ADC_VNN_1_0V_S] = "VNN_1.0V_S",
	[CPU1900_ADC_VDDQ_1_35V] = "VDDQ_1.35V",
	[CPU1900_ADC_VSFR_SX]    = "VSFR_SX",
	[CPU1900_ADC_VTT_DDR]    = "VTT_DDR",
	[CPU1900_ADC_1_35V_S]    = "1.35V_S",
	[CPU1900_ADC_1_05V_S]    = "1.05V_S",
	[CPU1900_ADC_1_5V_S]     = "1.5V_S",
	[CPU1900_ADC_1_8V_S]     = "1.8V_S",
	[CPU1900_ADC_3_3V_S]     = "3.3V_S",
	[CPU1900_ADC_5V_S]       = "5V_S",
	[CPU1900_ADC_1_0V_A]     = "1.0V_A",
	[CPU1900_ADC_1_8V_A]     = "1.8V_A",
	[CPU1900_ADC_3_3V_A]     = "3.3V_A",
	[CPU1900_ADC_VIN1]       = "VIN1",
	[CPU1900_ADC_VIN2]       = "VIN2",
	[CPU1900_ADC_5V_SYS]     = "5V_SYS",
	[CPU1900_ADC_3_3V_SYS]   = "3.3V_SYS",
	[CPU1900_ADC_2_5V_SYS]   = "2.5V_SYS",
	[CPU1900_ADC_1_8V_SYS]   = "1.8V_SYS",
	[CPU1900_ADC_1_2V_SYS]   = "1.2V_SYS",
	[CPU1900_ADC_1_8V_IFSUP] = "1.8V_IFSUP",
};

#else /* DEFINE_CPU1900_ADC_NAMES */
extern const char *cpu1900_adc_names[];
#endif /* DEFINE_CPU1900_ADC_NAMES */
#endif /* WANT_CPU1900_ADC_NAMES || DEFINE_CPU1900_ADC_NAMES */

/* maps an ADC number to a PSS register and mask */
static inline void cpu1900_adc_to_pss(int adc, int *reg, int *mask)
{
	*reg  = CPU1900_REG_PSS_1 + (adc >> 3);
	*mask = 1 << (adc & 0x07);
}

/**
 * Returns the mask to use with the "power_supply" sysfs item.
 */
static inline int cpu1900_adc_mask(enum cpu1900_adc adc)
{
	return (1 << adc);
}

/* all time-related registers (LED rate, WD TC) use 100 ms tick values */
#define CPU1900_TICK_MS                 100
#define CPU1900_SEC_TO_TICK(_sec)       ((_sec) * 10)
#define CPU1900_MS_TO_TICK(_ms)         (((_ms) + 99) / 100)

/* watchdog timer values */
#define CPU1900_WATCHDOG_DEF_SEC        7
#define CPU1900_WATCHDOG_RESET_SEC      150
#define CPU1900_WATCHDOG_MIN_SEC        1
#define CPU1900_WATCHDOG_MAX_SEC        409 /* 409.5 due to 12 bit TC */

/* funny enum to match the CPU-410 interface */
enum cpu1900_led_sys_values {
	CPU1900_LED_BLACK       = 0,    /* LED is off */
	CPU1900_LED_GREEN_BLINK = 0x40, /* 50/50 blink */
	CPU1900_LED_GREEN       = 0x7f, /* solid GREEN */
	CPU1900_LED_RED_BLINK   = 0xc0, /* 50/50 blink */
	CPU1900_LED_RED         = 0xff, /* solid RED */
	CPU1900_LED_MAX         = CPU1900_LED_RED
};

/* defines to map HZ to LED_RATE values */
#define CPU1900_LED_10_HZ       0  /* 100 ms */
#define CPU1900_LED_5_HZ        1  /* 200 ms */
#define CPU1900_LED_3_3_HZ      2  /* 300 ms */
#define CPU1900_LED_2_5_HZ      3  /* 400 ms */
#define CPU1900_LED_2_HZ        4  /* 500 ms */
#define CPU1900_LED_1_HZ        9  /* 1000 ms */
#define CPU1900_LED_0_5_HZ      19 /* 2000 ms */

#endif /* WABTEC_CPU1900_H_INCLUDED */
