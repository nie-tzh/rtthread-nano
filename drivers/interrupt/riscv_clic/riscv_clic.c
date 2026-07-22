#include <stddef.h>
#include <rthw.h>

#include "riscv_clic.h"

#define RISCV_CLIC_INFO_NUM_MASK             0x00001FFFUL
#define RISCV_CLIC_INFO_CONTROL_BITS_SHIFT   21U
#define RISCV_CLIC_INFO_CONTROL_BITS_MASK    0xFU
#define RISCV_CLIC_MAX_HW_IRQS               4096U
#define RISCV_CLIC_MAX_CONTROL_BITS          8U

#define RISCV_CLIC_CONFIG_NLBITS_SHIFT       1U
#define RISCV_CLIC_CONFIG_NLBITS_MASK        0x1EU
#define RISCV_CLIC_INTERRUPT_OFFSET          0x1000U
#define RISCV_CLIC_INT_ENABLE                0x01U
#define RISCV_CLIC_ATTR_SHV                  0x01U
#define RISCV_CLIC_ATTR_TRIGGER_SHIFT        1U
#define RISCV_CLIC_ATTR_TRIGGER_MASK         0x06U

struct riscv_clic_irq_regs
{
    volatile uint8_t pending;
    volatile uint8_t enable;
    volatile uint8_t attribute;
    volatile uint8_t control;
};

struct riscv_clic_regs
{
    volatile uint8_t config;
    uint8_t reserved0[3];
    const volatile uint32_t info;
    uint8_t reserved1[RISCV_CLIC_INTERRUPT_OFFSET - 0x0008U];
    struct riscv_clic_irq_regs interrupts[];
};

_Static_assert(offsetof(struct riscv_clic_irq_regs, pending) == 0U,
               "CLICINTIP offset mismatch");
_Static_assert(offsetof(struct riscv_clic_irq_regs, enable) == 1U,
               "CLICINTIE offset mismatch");
_Static_assert(offsetof(struct riscv_clic_irq_regs, attribute) == 2U,
               "CLICINTATTR offset mismatch");
_Static_assert(offsetof(struct riscv_clic_irq_regs, control) == 3U,
               "CLICINTCTL offset mismatch");
_Static_assert(sizeof(struct riscv_clic_irq_regs) == 4U,
               "CLIC interrupt register stride mismatch");
_Static_assert(offsetof(struct riscv_clic_regs, config) == 0x0000U,
               "CLICCFG offset mismatch");
_Static_assert(offsetof(struct riscv_clic_regs, info) == 0x0004U,
               "CLICINFO offset mismatch");
_Static_assert(offsetof(struct riscv_clic_regs, interrupts) ==
               RISCV_CLIC_INTERRUPT_OFFSET,
               "CLIC interrupt register base mismatch");

static struct riscv_clic_regs *clic_regs(const struct riscv_clic *clic)
{
    return (struct riscv_clic_regs *)clic->base;
}

static void clic_set_irq_enabled(struct riscv_clic *clic,
                                 uint32_t irq,
                                 uint32_t enabled)
{
    struct riscv_clic_regs *regs = clic_regs(clic);
    uint8_t value = regs->interrupts[irq].enable;

    /* Preserve CLICINTIE.T and any implementation-defined bits. */
    if (enabled != 0U)
    {
        value |= RISCV_CLIC_INT_ENABLE;
    }
    else
    {
        value &= ~RISCV_CLIC_INT_ENABLE;
    }
    regs->interrupts[irq].enable = value;
}

static int clic_irq_is_valid(const struct riscv_clic *clic, uint32_t irq)
{
    return (clic != NULL) && (clic->info.hw_irq_count != 0U) &&
           (irq < clic->info.hw_irq_count);
}

static int clic_trigger_is_valid(enum riscv_clic_trigger trigger)
{
    return (trigger == RISCV_CLIC_TRIGGER_HIGH_LEVEL) ||
           (trigger == RISCV_CLIC_TRIGGER_POSITIVE_EDGE) ||
           (trigger == RISCV_CLIC_TRIGGER_NEGATIVE_EDGE);
}

static int clic_level_is_valid(const struct riscv_clic *clic, uint8_t level)
{
    if (clic->info.control_bits == 0U)
    {
        return level == 0U;
    }
    if (clic->info.control_bits == RISCV_CLIC_MAX_CONTROL_BITS)
    {
        return 1;
    }

    return level < (1U << clic->info.control_bits);
}

static uint8_t clic_encode_level(const struct riscv_clic *clic, uint8_t level)
{
    if (clic->info.control_bits == 0U)
    {
        return 0U;
    }

    return level << (8U - clic->info.control_bits);
}

static void clic_set_irq_attribute(struct riscv_clic_regs *regs,
                                   uint32_t irq,
                                   enum riscv_clic_trigger trigger)
{
    uint8_t attribute = regs->interrupts[irq].attribute;

    attribute &= ~(RISCV_CLIC_ATTR_SHV | RISCV_CLIC_ATTR_TRIGGER_MASK);
    attribute |= RISCV_CLIC_ATTR_SHV |
                 (trigger << RISCV_CLIC_ATTR_TRIGGER_SHIFT);
    regs->interrupts[irq].attribute = attribute;
}

static void clic_configure_irq_locked(struct riscv_clic *clic,
                                      uint32_t irq,
                                      enum riscv_clic_trigger trigger,
                                      uint8_t level)
{
    struct riscv_clic_regs *regs = clic_regs(clic);

    clic_set_irq_enabled(clic, irq, 0U);
    /* This clears only the CLIC pending state, not a level-sensitive source. */
    regs->interrupts[irq].pending = 0U;

    clic_set_irq_attribute(regs, irq, trigger);
    regs->interrupts[irq].control = clic_encode_level(clic, level);
}

int riscv_clic_init(struct riscv_clic *clic,
                    const struct riscv_clic_config *config)
{
    struct riscv_clic_regs *regs;
    uint32_t raw_info;
    uint32_t hw_irq_count;
    uint32_t control_bits;
    uint32_t irq;
    rt_base_t previous_mstatus;
    uint8_t cliccfg;
    int result;

    if ((clic == NULL) || (config == NULL) || (config->base == 0U))
    {
        return RISCV_CLIC_ERROR_ARGUMENT;
    }

    previous_mstatus = rt_hw_interrupt_disable();
    clic->base = config->base;
    clic->info.raw = 0U;
    clic->info.hw_irq_count = 0U;
    clic->info.control_bits = 0U;
    regs = clic_regs(clic);

    raw_info = regs->info;
    hw_irq_count = raw_info & RISCV_CLIC_INFO_NUM_MASK;
    if (hw_irq_count == 0U)
    {
        hw_irq_count = config->zero_irq_count;
    }

    control_bits =
        (raw_info >> RISCV_CLIC_INFO_CONTROL_BITS_SHIFT) &
        RISCV_CLIC_INFO_CONTROL_BITS_MASK;
    if ((hw_irq_count == 0U) ||
        (hw_irq_count > RISCV_CLIC_MAX_HW_IRQS) ||
        (control_bits > RISCV_CLIC_MAX_CONTROL_BITS))
    {
        result = RISCV_CLIC_ERROR_INFO;
        goto clic_init_exit;
    }

    for (irq = 0U; irq < hw_irq_count; irq++)
    {
        clic_set_irq_enabled(clic, irq, 0U);
        regs->interrupts[irq].pending = 0U;
    }

    cliccfg = regs->config & ~RISCV_CLIC_CONFIG_NLBITS_MASK;
    cliccfg |= (control_bits << RISCV_CLIC_CONFIG_NLBITS_SHIFT) &
               RISCV_CLIC_CONFIG_NLBITS_MASK;
    regs->config = cliccfg;
    if (config->threshold_addr != 0U)
    {
        *(volatile uint32_t *)config->threshold_addr = 0U;
    }

    for (irq = 0U; irq < hw_irq_count; irq++)
    {
        clic_set_irq_attribute(regs,
                               irq,
                               RISCV_CLIC_TRIGGER_HIGH_LEVEL);
        regs->interrupts[irq].control = 0U;
    }

    clic->info.raw = raw_info;
    clic->info.hw_irq_count = hw_irq_count;
    clic->info.control_bits = (uint8_t)control_bits;
    result = RISCV_CLIC_OK;

clic_init_exit:
    rt_hw_interrupt_enable(previous_mstatus);
    return result;
}

int riscv_clic_configure_irq(struct riscv_clic *clic,
                             uint32_t irq,
                             enum riscv_clic_trigger trigger,
                             uint8_t level)
{
    rt_base_t previous_mstatus;

    if (!clic_irq_is_valid(clic, irq) ||
        !clic_trigger_is_valid(trigger) ||
        !clic_level_is_valid(clic, level))
    {
        return RISCV_CLIC_ERROR_ARGUMENT;
    }

    previous_mstatus = rt_hw_interrupt_disable();
    clic_configure_irq_locked(clic, irq, trigger, level);
    rt_hw_interrupt_enable(previous_mstatus);

    return RISCV_CLIC_OK;
}

void riscv_clic_enable_irq(struct riscv_clic *clic, uint32_t irq)
{
    clic_set_irq_enabled(clic, irq, 1U);
}

void riscv_clic_disable_irq(struct riscv_clic *clic, uint32_t irq)
{
    clic_set_irq_enabled(clic, irq, 0U);
}

void riscv_clic_set_pending(struct riscv_clic *clic, uint32_t irq)
{
    clic_regs(clic)->interrupts[irq].pending = 1U;
}

void riscv_clic_clear_pending(struct riscv_clic *clic, uint32_t irq)
{
    clic_regs(clic)->interrupts[irq].pending = 0U;
}

uint32_t riscv_clic_get_pending(struct riscv_clic *clic, uint32_t irq)
{
    return clic_regs(clic)->interrupts[irq].pending & 1U;
}

const struct riscv_clic_info *riscv_clic_get_info(
    const struct riscv_clic *clic)
{
    if ((clic == NULL) || (clic->info.hw_irq_count == 0U))
    {
        return NULL;
    }

    return &clic->info;
}
