#ifndef E902_H
#define E902_H

#define E902_FRAME_X1_OFFSET             0
#define E902_FRAME_X2_OFFSET             4
#define E902_FRAME_X3_OFFSET             8
#define E902_FRAME_X4_OFFSET            12
#define E902_FRAME_X5_OFFSET            16
#define E902_FRAME_X6_OFFSET            20
#define E902_FRAME_X7_OFFSET            24
#define E902_FRAME_X8_OFFSET            28
#define E902_FRAME_X9_OFFSET            32
#define E902_FRAME_X10_OFFSET           36
#define E902_FRAME_X11_OFFSET           40
#define E902_FRAME_X12_OFFSET           44
#define E902_FRAME_X13_OFFSET           48
#define E902_FRAME_X14_OFFSET           52
#define E902_FRAME_X15_OFFSET           56
#define E902_FRAME_MEPC_OFFSET          60
#define E902_FRAME_MSTATUS_OFFSET       64
#define E902_FRAME_MCAUSE_OFFSET        68
#define E902_FRAME_MTVAL_OFFSET         72
#define E902_FRAME_RESERVED_OFFSET      76
#define E902_FRAME_SIZE                 80

#define E902_MCAUSE_INTERRUPT_MASK      0x80000000
#define E902_MCAUSE_CODE_MASK           0x00000FFF

#define E902_MSTATUS_MIE_MASK           0x00000008
#define E902_MSTATUS_MPIE_MASK          0x00000080
#define E902_MSTATUS_MPP_MACHINE        0x00001800
#if !defined(__ASSEMBLER__)

#include <stddef.h>
#include <stdint.h>
struct e902_frame
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
    uint32_t mepc;
    uint32_t mstatus;
    uint32_t mcause;
    uint32_t mtval;
    uint32_t reserved;
};

_Static_assert(sizeof(struct e902_frame) == E902_FRAME_SIZE,
               "E902 frame size mismatch");
_Static_assert(offsetof(struct e902_frame, mepc) ==
               E902_FRAME_MEPC_OFFSET,
               "E902 frame mepc offset mismatch");
_Static_assert(offsetof(struct e902_frame, mtval) ==
               E902_FRAME_MTVAL_OFFSET,
               "E902 frame mtval offset mismatch");

void e902_context_switch_init(void);
void e902_exception_putchar(char ch);

#endif

#endif
