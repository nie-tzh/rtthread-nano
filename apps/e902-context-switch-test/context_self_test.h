#ifndef E902_CONTEXT_SELF_TEST_H
#define E902_CONTEXT_SELF_TEST_H

#define CONTEXT_TEST_MARKER_RA          0x11111111
#define CONTEXT_TEST_MARKER_TP          0x44444444
#define CONTEXT_TEST_MARKER_T0          0x55555555
#define CONTEXT_TEST_MARKER_T1          0x66666666
#define CONTEXT_TEST_MARKER_T2          0x77777777
#define CONTEXT_TEST_MARKER_S0          0x88888888
#define CONTEXT_TEST_MARKER_S1          0x99999999
#define CONTEXT_TEST_MARKER_A0          0xA0A0A0A0
#define CONTEXT_TEST_MARKER_A1          0xA1A1A1A1
#define CONTEXT_TEST_MARKER_A2          0xA2A2A2A2
#define CONTEXT_TEST_MARKER_A3          0xA3A3A3A3
#define CONTEXT_TEST_MARKER_A4          0xA4A4A4A4
#define CONTEXT_TEST_MARKER_A5          0xA5A5A5A5

#define CONTEXT_TEST_SCRATCH_SIZE       96
#define CONTEXT_TEST_SAVED_RA_OFFSET     0
#define CONTEXT_TEST_FROM_OFFSET         4
#define CONTEXT_TEST_TO_OFFSET           8
#define CONTEXT_TEST_OUTPUT_OFFSET      12
#define CONTEXT_TEST_MSTATUS_OFFSET     16
#define CONTEXT_TEST_SAVED_S0_OFFSET    20
#define CONTEXT_TEST_SAVED_S1_OFFSET    24
#define CONTEXT_TEST_SAVED_TP_OFFSET    28
#define CONTEXT_TEST_SNAPSHOT_OFFSET    32

#define CONTEXT_TEST_REG_RA_OFFSET       0
#define CONTEXT_TEST_REG_SP_OFFSET       4
#define CONTEXT_TEST_REG_GP_OFFSET       8
#define CONTEXT_TEST_REG_TP_OFFSET      12
#define CONTEXT_TEST_REG_T0_OFFSET      16
#define CONTEXT_TEST_REG_T1_OFFSET      20
#define CONTEXT_TEST_REG_T2_OFFSET      24
#define CONTEXT_TEST_REG_S0_OFFSET      28
#define CONTEXT_TEST_REG_S1_OFFSET      32
#define CONTEXT_TEST_REG_A0_OFFSET      36
#define CONTEXT_TEST_REG_A1_OFFSET      40
#define CONTEXT_TEST_REG_A2_OFFSET      44
#define CONTEXT_TEST_REG_A3_OFFSET      48
#define CONTEXT_TEST_REG_A4_OFFSET      52
#define CONTEXT_TEST_REG_A5_OFFSET      56
#define CONTEXT_TEST_REGISTER_COUNT     15
#define CONTEXT_TEST_REGISTERS_SIZE     60

#if !defined(__ASSEMBLER__)

#include <stddef.h>
#include <stdint.h>

#include "e902_context.h"

struct context_test_registers
{
    uint32_t ra;
    uint32_t sp;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t s0;
    uint32_t s1;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
};

_Static_assert(sizeof(struct context_test_registers) ==
               CONTEXT_TEST_REGISTERS_SIZE,
               "Context test register snapshot size mismatch");
_Static_assert(offsetof(struct context_test_registers, sp) ==
               CONTEXT_TEST_REG_SP_OFFSET,
               "Context test SP offset mismatch");
_Static_assert(offsetof(struct context_test_registers, a5) ==
               CONTEXT_TEST_REG_A5_OFFSET,
               "Context test A5 offset mismatch");

void e902_context_register_self_test(
    e902_ubase_t from,
    e902_ubase_t to,
    struct context_test_registers *output);

#endif

#endif
