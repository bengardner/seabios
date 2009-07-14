// 16bit code to handle serial and printer services.
//
// Copyright (C) 2008,2009  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2002  MandrakeSoft S.A.
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "biosvar.h" // SET_BDA
#include "util.h" // debug_enter
#include "bregs.h" // struct bregs

// Timers based on 18.2Hz clock irq.
struct tick_timer_s {
    u16 last_tick, remaining;
};

struct tick_timer_s
initTickTimer(u16 count)
{
    struct tick_timer_s tt = {GET_BDA(timer_counter), count};
    return tt;
}

int
checkTickTimer(struct tick_timer_s *tt)
{
    u16 timer = GET_BDA(timer_counter);
    if (tt->last_tick != timer) {
        tt->last_tick = timer;
        tt->last_tick--;
        if (!tt->last_tick)
            return 1;
    }
    return 0;
}


/****************************************************************
 * COM ports
 ****************************************************************/

static u16
detect_serial(u16 port, u8 timeout, u8 count)
{
    outb(0x02, port+SEROFF_IER);
    u8 ier = inb(port+SEROFF_IER);
    if (ier != 0x02)
        return 0;
    u8 iir = inb(port+SEROFF_IIR);
    if ((iir & 0x3f) != 0x02)
        return 0;

    outb(0x00, port+SEROFF_IER);
    SET_BDA(port_com[count], port);
    SET_BDA(com_timeout[count], timeout);
    return 1;
}

void
serial_setup()
{
    if (! CONFIG_SERIAL)
        return;
    dprintf(3, "init serial\n");

    u16 count = 0;
    count += detect_serial(PORT_SERIAL1, 0x0a, count);
    count += detect_serial(PORT_SERIAL2, 0x0a, count);
    count += detect_serial(PORT_SERIAL3, 0x0a, count);
    count += detect_serial(PORT_SERIAL4, 0x0a, count);
    dprintf(1, "Found %d serial ports\n", count);

    // Equipment word bits 9..11 determing # serial ports
    u16 eqb = GET_BDA(equipment_list_flags);
    SET_BDA(equipment_list_flags, (eqb & 0xf1ff) | (count << 9));
}

static u16
getComAddr(struct bregs *regs)
{
    if (regs->dx >= 4) {
        set_fail(regs);
        return 0;
    }
    u16 addr = GET_BDA(port_com[regs->dx]);
    if (! addr)
        set_fail(regs);
    return addr;
}

// SERIAL - INITIALIZE PORT
static void
handle_1400(struct bregs *regs)
{
    u16 addr = getComAddr(regs);
    if (!addr)
        return;
    outb(inb(addr+SEROFF_LCR) | 0x80, addr+SEROFF_LCR);
    if ((regs->al & 0xE0) == 0) {
        outb(0x17, addr+SEROFF_DLL);
        outb(0x04, addr+SEROFF_DLH);
    } else {
        u16 val16 = 0x600 >> ((regs->al & 0xE0) >> 5);
        outb(val16 & 0xFF, addr+SEROFF_DLL);
        outb(val16 >> 8, addr+SEROFF_DLH);
    }
    outb(regs->al & 0x1F, addr+SEROFF_LCR);
    regs->ah = inb(addr+SEROFF_LSR);
    regs->al = inb(addr+SEROFF_MSR);
    set_success(regs);
}

// SERIAL - WRITE CHARACTER TO PORT
static void
handle_1401(struct bregs *regs)
{
    u16 addr = getComAddr(regs);
    if (!addr)
        return;
    struct tick_timer_s tt = initTickTimer(GET_BDA(com_timeout[regs->dx]));
    irq_enable();
    for (;;) {
        u8 lsr = inb(addr+SEROFF_LSR);
        if ((lsr & 0x60) == 0x60) {
            // Success - can write data
            outb(regs->al, addr+SEROFF_DATA);
            // XXX - reread lsr?
            regs->ah = lsr;
            break;
        }
        if (checkTickTimer(&tt)) {
            // Timed out - can't write data.
            regs->ah = lsr | 0x80;
            break;
        }
    }
    irq_disable();
    set_success(regs);
}

// SERIAL - READ CHARACTER FROM PORT
static void
handle_1402(struct bregs *regs)
{
    u16 addr = getComAddr(regs);
    if (!addr)
        return;
    struct tick_timer_s tt = initTickTimer(GET_BDA(com_timeout[regs->dx]));
    irq_enable();
    for (;;) {
        u8 lsr = inb(addr+SEROFF_LSR);
        if (lsr & 0x01) {
            // Success - can read data
            regs->al = inb(addr+SEROFF_DATA);
            regs->ah = lsr;
            break;
        }
        if (checkTickTimer(&tt)) {
            // Timed out - can't read data.
            regs->ah = lsr | 0x80;
            break;
        }
    }
    irq_disable();
    set_success(regs);
}

// SERIAL - GET PORT STATUS
static void
handle_1403(struct bregs *regs)
{
    u16 addr = getComAddr(regs);
    if (!addr)
        return;
    regs->ah = inb(addr+SEROFF_LSR);
    regs->al = inb(addr+SEROFF_MSR);
    set_success(regs);
}

static void
handle_14XX(struct bregs *regs)
{
    // Unsupported
    set_fail(regs);
}

// INT 14h Serial Communications Service Entry Point
void VISIBLE16
handle_14(struct bregs *regs)
{
    debug_enter(regs, DEBUG_HDL_14);
    if (! CONFIG_SERIAL) {
        handle_14XX(regs);
        return;
    }

    switch (regs->ah) {
    case 0x00: handle_1400(regs); break;
    case 0x01: handle_1401(regs); break;
    case 0x02: handle_1402(regs); break;
    case 0x03: handle_1403(regs); break;
    default:   handle_14XX(regs); break;
    }
}

// XXX - Baud Rate Generator Table
u8 BaudTable[16] VAR16FIXED(0xe729);


/****************************************************************
 * LPT ports
 ****************************************************************/

static u16
detect_parport(u16 port, u8 timeout, u8 count)
{
    // clear input mode
    outb(inb(port+2) & 0xdf, port+2);

    outb(0xaa, port);
    if (inb(port) != 0xaa)
        // Not present
        return 0;
    SET_BDA(port_lpt[count], port);
    SET_BDA(lpt_timeout[count], timeout);
    return 1;
}

void
lpt_setup()
{
    if (! CONFIG_LPT)
        return;
    dprintf(3, "init lpt\n");

    u16 count = 0;
    count += detect_parport(PORT_LPT1, 0x14, count);
    count += detect_parport(PORT_LPT2, 0x14, count);
    dprintf(1, "Found %d lpt ports\n", count);

    // Equipment word bits 14..15 determing # parallel ports
    u16 eqb = GET_BDA(equipment_list_flags);
    SET_BDA(equipment_list_flags, (eqb & 0x3fff) | (count << 14));
}

static u16
getLptAddr(struct bregs *regs)
{
    if (regs->dx >= 3) {
        set_fail(regs);
        return 0;
    }
    u16 addr = GET_BDA(port_lpt[regs->dx]);
    if (! addr)
        set_fail(regs);
    return addr;
}

// INT 17 - PRINTER - WRITE CHARACTER
static void
handle_1700(struct bregs *regs)
{
    u16 addr = getLptAddr(regs);
    if (!addr)
        return;

    struct tick_timer_s tt = initTickTimer(GET_BDA(lpt_timeout[regs->dx]));
    irq_enable();

    outb(regs->al, addr);
    u8 val8 = inb(addr+2);
    outb(val8 | 0x01, addr+2); // send strobe
    udelay(5);
    outb(val8 & ~0x01, addr+2);

    for (;;) {
        u8 v = inb(addr+1);
        if (!(v & 0x40)) {
            // Success
            regs->ah = v ^ 0x48;
            break;
        }
        if (checkTickTimer(&tt)) {
            // Timeout
            regs->ah = (v ^ 0x48) | 0x01;
            break;
        }
    }

    irq_disable();
    set_success(regs);
}

// INT 17 - PRINTER - INITIALIZE PORT
static void
handle_1701(struct bregs *regs)
{
    u16 addr = getLptAddr(regs);
    if (!addr)
        return;

    u8 val8 = inb(addr+2);
    outb(val8 & ~0x04, addr+2); // send init
    udelay(5);
    outb(val8 | 0x04, addr+2);

    regs->ah = inb(addr+1) ^ 0x48;
    set_success(regs);
}

// INT 17 - PRINTER - GET STATUS
static void
handle_1702(struct bregs *regs)
{
    u16 addr = getLptAddr(regs);
    if (!addr)
        return;
    regs->ah = inb(addr+1) ^ 0x48;
    set_success(regs);
}

static void
handle_17XX(struct bregs *regs)
{
    // Unsupported
    set_fail(regs);
}

// INT17h : Printer Service Entry Point
void VISIBLE16
handle_17(struct bregs *regs)
{
    debug_enter(regs, DEBUG_HDL_17);
    if (! CONFIG_LPT) {
        handle_17XX(regs);
        return;
    }

    switch (regs->ah) {
    case 0x00: handle_1700(regs); break;
    case 0x01: handle_1701(regs); break;
    case 0x02: handle_1702(regs); break;
    default:   handle_17XX(regs); break;
    }
}
