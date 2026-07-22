#ifndef RISCV_CLIC_H
#define RISCV_CLIC_H

#include <stdint.h>

enum riscv_clic_result
{
    RISCV_CLIC_OK = 0,
    RISCV_CLIC_ERROR_ARGUMENT = -1,
    RISCV_CLIC_ERROR_INFO = -2,
    RISCV_CLIC_ERROR_CAPACITY = -3
};

enum riscv_clic_trigger
{
    RISCV_CLIC_TRIGGER_HIGH_LEVEL = 0,
    RISCV_CLIC_TRIGGER_POSITIVE_EDGE = 1,
    RISCV_CLIC_TRIGGER_NEGATIVE_EDGE = 3
};

struct riscv_clic_info
{
    uint32_t raw;
    uint32_t hw_irq_count;
    uint8_t control_bits;
};

struct riscv_clic_config
{
    uintptr_t base;
    /* Zero means the threshold is managed outside this MMIO driver. */
    uintptr_t threshold_addr;
    /* Used only when CLICINFO.num_interrupt is encoded as zero. */
    uint32_t zero_irq_count;
};

struct riscv_clic
{
    uintptr_t base;
    struct riscv_clic_info info;
};

int riscv_clic_init(struct riscv_clic *clic,
                    const struct riscv_clic_config *config);
int riscv_clic_configure_irq(struct riscv_clic *clic,
                             uint32_t irq,
                             enum riscv_clic_trigger trigger,
                             uint8_t level);
/* Fast MMIO operations require an initialized instance and a valid IRQ. */
void riscv_clic_enable_irq(struct riscv_clic *clic, uint32_t irq);
void riscv_clic_disable_irq(struct riscv_clic *clic, uint32_t irq);
void riscv_clic_set_pending(struct riscv_clic *clic, uint32_t irq);
void riscv_clic_clear_pending(struct riscv_clic *clic, uint32_t irq);
uint32_t riscv_clic_get_pending(struct riscv_clic *clic, uint32_t irq);
const struct riscv_clic_info *riscv_clic_get_info(
    const struct riscv_clic *clic);

#endif
