    .section .text.init, "ax", @progbits
    .align  2
    .globl  _start
    .type   _start, @function

_start:
    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop

    la      sp, __stack_top

    la      t0, __data_load_start
    la      t1, __data_start
    la      t2, __data_end
.Lcopy_data:
    bgeu    t1, t2, .Lclear_bss_start
    lw      a0, 0(t0)
    sw      a0, 0(t1)
    addi    t0, t0, 4
    addi    t1, t1, 4
    j       .Lcopy_data

.Lclear_bss_start:
    la      t0, __bss_start
    la      t1, __bss_end
.Lclear_bss:
    bgeu    t0, t1, .Lcall_board_init
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       .Lclear_bss

.Lcall_board_init:
    call    board_init

.Lcall_main:
    call    main

.Lhang:
    j       .Lhang

    .size   _start, . - _start
