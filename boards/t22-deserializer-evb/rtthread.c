#include <stdint.h>

#include <rthw.h>
#include <rtthread.h>

#include "board.h"
#include "e902_clic.h"
#include "e902_context.h"
#include "t22_serdes_irq.h"

#define T22_RT_IRQ_VECTOR_COUNT  T22_SERDES_IRQ_VECTOR_COUNT
#define T22_RT_CONTEXT_IRQ       T22_SERDES_IRQ_MACHINE_SOFTWARE
#define T22_RT_TIMER_IRQ         T22_SERDES_IRQ_DW_TIMER
#define T22_RT_IRQ_CONTROL       0xFFU

struct t22_rt_irq_slot
{
    rt_isr_handler_t handler;
    void *parameter;
};

static struct t22_rt_irq_slot t22_rt_irq_slots[T22_RT_IRQ_VECTOR_COUNT];
static uint32_t t22_rt_interrupt_initialized;

static int t22_rt_irq_is_reserved(uint32_t irq)
{
    return (irq == T22_RT_CONTEXT_IRQ) || (irq == T22_RT_TIMER_IRQ);
}

static void t22_rt_default_irq_handler(int vector, void *parameter)
{
    (void)parameter;
    (void)e902_clic_disable_irq((uint32_t)vector);
}

static void t22_rt_irq_adapter(uint32_t irq,
                               void *parameter,
                               const struct e902_exception_frame *frame)
{
    struct t22_rt_irq_slot *slot = parameter;

    (void)frame;
    if ((slot == 0) || (slot->handler == 0))
    {
        (void)e902_clic_disable_irq(irq);
        return;
    }

    slot->handler((int)irq, slot->parameter);
}

static void t22_rt_interrupt_halt(const char *reason)
{
    (void)board_early_puts(reason);
    (void)e902_global_irq_disable();

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

void rt_hw_interrupt_init(void)
{
    uint32_t irq;
    int result;

    if (t22_rt_interrupt_initialized != 0U)
    {
        return;
    }

    for (irq = 0U; irq < T22_RT_IRQ_VECTOR_COUNT; irq++)
    {
        t22_rt_irq_slots[irq].handler = t22_rt_default_irq_handler;
        t22_rt_irq_slots[irq].parameter = 0;
    }

    result = t22_serdes_irq_init();
    if (result != E902_CLIC_OK)
    {
        t22_rt_interrupt_halt("RT-Thread interrupt init failed\n");
    }

    result = e902_context_switch_init();
    if (result != E902_CONTEXT_OK)
    {
        t22_rt_interrupt_halt("RT-Thread context init failed\n");
    }

    t22_rt_interrupt_initialized = 1U;
}

void rt_hw_interrupt_mask(int vector)
{
    if ((vector >= 0) &&
        ((uint32_t)vector < T22_RT_IRQ_VECTOR_COUNT))
    {
        (void)e902_clic_disable_irq((uint32_t)vector);
    }
}

void rt_hw_interrupt_umask(int vector)
{
    if ((vector >= 0) &&
        ((uint32_t)vector < T22_RT_IRQ_VECTOR_COUNT))
    {
        (void)e902_clic_enable_irq((uint32_t)vector);
    }
}

rt_isr_handler_t rt_hw_interrupt_install(int vector,
                                          rt_isr_handler_t handler,
                                          void *parameter,
                                          const char *name)
{
    struct t22_rt_irq_slot *slot;
    rt_isr_handler_t old_handler;
    void *old_parameter;
    int result;

    (void)name;
    if ((vector < 0) ||
        ((uint32_t)vector >= T22_RT_IRQ_VECTOR_COUNT) ||
        (handler == 0) ||
        t22_rt_irq_is_reserved((uint32_t)vector))
    {
        return 0;
    }

    slot = &t22_rt_irq_slots[(uint32_t)vector];
    old_handler = slot->handler;
    old_parameter = slot->parameter;
    slot->handler = handler;
    slot->parameter = parameter;
    result = e902_clic_register_irq(
        (uint32_t)vector,
        t22_rt_irq_adapter,
        slot,
        E902_CLIC_TRIGGER_HIGH_LEVEL,
        T22_RT_IRQ_CONTROL);
    if (result != E902_CLIC_OK)
    {
        slot->handler = old_handler;
        slot->parameter = old_parameter;
    }

    return old_handler;
}
