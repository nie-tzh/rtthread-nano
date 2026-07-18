#include <stddef.h>
#include <stdint.h>

#include "e902_clic.h"
#include "e902_context.h"

#define E902_CONTEXT_INTERRUPT_CONTROL  0xFFU

extern char __global_pointer$;

volatile e902_ubase_t rt_interrupt_from_thread;
volatile e902_ubase_t rt_interrupt_to_thread;
volatile e902_ubase_t rt_thread_switch_interrupt_flag;

volatile uint32_t g_e902_context_request_count;
volatile uint32_t g_e902_context_irq_count;
volatile uint32_t g_e902_context_switch_count;
volatile int32_t g_e902_context_last_error;
volatile uint32_t g_e902_context_halted;

static uint32_t context_initialized;

static void context_halt(int32_t error) __attribute__((noreturn));

static void context_halt(int32_t error)
{
    g_e902_context_last_error = error;
    g_e902_context_halted = 1U;
    (void)e902_global_irq_disable();

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

static void context_switch_irq_handler(
    uint32_t irq,
    void *parameter,
    const struct e902_exception_frame *frame)
{
    (void)parameter;
    (void)frame;

    if (irq != E902_CONTEXT_SWITCH_IRQ)
    {
        context_halt(E902_CONTEXT_ERROR_INTERRUPT);
    }

    g_e902_context_irq_count++;
    if (e902_clic_clear_pending(E902_CONTEXT_SWITCH_IRQ) != E902_CLIC_OK)
    {
        context_halt(E902_CONTEXT_ERROR_INTERRUPT);
    }
}

int e902_context_switch_init(void)
{
    uint32_t previous_mstatus;
    int result;

    previous_mstatus = e902_global_irq_disable();
    if (context_initialized != 0U)
    {
        e902_global_irq_restore(previous_mstatus);
        return E902_CONTEXT_OK;
    }

    rt_interrupt_from_thread = 0UL;
    rt_interrupt_to_thread = 0UL;
    rt_thread_switch_interrupt_flag = 0UL;
    g_e902_context_request_count = 0U;
    g_e902_context_irq_count = 0U;
    g_e902_context_switch_count = 0U;
    g_e902_context_last_error = 0U;
    g_e902_context_halted = 0U;

    result = e902_clic_register_irq(
        E902_CONTEXT_SWITCH_IRQ,
        context_switch_irq_handler,
        0,
        E902_CLIC_TRIGGER_POSITIVE_EDGE,
        E902_CONTEXT_INTERRUPT_CONTROL);
    if (result != E902_CLIC_OK)
    {
        g_e902_context_last_error = result;
        e902_global_irq_restore(previous_mstatus);
        return E902_CONTEXT_ERROR_INTERRUPT;
    }

    result = e902_clic_clear_pending(E902_CONTEXT_SWITCH_IRQ);
    if (result == E902_CLIC_OK)
    {
        result = e902_clic_enable_irq(E902_CONTEXT_SWITCH_IRQ);
    }
    if (result != E902_CLIC_OK)
    {
        g_e902_context_last_error = result;
        (void)e902_clic_disable_irq(E902_CONTEXT_SWITCH_IRQ);
        e902_global_irq_restore(previous_mstatus);
        return E902_CONTEXT_ERROR_INTERRUPT;
    }

    context_initialized = 1U;
    e902_global_irq_restore(previous_mstatus);
    return E902_CONTEXT_OK;
}

uint8_t *rt_hw_stack_init(void *entry,
                          void *parameter,
                          uint8_t *stack_addr,
                          void *exit)
{
    e902_context_frame_t *frame;
    e902_ubase_t stack_top;
    uint32_t *word;
    uint32_t index;

    if ((entry == 0) || (stack_addr == 0) || (exit == 0))
    {
        return 0;
    }

    stack_top = (e902_ubase_t)stack_addr + sizeof(e902_ubase_t);
    stack_top &= ~((e902_ubase_t)E902_CONTEXT_STACK_ALIGNMENT - 1UL);
    frame = (e902_context_frame_t *)(
        stack_top - sizeof(e902_context_frame_t));

    word = (uint32_t *)frame;
    for (index = 0U;
         index < (sizeof(e902_context_frame_t) / sizeof(uint32_t));
         index++)
    {
        word[index] = E902_CONTEXT_REGISTER_FILL;
    }

    frame->ra = (uint32_t)(e902_ubase_t)exit;
    frame->sp = (uint32_t)stack_top;
    frame->gp = (uint32_t)(e902_ubase_t)&__global_pointer$;
    frame->tp = 0U;
    frame->a0 = (uint32_t)(e902_ubase_t)parameter;
    frame->mepc = (uint32_t)(e902_ubase_t)entry;
    frame->mstatus = E902_CONTEXT_INITIAL_MSTATUS;
    frame->mcause = 0U;
    frame->mtval = 0U;
    frame->reserved = 0U;

    return (uint8_t *)frame;
}

static void context_switch_request(e902_ubase_t from, e902_ubase_t to)
{
    uint32_t previous_mstatus;

    previous_mstatus = e902_global_irq_disable();
    if ((context_initialized == 0U) || (from == 0UL) || (to == 0UL))
    {
        context_halt(E902_CONTEXT_ERROR_STATE);
    }
    if (from == to)
    {
        e902_global_irq_restore(previous_mstatus);
        return;
    }

    if (rt_thread_switch_interrupt_flag == 0UL)
    {
        rt_interrupt_from_thread = from;
    }
    rt_interrupt_to_thread = to;
    rt_thread_switch_interrupt_flag = 1UL;
    g_e902_context_request_count++;

    if (e902_clic_set_pending(E902_CONTEXT_SWITCH_IRQ) != E902_CLIC_OK)
    {
        context_halt(E902_CONTEXT_ERROR_INTERRUPT);
    }

    e902_global_irq_restore(previous_mstatus);
}

void rt_hw_context_switch(e902_ubase_t from, e902_ubase_t to)
{
    context_switch_request(from, to);
}

void rt_hw_context_switch_interrupt(e902_ubase_t from, e902_ubase_t to)
{
    context_switch_request(from, to);
}
