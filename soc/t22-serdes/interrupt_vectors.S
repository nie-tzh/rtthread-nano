#include "t22_serdes_irq.h"

    .section .vectors, "a", @progbits
    .balign 64
    .globl t22_serdes_irq_vectors
    .globl t22_serdes_irq_vectors_end
    .type t22_serdes_irq_vectors, @object

t22_serdes_irq_vectors:
    /* IRQ 3 owns the deferred context-switch entry; other IRQs dispatch. */
    .rept T22_SERDES_IRQ_MACHINE_SOFTWARE
    .word e902_irq_entry
    .endr
    .word e902_context_switch_entry
    .rept (T22_SERDES_IRQ_VECTOR_COUNT - \
           T22_SERDES_IRQ_MACHINE_SOFTWARE - 1)
    .word e902_irq_entry
    .endr
t22_serdes_irq_vectors_end:

    .size t22_serdes_irq_vectors, . - t22_serdes_irq_vectors
