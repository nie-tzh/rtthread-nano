#include <stdint.h>

#include "e902_irq.h"
#include "riscv_clic.h"
#include "t22_serdes.h"
#include "t22_serdes_irq.h"

extern const uintptr_t t22_serdes_irq_vectors[];

static struct riscv_clic t22_serdes_clic_instance;
static const struct riscv_clic_config t22_serdes_clic_config =
{
    .base = T22_SERDES_CLIC_BASE,
    .threshold_addr = T22_SERDES_CLIC_MINTTHRESH_ADDR,
    .zero_irq_count = T22_SERDES_CLIC_ZERO_IRQ_COUNT
};

_Static_assert(T22_SERDES_IRQ_HIGHEST < T22_SERDES_IRQ_VECTOR_COUNT,
               "T22 IRQ vector table does not cover the highest IRQ");

int t22_serdes_irq_init(void)
{
    return e902_irq_init(&t22_serdes_clic_instance,
                         &t22_serdes_clic_config,
                         t22_serdes_irq_vectors,
                         T22_SERDES_IRQ_VECTOR_COUNT);
}

struct riscv_clic *t22_serdes_clic(void)
{
    return &t22_serdes_clic_instance;
}
