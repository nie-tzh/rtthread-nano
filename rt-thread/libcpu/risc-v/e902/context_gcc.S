#include "e902_context.h"

    .section .text.context, "ax", @progbits
    .balign 4

    .globl rt_hw_context_switch_to
    .type rt_hw_context_switch_to, @function
rt_hw_context_switch_to:
    csrci mstatus, 8
    la    t0, rt_thread_switch_interrupt_flag
    sw    zero, 0(t0)
    lw    sp, 0(a0)
    beqz  sp, .Lcontext_start_halt
    lw    t0, E902_EXCEPTION_MCAUSE_OFFSET(sp)
    csrw  mcause, t0
    j     e902_context_restore

.Lcontext_start_halt:
    la    t0, g_e902_context_last_error
    li    t1, E902_CONTEXT_ERROR_STATE_VALUE
    sw    t1, 0(t0)
    la    t0, g_e902_context_halted
    li    t1, 1
    sw    t1, 0(t0)
.Lcontext_start_halt_loop:
    j     .Lcontext_start_halt_loop

    .size rt_hw_context_switch_to, . - rt_hw_context_switch_to

    .globl rt_hw_interrupt_disable
    .type rt_hw_interrupt_disable, @function
rt_hw_interrupt_disable:
    csrrci a0, mstatus, 8
    ret

    .size rt_hw_interrupt_disable, . - rt_hw_interrupt_disable

    .globl rt_hw_interrupt_enable
    .type rt_hw_interrupt_enable, @function
rt_hw_interrupt_enable:
    csrw mstatus, a0
    ret

    .size rt_hw_interrupt_enable, . - rt_hw_interrupt_enable
