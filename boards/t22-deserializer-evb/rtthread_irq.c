#include <stdint.h>

#include <rthw.h>
#include <rtthread.h>

#include "board.h"
#include "e902.h"
#include "e902_irq.h"
#include "riscv_clic.h"
#include "t22_serdes_irq.h"

#define T22_RT_IRQ_VECTOR_COUNT  T22_SERDES_IRQ_VECTOR_COUNT
#define T22_RT_CONTEXT_IRQ       T22_SERDES_IRQ_MACHINE_SOFTWARE

static struct rt_irq_desc irq_descs[T22_RT_IRQ_VECTOR_COUNT];
static struct riscv_clic *t22_rt_clic;

static int t22_rt_irq_is_reserved(uint32_t irq)
{
    return irq == T22_RT_CONTEXT_IRQ;
}

static void t22_rt_default_irq_handler(int vector, void *parameter)
{
    (void)parameter;
    riscv_clic_disable_irq(t22_rt_clic, (uint32_t)vector);
}

void rt_hw_interrupt_dispatch(int vector)
{
    struct rt_irq_desc *desc = &irq_descs[(uint32_t)vector];

#ifdef RT_USING_INTERRUPT_INFO
    desc->counter++;
#endif
    desc->handler(vector, desc->param);
}

static void t22_rt_interrupt_halt(const char *reason)
{
    (void)board_early_puts(reason);
    (void)rt_hw_interrupt_disable();

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

void rt_hw_interrupt_init(void)
{
    uint32_t irq;
    int result;

    for (irq = 0U; irq < T22_RT_IRQ_VECTOR_COUNT; irq++)
    {
        irq_descs[irq].handler = t22_rt_default_irq_handler;
        irq_descs[irq].param = RT_NULL;
#ifdef RT_USING_INTERRUPT_INFO
        rt_snprintf(irq_descs[irq].name, RT_NAME_MAX, "default");
        irq_descs[irq].counter = 0U;
#endif
    }

    result = t22_serdes_irq_init();
    if (result != RISCV_CLIC_OK)
    {
        t22_rt_interrupt_halt("RT-Thread interrupt init failed\n");
    }
    t22_rt_clic = t22_serdes_clic();

    e902_context_switch_init();
}

void rt_hw_interrupt_mask(int vector)
{
    if ((vector >= 0) &&
        ((uint32_t)vector < T22_RT_IRQ_VECTOR_COUNT) &&
        !t22_rt_irq_is_reserved((uint32_t)vector))
    {
        riscv_clic_disable_irq(t22_rt_clic, (uint32_t)vector);
    }
}

void rt_hw_interrupt_umask(int vector)
{
    if ((vector >= 0) &&
        ((uint32_t)vector < T22_RT_IRQ_VECTOR_COUNT) &&
        !t22_rt_irq_is_reserved((uint32_t)vector))
    {
        riscv_clic_enable_irq(t22_rt_clic, (uint32_t)vector);
    }
}

rt_isr_handler_t rt_hw_interrupt_install(int vector,
                                          rt_isr_handler_t handler,
                                          void *parameter,
                                          const char *name)
{
    struct rt_irq_desc *desc;
    rt_isr_handler_t old_handler;
    rt_base_t previous_mstatus;

#ifndef RT_USING_INTERRUPT_INFO
    (void)name;
#endif
    if ((vector < 0) ||
        ((uint32_t)vector >= T22_RT_IRQ_VECTOR_COUNT) ||
        t22_rt_irq_is_reserved((uint32_t)vector))
    {
        return RT_NULL;
    }

    previous_mstatus = rt_hw_interrupt_disable();
    desc = &irq_descs[(uint32_t)vector];
    old_handler = desc->handler;
    if (handler != RT_NULL)
    {
        desc->handler = handler;
        desc->param = parameter;
#ifdef RT_USING_INTERRUPT_INFO
        rt_snprintf(desc->name, RT_NAME_MAX, "%s", name);
        desc->counter = 0U;
#endif
    }
    rt_hw_interrupt_enable(previous_mstatus);

    return old_handler;
}
