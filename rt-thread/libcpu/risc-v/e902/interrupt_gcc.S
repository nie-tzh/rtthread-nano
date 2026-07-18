#include "e902_exception.h"
#include "e902_context.h"

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

    lw   t0, E902_EXCEPTION_MCAUSE_OFFSET(sp)
    bgez t0, .Lirq_restore
    li   t1, E902_MCAUSE_CODE_MASK
    and  t0, t0, t1
    li   t1, E902_CONTEXT_SWITCH_IRQ
    bne  t0, t1, .Lirq_restore

    la   t0, rt_thread_switch_interrupt_flag
    lw   t2, 0(t0)
    beqz t2, .Lirq_restore
    sw   zero, 0(t0)

    la   t0, rt_interrupt_from_thread
    lw   t1, 0(t0)
    beqz t1, .Lcontext_switch_halt
    sw   sp, 0(t1)

    la   t0, rt_interrupt_to_thread
    lw   t1, 0(t0)
    beqz t1, .Lcontext_switch_halt
    lw   t2, 0(t1)
    beqz t2, .Lcontext_switch_halt
    mv   sp, t2

    la   t0, g_e902_context_switch_count
    lw   t1, 0(t0)
    addi t1, t1, 1
    sw   t1, 0(t0)

    /* Keep the active IRQ 3 mcause so mret restores its return MPIL. */
    j    e902_context_restore

.Lirq_restore:
    /* A same-frame IRQ return restores the saved active trap state. */
    lw   t0, E902_EXCEPTION_MCAUSE_OFFSET(sp)
    csrw mcause, t0

    .globl e902_context_restore
    .type e902_context_restore, @function
e902_context_restore:
    /* Thread restore deliberately leaves the active mcause unchanged. */
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

    .size e902_context_restore, . - e902_context_restore

.Lcontext_switch_halt:
    la    t0, g_e902_context_last_error
    li    t1, E902_CONTEXT_ERROR_STATE_VALUE
    sw    t1, 0(t0)
    la    t0, g_e902_context_halted
    li    t1, 1
    sw    t1, 0(t0)
    csrci mstatus, 8
.Lcontext_switch_halt_loop:
    j     .Lcontext_switch_halt_loop

    .size e902_irq_entry, . - e902_irq_entry
