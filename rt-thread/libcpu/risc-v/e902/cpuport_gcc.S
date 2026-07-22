#include "e902.h"

    .macro E902_SAVE_CONTEXT
    addi sp, sp, -E902_FRAME_SIZE

    sw   x1,  E902_FRAME_X1_OFFSET(sp)
    sw   x3,  E902_FRAME_X3_OFFSET(sp)
    sw   x4,  E902_FRAME_X4_OFFSET(sp)
    sw   x5,  E902_FRAME_X5_OFFSET(sp)
    sw   x6,  E902_FRAME_X6_OFFSET(sp)
    sw   x7,  E902_FRAME_X7_OFFSET(sp)
    sw   x8,  E902_FRAME_X8_OFFSET(sp)
    sw   x9,  E902_FRAME_X9_OFFSET(sp)
    sw   x10, E902_FRAME_X10_OFFSET(sp)
    sw   x11, E902_FRAME_X11_OFFSET(sp)
    sw   x12, E902_FRAME_X12_OFFSET(sp)
    sw   x13, E902_FRAME_X13_OFFSET(sp)
    sw   x14, E902_FRAME_X14_OFFSET(sp)
    sw   x15, E902_FRAME_X15_OFFSET(sp)

    addi t0, sp, E902_FRAME_SIZE
    sw   t0, E902_FRAME_X2_OFFSET(sp)

    csrr t0, mepc
    sw   t0, E902_FRAME_MEPC_OFFSET(sp)
    csrr t0, mstatus
    sw   t0, E902_FRAME_MSTATUS_OFFSET(sp)
    csrr t0, mcause
    sw   t0, E902_FRAME_MCAUSE_OFFSET(sp)
    csrr t0, mtval
    sw   t0, E902_FRAME_MTVAL_OFFSET(sp)
    sw   zero, E902_FRAME_RESERVED_OFFSET(sp)
    .endm

    .macro E902_RESTORE_CONTEXT
    lw   t0, E902_FRAME_MSTATUS_OFFSET(sp)
    csrw mstatus, t0
    lw   t0, E902_FRAME_MEPC_OFFSET(sp)
    csrw mepc, t0

    lw   x1,  E902_FRAME_X1_OFFSET(sp)
    lw   x3,  E902_FRAME_X3_OFFSET(sp)
    lw   x4,  E902_FRAME_X4_OFFSET(sp)
    lw   x5,  E902_FRAME_X5_OFFSET(sp)
    lw   x6,  E902_FRAME_X6_OFFSET(sp)
    lw   x7,  E902_FRAME_X7_OFFSET(sp)
    lw   x8,  E902_FRAME_X8_OFFSET(sp)
    lw   x9,  E902_FRAME_X9_OFFSET(sp)
    lw   x10, E902_FRAME_X10_OFFSET(sp)
    lw   x11, E902_FRAME_X11_OFFSET(sp)
    lw   x12, E902_FRAME_X12_OFFSET(sp)
    lw   x13, E902_FRAME_X13_OFFSET(sp)
    lw   x14, E902_FRAME_X14_OFFSET(sp)
    lw   x15, E902_FRAME_X15_OFFSET(sp)
    lw   x2,  E902_FRAME_X2_OFFSET(sp)
    mret
    .endm

    .section .text.context, "ax", @progbits
    .balign 4

    .globl e902_context_switch_entry
    .type e902_context_switch_entry, @function
e902_context_switch_entry:
    E902_SAVE_CONTEXT

    la   t0, rt_thread_switch_interrupt_flag
    lw   t2, 0(t0)
    beqz t2, .Lcontext_irq_restore
    sw   zero, 0(t0)

    la   t0, rt_interrupt_from_thread
    lw   t1, 0(t0)
    /* from == 0 is startup: the boot-stack frame is temporary. */
    beqz t1, .Lcontext_load_to
    sw   sp, 0(t1)

.Lcontext_load_to:
    la   t0, rt_interrupt_to_thread
    lw   t1, 0(t0)
    lw   t2, 0(t1)
    mv   sp, t2

    /*
     * Keep the active IRQ 3 mcause for mret.  The target thread's saved
     * mcause describes its previous trap and must not replace this value.
     */
    E902_RESTORE_CONTEXT

.Lcontext_irq_restore:
    lw   t0, E902_FRAME_MCAUSE_OFFSET(sp)
    csrw mcause, t0
    E902_RESTORE_CONTEXT

    .size e902_context_switch_entry, . - e902_context_switch_entry

    .globl rt_hw_interrupt_disable
    .type rt_hw_interrupt_disable, @function
rt_hw_interrupt_disable:
    csrrci a0, mstatus, E902_MSTATUS_MIE_MASK
    ret

    .size rt_hw_interrupt_disable, . - rt_hw_interrupt_disable

    .globl rt_hw_interrupt_enable
    .type rt_hw_interrupt_enable, @function
rt_hw_interrupt_enable:
    csrw mstatus, a0
    ret

    .size rt_hw_interrupt_enable, . - rt_hw_interrupt_enable

    .section .text.irq_entry, "ax", @progbits
    .balign 4
    .globl e902_irq_entry
    .type e902_irq_entry, @function

e902_irq_entry:
    E902_SAVE_CONTEXT

    call rt_interrupt_enter
    mv   a0, sp
    call e902_irq_dispatch
    call rt_interrupt_leave

    /* Ordinary interrupts always return through their original frame. */
    lw   t0, E902_FRAME_MCAUSE_OFFSET(sp)
    csrw mcause, t0
    E902_RESTORE_CONTEXT

    .size e902_irq_entry, . - e902_irq_entry

    .section .text.exception, "ax", @progbits
    .balign 64
    .globl e902_exception_entry
    .type e902_exception_entry, @function

e902_exception_entry:
    E902_SAVE_CONTEXT

    mv   a0, sp
    call e902_exception_dispatch
    beqz a0, .Lexception_halt

    lw   t0, E902_FRAME_MCAUSE_OFFSET(sp)
    csrw mcause, t0
    E902_RESTORE_CONTEXT

.Lexception_halt:
    csrci mstatus, E902_MSTATUS_MIE_MASK

.Lexception_halt_loop:
    j .Lexception_halt_loop

    .size e902_exception_entry, . - e902_exception_entry
