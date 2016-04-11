/**
 * Wabtec CPU-1900 FPGA I/O wrapper.
 */
#ifndef WABTEC_CPU1900_IO_H_INCLUDED
#define WABTEC_CPU1900_IO_H_INCLUDED

#include "wabtec-cpu1900.h"

/* Used to enable extra debug logs if (inb(reg) & mask) == val */
#define CPU1900_REG_DBG				CPU1900_REG_FPGA_OPTIONS
#define CPU1900_REG_DBG_MSK			0x10
#define CPU1900_REG_DBG_VAL			0x00

static inline uint8_t fpga_read_u8(u16 reg)
{
	if (reg < CPU1900_FPGA_REG_SIZE)
		return inb(CPU1900_FPGA_REG_BASE + reg);
	return -1;
}

static inline void fpga_write_u8(u16 reg, u8 val)
{
	if (reg < CPU1900_FPGA_REG_SIZE)
		return outb(val, CPU1900_FPGA_REG_BASE + reg);
}

#endif /* WABTEC_CPU1900_IO_H_INCLUDED */
