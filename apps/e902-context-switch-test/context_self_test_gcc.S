#include "context_self_test.h"
#include "e902.h"

    .section .text.context_test, "ax", @progbits
    .balign 4
    .globl e902_context_register_self_test
    .type e902_context_register_self_test, @function

e902_context_register_self_test:
    addi sp, sp, -CONTEXT_TEST_SCRATCH_SIZE
    sw   ra, CONTEXT_TEST_SAVED_RA_OFFSET(sp)
    sw   a0, CONTEXT_TEST_FROM_OFFSET(sp)
    sw   a1, CONTEXT_TEST_TO_OFFSET(sp)
    sw   a2, CONTEXT_TEST_OUTPUT_OFFSET(sp)
    sw   s0, CONTEXT_TEST_SAVED_S0_OFFSET(sp)
    sw   s1, CONTEXT_TEST_SAVED_S1_OFFSET(sp)
    sw   tp, CONTEXT_TEST_SAVED_TP_OFFSET(sp)

    call rt_hw_interrupt_disable
    sw   a0, CONTEXT_TEST_MSTATUS_OFFSET(sp)
    lw   a0, CONTEXT_TEST_FROM_OFFSET(sp)
    lw   a1, CONTEXT_TEST_TO_OFFSET(sp)
    call rt_hw_context_switch

    li   ra, CONTEXT_TEST_MARKER_RA
    li   tp, CONTEXT_TEST_MARKER_TP
    li   t0, CONTEXT_TEST_MARKER_T0
    li   t1, CONTEXT_TEST_MARKER_T1
    li   t2, CONTEXT_TEST_MARKER_T2
    li   s0, CONTEXT_TEST_MARKER_S0
    li   s1, CONTEXT_TEST_MARKER_S1
    li   a0, CONTEXT_TEST_MARKER_A0
    li   a1, CONTEXT_TEST_MARKER_A1
    li   a2, CONTEXT_TEST_MARKER_A2
    li   a3, CONTEXT_TEST_MARKER_A3
    li   a4, CONTEXT_TEST_MARKER_A4
    li   a5, CONTEXT_TEST_MARKER_A5

    csrsi mstatus, E902_MSTATUS_MIE_MASK
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

    sw   ra, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_RA_OFFSET(sp)
    sw   sp, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_SP_OFFSET(sp)
    sw   gp, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_GP_OFFSET(sp)
    sw   tp, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_TP_OFFSET(sp)
    sw   t0, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_T0_OFFSET(sp)
    sw   t1, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_T1_OFFSET(sp)
    sw   t2, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_T2_OFFSET(sp)
    sw   s0, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_S0_OFFSET(sp)
    sw   s1, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_S1_OFFSET(sp)
    sw   a0, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_A0_OFFSET(sp)
    sw   a1, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_A1_OFFSET(sp)
    sw   a2, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_A2_OFFSET(sp)
    sw   a3, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_A3_OFFSET(sp)
    sw   a4, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_A4_OFFSET(sp)
    sw   a5, CONTEXT_TEST_SNAPSHOT_OFFSET + CONTEXT_TEST_REG_A5_OFFSET(sp)

    lw   t0, CONTEXT_TEST_OUTPUT_OFFSET(sp)
    addi t1, sp, CONTEXT_TEST_SNAPSHOT_OFFSET
    li   t2, CONTEXT_TEST_REGISTER_COUNT
.Lcopy_registers:
    lw   a0, 0(t1)
    sw   a0, 0(t0)
    addi t1, t1, 4
    addi t0, t0, 4
    addi t2, t2, -1
    bnez t2, .Lcopy_registers

    lw   t0, CONTEXT_TEST_MSTATUS_OFFSET(sp)
    csrw mstatus, t0
    lw   tp, CONTEXT_TEST_SAVED_TP_OFFSET(sp)
    lw   s0, CONTEXT_TEST_SAVED_S0_OFFSET(sp)
    lw   s1, CONTEXT_TEST_SAVED_S1_OFFSET(sp)
    lw   ra, CONTEXT_TEST_SAVED_RA_OFFSET(sp)
    addi sp, sp, CONTEXT_TEST_SCRATCH_SIZE
    ret

    .size e902_context_register_self_test, . - e902_context_register_self_test
