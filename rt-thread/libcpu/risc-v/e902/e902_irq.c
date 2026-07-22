#include <stddef.h>
#include <rthw.h>

#include "e902.h"
#include "e902_irq.h"

#define E902_MTVT_ALIGNMENT             64U
#define E902_CLIC_MIN_CONTROL_BITS      2U
#define E902_CLIC_MAX_CONTROL_BITS      5U

static struct riscv_clic *active_clic;

int e902_irq_init(struct riscv_clic *clic,
                  const struct riscv_clic_config *clic_config,
                  const uintptr_t *vector_table,
                  uint32_t vector_count)
{
    const struct riscv_clic_info *info;
    rt_base_t previous_mstatus;
    int result;

    if ((clic_config == NULL) || (vector_table == NULL) ||
        (vector_count == 0U) ||
        ((((uintptr_t)vector_table) & (E902_MTVT_ALIGNMENT - 1U)) != 0U))
    {
        return RISCV_CLIC_ERROR_ARGUMENT;
    }

    previous_mstatus = rt_hw_interrupt_disable();
    active_clic = NULL;

    result = riscv_clic_init(clic, clic_config);
    if (result != RISCV_CLIC_OK)
    {
        goto e902_irq_init_exit;
    }

    info = riscv_clic_get_info(clic);
    if (info->hw_irq_count > vector_count)
    {
        result = RISCV_CLIC_ERROR_CAPACITY;
        goto e902_irq_init_exit;
    }
    if ((info->control_bits < E902_CLIC_MIN_CONTROL_BITS) ||
        (info->control_bits > E902_CLIC_MAX_CONTROL_BITS))
    {
        result = RISCV_CLIC_ERROR_INFO;
        goto e902_irq_init_exit;
    }

    __asm__ volatile ("csrw mtvt, %0"
                      :
                      : "r"((uint32_t)(uintptr_t)vector_table)
                      : "memory");
    active_clic = clic;

e902_irq_init_exit:
    rt_hw_interrupt_enable(previous_mstatus);
    return result;
}

struct riscv_clic *e902_irq_clic(void)
{
    return active_clic;
}

void e902_irq_dispatch(struct e902_frame *frame)
{
    uint32_t irq = frame->mcause & E902_MCAUSE_CODE_MASK;

    rt_hw_interrupt_dispatch((int)irq);
}
