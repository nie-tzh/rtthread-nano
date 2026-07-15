    .section .text.e902_exception_self_test, "ax", @progbits
    .align 2

    .equ E902_SELF_TEST_FRAME_SIZE,       144
    .equ E902_SELF_TEST_GUARD_OFFSET,       0
    .equ E902_SELF_TEST_RA_OFFSET,          4
    .equ E902_SELF_TEST_GP_OFFSET,          8
    .equ E902_SELF_TEST_TP_OFFSET,         12
    .equ E902_SELF_TEST_S0_OFFSET,         16
    .equ E902_SELF_TEST_S1_OFFSET,         20
    .equ E902_SELF_TEST_EXPECTED_OFFSET,   24
    .equ E902_SELF_TEST_RESTORED_OFFSET,   84
    .equ E902_SELF_TEST_REGISTER_COUNT,    15
    .equ E902_SELF_TEST_GUARD_VALUE,       0xA55A5AA5

    .globl e902_exception_register_self_test
    .type e902_exception_register_self_test, @function

e902_exception_register_self_test:
    addi sp, sp, -E902_SELF_TEST_FRAME_SIZE

    sw   ra, E902_SELF_TEST_RA_OFFSET(sp)
    sw   gp, E902_SELF_TEST_GP_OFFSET(sp)
    sw   tp, E902_SELF_TEST_TP_OFFSET(sp)
    sw   s0, E902_SELF_TEST_S0_OFFSET(sp)
    sw   s1, E902_SELF_TEST_S1_OFFSET(sp)

    li   t0, E902_SELF_TEST_GUARD_VALUE
    sw   t0, E902_SELF_TEST_GUARD_OFFSET(sp)

    li   x1,  0x11111111
    li   x4,  0x44444444
    li   x5,  0x55555555
    li   x6,  0x66666666
    li   x7,  0x77777777
    li   x8,  0x88888888
    li   x9,  0x99999999
    li   x10, 0xAAAAAAAA
    li   x11, 0xBBBBBBBB
    li   x12, 0xCCCCCCCC
    li   x13, 0xDDDDDDDD
    li   x14, 0xEEEEEEEE
    li   x15, 0xFFFFFFFF

    sw   x1,  E902_SELF_TEST_EXPECTED_OFFSET + 0(sp)
    sw   x2,  E902_SELF_TEST_EXPECTED_OFFSET + 4(sp)
    sw   x3,  E902_SELF_TEST_EXPECTED_OFFSET + 8(sp)
    sw   x4,  E902_SELF_TEST_EXPECTED_OFFSET + 12(sp)
    sw   x5,  E902_SELF_TEST_EXPECTED_OFFSET + 16(sp)
    sw   x6,  E902_SELF_TEST_EXPECTED_OFFSET + 20(sp)
    sw   x7,  E902_SELF_TEST_EXPECTED_OFFSET + 24(sp)
    sw   x8,  E902_SELF_TEST_EXPECTED_OFFSET + 28(sp)
    sw   x9,  E902_SELF_TEST_EXPECTED_OFFSET + 32(sp)
    sw   x10, E902_SELF_TEST_EXPECTED_OFFSET + 36(sp)
    sw   x11, E902_SELF_TEST_EXPECTED_OFFSET + 40(sp)
    sw   x12, E902_SELF_TEST_EXPECTED_OFFSET + 44(sp)
    sw   x13, E902_SELF_TEST_EXPECTED_OFFSET + 48(sp)
    sw   x14, E902_SELF_TEST_EXPECTED_OFFSET + 52(sp)
    sw   x15, E902_SELF_TEST_EXPECTED_OFFSET + 56(sp)

    .option push
    .option norvc
    .globl e902_exception_self_test_ebreak
e902_exception_self_test_ebreak:
    ebreak
    .option pop

    sw   x1,  E902_SELF_TEST_RESTORED_OFFSET + 0(sp)
    sw   x2,  E902_SELF_TEST_RESTORED_OFFSET + 4(sp)
    sw   x3,  E902_SELF_TEST_RESTORED_OFFSET + 8(sp)
    sw   x4,  E902_SELF_TEST_RESTORED_OFFSET + 12(sp)
    sw   x5,  E902_SELF_TEST_RESTORED_OFFSET + 16(sp)
    sw   x6,  E902_SELF_TEST_RESTORED_OFFSET + 20(sp)
    sw   x7,  E902_SELF_TEST_RESTORED_OFFSET + 24(sp)
    sw   x8,  E902_SELF_TEST_RESTORED_OFFSET + 28(sp)
    sw   x9,  E902_SELF_TEST_RESTORED_OFFSET + 32(sp)
    sw   x10, E902_SELF_TEST_RESTORED_OFFSET + 36(sp)
    sw   x11, E902_SELF_TEST_RESTORED_OFFSET + 40(sp)
    sw   x12, E902_SELF_TEST_RESTORED_OFFSET + 44(sp)
    sw   x13, E902_SELF_TEST_RESTORED_OFFSET + 48(sp)
    sw   x14, E902_SELF_TEST_RESTORED_OFFSET + 52(sp)
    sw   x15, E902_SELF_TEST_RESTORED_OFFSET + 56(sp)

    addi t0, sp, E902_SELF_TEST_EXPECTED_OFFSET
    addi t1, sp, E902_SELF_TEST_RESTORED_OFFSET
    li   t2, E902_SELF_TEST_REGISTER_COUNT
    li   a0, 1

.Lcompare_registers:
    lw   a1, 0(t0)
    lw   a2, 0(t1)
    bne  a1, a2, .Lself_test_done
    addi t0, t0, 4
    addi t1, t1, 4
    addi t2, t2, -1
    addi a0, a0, 1
    bnez t2, .Lcompare_registers

    lw   a1, E902_SELF_TEST_GUARD_OFFSET(sp)
    li   a2, E902_SELF_TEST_GUARD_VALUE
    bne  a1, a2, .Lself_test_done
    li   a0, 0

.Lself_test_done:
    lw   ra, E902_SELF_TEST_RA_OFFSET(sp)
    lw   gp, E902_SELF_TEST_GP_OFFSET(sp)
    lw   tp, E902_SELF_TEST_TP_OFFSET(sp)
    lw   s0, E902_SELF_TEST_S0_OFFSET(sp)
    lw   s1, E902_SELF_TEST_S1_OFFSET(sp)
    addi sp, sp, E902_SELF_TEST_FRAME_SIZE
    ret

    .size e902_exception_register_self_test, . - e902_exception_register_self_test
