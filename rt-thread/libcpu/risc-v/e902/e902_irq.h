#ifndef E902_IRQ_H
#define E902_IRQ_H

#include <stdint.h>

#include "riscv_clic.h"

struct e902_frame;

enum e902_irq_level
{
    E902_IRQ_LEVEL0 = 0U,
    E902_IRQ_LEVEL1 = 1U,
    E902_IRQ_LEVEL2 = 2U,
    E902_IRQ_LEVEL3 = 3U,
    E902_IRQ_LEVEL4 = 4U,
    E902_IRQ_LEVEL5 = 5U,
    E902_IRQ_LEVEL6 = 6U,
    E902_IRQ_LEVEL7 = 7U
};

/* The SoC owns clic_config and the vector table. */
int e902_irq_init(struct riscv_clic *clic,
                  const struct riscv_clic_config *clic_config,
                  const uintptr_t *vector_table,
                  uint32_t vector_count);
struct riscv_clic *e902_irq_clic(void);
void e902_irq_dispatch(struct e902_frame *frame);
/* BSP hook used by the common E902 IRQ entry. */
void rt_hw_interrupt_dispatch(int vector);

#endif
