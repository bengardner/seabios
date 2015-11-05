/**
 * FPGA register definitions for the Wabtec CPU-1900.
 */
#ifndef WABTEC_CPU1900_H_INCLUDED
#define WABTEC_CPU1900_H_INCLUDED

/* fixed base I/O address and reserved size */
#define CPU1900_REG_BASE	0x1100
#define CPU1900_REG_SIZE	0x0200	// must be base-2


/* register offset from the BAR */
#define CPU1900_REG_FPGA_MAJOR_REV		(CPU1900_REG_BASE + 0x00)
#define CPU1900_REG_FPGA_MINOR_REV		(CPU1900_REG_BASE + 0x01)
#define CPU1900_REG_SCRATCH			(CPU1900_REG_BASE + 0x02)
#define CPU1900_REG_HW_REV			(CPU1900_REG_BASE + 0x03)
#define CPU1900_REG_SLOTID			(CPU1900_REG_BASE + 0x04)
#define  CPU1900_REG_SLOTID__VIN1			0x40
#define  CPU1900_REG_SLOTID__VIN2			0x20
#define  CPU1900_REG_SLOTID__ID				0x1f
#define CPU1900_REG_FPGA_OPTIONS		(CPU1900_REG_BASE + 0x05)
#define CPU1900_REG_DEBUG_LED			(CPU1900_REG_BASE + 0x06)
#define  CPU1900_REG_DEBUG_LED__MUX			0x80
#define  CPU1900_REG_DEBUG_LED__EXP			0x10
#define  CPU1900_REG_DEBUG_LED__LED3			0x08
#define  CPU1900_REG_DEBUG_LED__LED2			0x04
#define  CPU1900_REG_DEBUG_LED__LED1			0x02
#define  CPU1900_REG_DEBUG_LED__LED0			0x01
#define CPU1900_REG_STATUS_LED_RATE		(CPU1900_REG_BASE + 0x07)
#define  CPU1900_REG_LED_RATE__RATE			0x7f // both Status and DTE
#define CPU1900_REG_STATUS_LED_DUTY		(CPU1900_REG_BASE + 0x08)
#define  CPU1900_REG_LED_DUTY__RED			0x80 // both Status and DTE
#define  CPU1900_REG_LED_DUTY__DUTY			0x7f // both Status and DTE
#define CPU1900_REG_DTE_LED_RATE			(CPU1900_REG_BASE + 0x09)
#define CPU1900_REG_DTE_LED_DUTY		(CPU1900_REG_BASE + 0x0a)
#define CPU1900_REG_RESET_1			(CPU1900_REG_BASE + 0x0b)
#define  CPU1900_REG_RESET_1__PCIE_DISABLE		0x80
#define  CPU1900_REG_RESET_1__PCIE_RESET		0x40
#define  CPU1900_REG_RESET_1__FP_ETH_DISABLE		0x20
#define  CPU1900_REG_RESET_1__FP_ETH_RESET		0x10
#define  CPU1900_REG_RESET_1__BP_ETH_DISABLE		0x08
#define  CPU1900_REG_RESET_1__BP_ETH_RESET		0x04
#define  CPU1900_REG_RESET_1__USB3_DISABLE		0x02
#define CPU1900_REG_RESET_2			(CPU1900_REG_BASE + 0x0c)
#define  CPU1900_REG_RESET_2__EXP_CARD_RESET		0x04
#define  CPU1900_REG_RESET_2__DISPLAY_DISABLE		0x02
#define  CPU1900_REG_RESET_2__DISPLAY_RESET		0x01
#define CPU1900_REG_FPGA_REBOOT			(CPU1900_REG_BASE + 0x0d)
#define CPU1900_REG_RESET_CAUSE			(CPU1900_REG_BASE + 0x0e)
#define  CPU1900_REG_RESET_CAUSE__MASK			0x07
#define  CPU1900_REG_RESET_CAUSE__COLD			0
#define  CPU1900_REG_RESET_CAUSE__WD			1
#define  CPU1900_REG_RESET_CAUSE__SLEEP			2
#define  CPU1900_REG_RESET_CAUSE__POWER			3
#define  CPU1900_REG_RESET_CAUSE__SW_RESET		4
#define CPU1900_REG_LPC_ABORT_COUNT		(CPU1900_REG_BASE + 0x0f)
#define CPU1900_REG_PSS1			(CPU1900_REG_BASE + 0x10)
#define CPU1900_REG_PSS2			(CPU1900_REG_BASE + 0x11)
#define CPU1900_REG_PSS3			(CPU1900_REG_BASE + 0x12)
#define CPU1900_REG_WD_CSR			(CPU1900_REG_BASE + 0x13)
#define  CPU1900_REG_WD_CSR__HW_DISABLE			0x04
#define  CPU1900_REG_WD_CSR__SW_DISABLE			0x02
#define  CPU1900_REG_WD_CSR__KICK			0x01
#define CPU1900_REG_WD_TC_LSB			(CPU1900_REG_BASE + 0x14)
#define CPU1900_REG_WD_TC_MSB			(CPU1900_REG_BASE + 0x15)
#define  CPU1900_REG_WD_TC__MASK			0x0fff
#define CPU1900_REG_BIOS_WRITE			(CPU1900_REG_BASE + 0x16)
#define CPU1900_REG_BIOS_BANK			(CPU1900_REG_BASE + 0x17)
#define CPU1900_REG_ADC_BASE			(CPU1900_REG_BASE + 0x20)

/* Used to enable extra debug logs if (inb(reg) & mask) == val */
#define CPU1900_REG_DBG				CPU1900_REG_FPGA_OPTIONS
#define CPU1900_REG_DBG_MSK			0x10
#define CPU1900_REG_DBG_VAL			0x00

/* ADC registers defined */
#define CPU1900_ADC_1_0V_S			0x00
#define CPU1900_ADC_1_0V_SX			0x01
#define CPU1900_ADC_VCC_1_0V_S			0x02
#define CPU1900_ADC_VNN_1_0V_S			0x03
#define CPU1900_ADC_VDDQ_1_35V			0x04
#define CPU1900_ADC_VSFR_SX			0x05
#define CPU1900_ADC_VTT_DDR			0x06
#define CPU1900_ADC_1_35V_S			0x07
#define CPU1900_ADC_1_05V_S			0x08
#define CPU1900_ADC_1_5V_S			0x09
#define CPU1900_ADC_1_8V_S			0x0a
#define CPU1900_ADC_3_3V_S			0x0b
#define CPU1900_ADC_5V_S			0x0c
#define CPU1900_ADC_1_0V_A			0x0d
#define CPU1900_ADC_1_8V_A			0x0e
#define CPU1900_ADC_3_3V_A			0x0f
#define CPU1900_ADC_VIN1			0x10
#define CPU1900_ADC_VIN2			0x11
#define CPU1900_ADC_5V_SYS			0x12
#define CPU1900_ADC_3_3V_SYS			0x13
#define CPU1900_ADC_2_5V_SYS			0x14
#define CPU1900_ADC_1_8V_SYS			0x15
#define CPU1900_ADC_1_2V_SYS			0x16
#define CPU1900_ADC_1_8V_IFSUP			0x17

#ifdef DEFINE_CPU1900_ADC_NAMES
/* names for the sysfs thingy */
static const char *cpu1900_adc_names[] = {
	[CPU1900_ADC_1_0V_S]		= "1.0V_S",
	[CPU1900_ADC_1_0V_SX]		= "1.0V_SX",
	[CPU1900_ADC_VCC_1_0V_S]	= "VCC_1.0V_S",
	[CPU1900_ADC_VNN_1_0V_S]	= "VNN_1.0V_S",
	[CPU1900_ADC_VDDQ_1_35V]	= "VDDQ_1.35V",
	[CPU1900_ADC_VSFR_SX]		= "VSFR_SX",
	[CPU1900_ADC_VTT_DDR]		= "VTT_DDR",
	[CPU1900_ADC_1_35V_S]		= "1.35V_S",
	[CPU1900_ADC_1_05V_S]		= "1.05V_S",
	[CPU1900_ADC_1_5V_S]		= "1.5V_S",
	[CPU1900_ADC_1_8V_S]		= "1.8V_S",
	[CPU1900_ADC_3_3V_S]		= "3.3V_S",
	[CPU1900_ADC_5V_S]		= "5V_S",
	[CPU1900_ADC_1_0V_A]		= "1.0V_A",
	[CPU1900_ADC_1_8V_A]		= "1.8V_A",
	[CPU1900_ADC_3_3V_A]		= "3.3V_A",
	[CPU1900_ADC_VIN1]		= "VIN1",
	[CPU1900_ADC_VIN2]		= "VIN2",
	[CPU1900_ADC_5V_SYS]		= "5V_SYS",
	[CPU1900_ADC_3_3V_SYS]		= "3.3V_SYS",
	[CPU1900_ADC_2_5V_SYS]		= "2.5V_SYS",
	[CPU1900_ADC_1_8V_SYS]		= "1.8V_SYS",
	[CPU1900_ADC_1_2V_SYS]		= "1.2V_SYS",
	[CPU1900_ADC_1_8V_IFSUP]	= "1.8V_IFSUP",
};
#endif /* DEFINE_CPU1900_ADC_NAMES */

/* maps an ADC number to a PSS register and mask */
static inline void cpu1900_adc_to_pss(int adc, int *reg, int *mask)
{
	*reg  = CPU1900_REG_PSS1 + (adc >> 3);
	*mask = 1 << (adc & 0x07);
}

/* all time-related registers (LED rate, WD TC) use 100 ms tick values */
#define CPU1900_TICK_MS			100
#define CPU1900_SEC_TO_TICK(_sec)	((_sec) * 10)
#define CPU1900_MS_TO_TICK(_ms)		(((_ms) + 99) / 100)

/* watchdog timer values */
#define CPU1900_WATCHDOG_DEF_SEC	7
#define CPU1900_WATCHDOG_RESET_SEC	150
#define CPU1900_WATCHDOG_MIN_SEC	1
#define CPU1900_WATCHDOG_MAX_SEC	409 /* 409.5 due to 12 bit TC */

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
