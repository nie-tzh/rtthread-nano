#include "t22_serdes_irq.h"

    .section .vectors, "a", @progbits
    .balign 64
    .globl t22_serdes_irq_vectors
    .globl t22_serdes_irq_vectors_end
    .type t22_serdes_irq_vectors, @object

t22_serdes_irq_vectors:
    /* The common entry dispatches each vector by mcause.ExceptionCode. */
    .rept T22_SERDES_IRQ_VECTOR_COUNT
    .word e902_irq_entry
    .endr
t22_serdes_irq_vectors_end:

    .size t22_serdes_irq_vectors, . - t22_serdes_irq_vectors
