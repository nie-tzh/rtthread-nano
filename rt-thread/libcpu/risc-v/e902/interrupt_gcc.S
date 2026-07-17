#include "e902_exception.h"

    .section .text.irq_entry, "ax", @progbits
    .balign 4
    .globl e902_irq_entry
    .type e902_irq_entry, @function

e902_irq_entry:
    addi sp, sp, -E902_EXCEPTION_FRAME_SIZE

    sw   x1,  E902_EXCEPTION_X1_OFFSET(sp)
    sw   x3,  E902_EXCEPTION_X3_OFFSET(sp)
    sw   x4,  E902_EXCEPTION_X4_OFFSET(sp)
    sw   x5,  E902_EXCEPTION_X5_OFFSET(sp)
    sw   x6,  E902_EXCEPTION_X6_OFFSET(sp)
    sw   x7,  E902_EXCEPTION_X7_OFFSET(sp)
    sw   x8,  E902_EXCEPTION_X8_OFFSET(sp)
    sw   x9,  E902_EXCEPTION_X9_OFFSET(sp)
    sw   x10, E902_EXCEPTION_X10_OFFSET(sp)
    sw   x11, E902_EXCEPTION_X11_OFFSET(sp)
    sw   x12, E902_EXCEPTION_X12_OFFSET(sp)
    sw   x13, E902_EXCEPTION_X13_OFFSET(sp)
    sw   x14, E902_EXCEPTION_X14_OFFSET(sp)
    sw   x15, E902_EXCEPTION_X15_OFFSET(sp)

    addi t0, sp, E902_EXCEPTION_FRAME_SIZE
    sw   t0, E902_EXCEPTION_X2_OFFSET(sp)

    csrr t0, mepc
    sw   t0, E902_EXCEPTION_MEPC_OFFSET(sp)
    csrr t0, mstatus
    sw   t0, E902_EXCEPTION_MSTATUS_OFFSET(sp)
    csrr t0, mcause
    sw   t0, E902_EXCEPTION_MCAUSE_OFFSET(sp)
    csrr t0, mtval
    sw   t0, E902_EXCEPTION_MTVAL_OFFSET(sp)
    sw   zero, E902_EXCEPTION_RESERVED_OFFSET(sp)

    mv   a0, sp
    call e902_irq_dispatch

    /* mret uses mcause.MPIL to restore the previous CLIC level. */
    lw   t0, E902_EXCEPTION_MCAUSE_OFFSET(sp)
    csrw mcause, t0
    lw   t0, E902_EXCEPTION_MSTATUS_OFFSET(sp)
    csrw mstatus, t0
    lw   t0, E902_EXCEPTION_MEPC_OFFSET(sp)
    csrw mepc, t0

    lw   x1,  E902_EXCEPTION_X1_OFFSET(sp)
    lw   x3,  E902_EXCEPTION_X3_OFFSET(sp)
    lw   x4,  E902_EXCEPTION_X4_OFFSET(sp)
    lw   x5,  E902_EXCEPTION_X5_OFFSET(sp)
    lw   x6,  E902_EXCEPTION_X6_OFFSET(sp)
    lw   x7,  E902_EXCEPTION_X7_OFFSET(sp)
    lw   x8,  E902_EXCEPTION_X8_OFFSET(sp)
    lw   x9,  E902_EXCEPTION_X9_OFFSET(sp)
    lw   x10, E902_EXCEPTION_X10_OFFSET(sp)
    lw   x11, E902_EXCEPTION_X11_OFFSET(sp)
    lw   x12, E902_EXCEPTION_X12_OFFSET(sp)
    lw   x13, E902_EXCEPTION_X13_OFFSET(sp)
    lw   x14, E902_EXCEPTION_X14_OFFSET(sp)
    lw   x15, E902_EXCEPTION_X15_OFFSET(sp)
    lw   x2,  E902_EXCEPTION_X2_OFFSET(sp)
    mret

    .size e902_irq_entry, . - e902_irq_entry
