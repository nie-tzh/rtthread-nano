#include <stddef.h>
#include <stdint.h>
#include <rthw.h>

#include "e902.h"
#include "e902_irq.h"
#include "riscv_clic.h"

#define E902_CONTEXT_SWITCH_IRQ       3U
#define E902_CONTEXT_INITIAL_MSTATUS \
    (E902_MSTATUS_MPP_MACHINE | E902_MSTATUS_MPIE_MASK)
#define E902_CONTEXT_REGISTER_FILL    0xDEADBEEFU

extern char __global_pointer$;

_Static_assert(sizeof(rt_ubase_t) == sizeof(uint32_t),
               "E902 context requires a 32-bit base type");
_Static_assert((sizeof(struct e902_frame) % RT_ALIGN_SIZE) == 0U,
               "E902 frame breaks RT-Thread stack alignment");

uint32_t e902_exception_dispatch(struct e902_frame *frame);

volatile rt_ubase_t rt_interrupt_from_thread;
volatile rt_ubase_t rt_interrupt_to_thread;
volatile rt_uint32_t rt_thread_switch_interrupt_flag;

static volatile uint32_t exception_active;
static rt_err_t (*rt_exception_hook)(void *context) = RT_NULL;
static struct riscv_clic *context_switch_clic;

void e902_context_switch_init(void)
{
    rt_base_t previous_mstatus;

    previous_mstatus = rt_hw_interrupt_disable();
    rt_interrupt_from_thread = 0UL;
    rt_interrupt_to_thread = 0UL;
    rt_thread_switch_interrupt_flag = 0UL;
    context_switch_clic = e902_irq_clic();
    riscv_clic_configure_irq(
        context_switch_clic,
        E902_CONTEXT_SWITCH_IRQ,
        RISCV_CLIC_TRIGGER_POSITIVE_EDGE,
        E902_IRQ_LEVEL0);
    riscv_clic_enable_irq(context_switch_clic,
                          E902_CONTEXT_SWITCH_IRQ);

    rt_hw_interrupt_enable(previous_mstatus);
}

uint8_t *rt_hw_stack_init(void *entry,
                          void *parameter,
                          uint8_t *stack_addr,
                          void *exit)
{
    struct e902_frame *frame;
    rt_ubase_t stack_top;
    uint32_t *word;
    uint32_t index;

    stack_top = (rt_ubase_t)stack_addr + sizeof(rt_ubase_t);
    stack_top &= ~((rt_ubase_t)RT_ALIGN_SIZE - 1UL);
    frame = (struct e902_frame *)(
        stack_top - sizeof(struct e902_frame));

    word = (uint32_t *)frame;
    for (index = 0U;
         index < (sizeof(struct e902_frame) / sizeof(uint32_t));
         index++)
    {
        word[index] = E902_CONTEXT_REGISTER_FILL;
    }

    frame->ra = (uint32_t)(rt_ubase_t)exit;
    frame->sp = (uint32_t)stack_top;
    frame->gp = (uint32_t)(rt_ubase_t)&__global_pointer$;
    frame->tp = 0U;
    frame->a0 = (uint32_t)(rt_ubase_t)parameter;
    frame->mepc = (uint32_t)(rt_ubase_t)entry;
    frame->mstatus = E902_CONTEXT_INITIAL_MSTATUS;
    frame->mcause = 0U;
    frame->mtval = 0U;
    frame->reserved = 0U;

    return (uint8_t *)frame;
}

static void context_switch_request(rt_ubase_t from, rt_ubase_t to)
{
    rt_base_t previous_mstatus;

    previous_mstatus = rt_hw_interrupt_disable();
    if (rt_thread_switch_interrupt_flag == 0UL)
    {
        rt_interrupt_from_thread = from;
    }
    rt_interrupt_to_thread = to;
    rt_thread_switch_interrupt_flag = 1UL;
    riscv_clic_set_pending(context_switch_clic,
                           E902_CONTEXT_SWITCH_IRQ);

    rt_hw_interrupt_enable(previous_mstatus);
}

void rt_hw_context_switch_to(rt_ubase_t to)
{
    rt_base_t previous_mstatus;

    previous_mstatus = rt_hw_interrupt_disable();
    /* The first thread has no saved context; IRQ 3 performs its first mret. */
    rt_interrupt_from_thread = 0UL;
    rt_interrupt_to_thread = to;
    rt_thread_switch_interrupt_flag = 1UL;

    riscv_clic_set_pending(context_switch_clic,
                           E902_CONTEXT_SWITCH_IRQ);

    rt_hw_interrupt_enable(previous_mstatus | E902_MSTATUS_MIE_MASK);

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to)
{
    context_switch_request(from, to);
}

void rt_hw_context_switch_interrupt(rt_ubase_t from, rt_ubase_t to)
{
    context_switch_request(from, to);
}

__attribute__((weak)) void e902_exception_putchar(char ch)
{
    (void)ch;
}

void rt_hw_exception_install(rt_err_t (*exception_handle)(void *context))
{
    rt_exception_hook = exception_handle;
}

static void exception_putc(char ch)
{
    e902_exception_putchar(ch);
}

static void exception_puts(const char *text)
{
    while (*text != '\0')
    {
        exception_putc(*text++);
    }
}

static void exception_put_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    exception_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        exception_putc(digits[(value >> shift) & 0xFU]);
    }
}

static void exception_put_register(const char *name, uint32_t value)
{
    exception_puts(name);
    exception_putc('=');
    exception_put_hex32(value);
    exception_putc(' ');
}

static void exception_report(const struct e902_frame *frame)
{
    exception_puts("\n[E902 exception]\n");
    exception_put_register("mcause", frame->mcause);
    exception_put_register("mepc", frame->mepc);
    exception_put_register("mtval", frame->mtval);
    exception_put_register("mstatus", frame->mstatus);
    exception_putc('\n');

    exception_put_register("x1/ra", frame->ra);
    exception_put_register("x2/sp", frame->sp);
    exception_put_register("x3/gp", frame->gp);
    exception_put_register("x4/tp", frame->tp);
    exception_putc('\n');

    exception_put_register("x5/t0", frame->t0);
    exception_put_register("x6/t1", frame->t1);
    exception_put_register("x7/t2", frame->t2);
    exception_put_register("x8/s0", frame->s0);
    exception_put_register("x9/s1", frame->s1);
    exception_putc('\n');

    exception_put_register("x10/a0", frame->a0);
    exception_put_register("x11/a1", frame->a1);
    exception_put_register("x12/a2", frame->a2);
    exception_put_register("x13/a3", frame->a3);
    exception_put_register("x14/a4", frame->a4);
    exception_put_register("x15/a5", frame->a5);
    exception_putc('\n');
}

uint32_t e902_exception_dispatch(struct e902_frame *frame)
{
    if (exception_active != 0U)
    {
        return 0U;
    }

    exception_active = 1U;
    if (rt_exception_hook != RT_NULL)
    {
        if (rt_exception_hook(frame) == RT_EOK)
        {
            exception_active = 0U;
            return 1U;
        }
    }

    exception_report(frame);
    exception_puts("action=halt reason=unrecoverable-exception\n");
    return 0U;
}
