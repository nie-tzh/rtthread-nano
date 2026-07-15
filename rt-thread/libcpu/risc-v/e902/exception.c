#include "e902_exception.h"

#if defined(CONFIG_E902_EXCEPTION_SELF_TEST)
#define E902_C_EBREAK_ENCODING  0x00009002U
#define E902_EBREAK_ENCODING    0x00100073U
#endif

volatile struct e902_exception_frame g_e902_last_exception;
volatile uint32_t g_e902_exception_count;
volatile uint32_t g_e902_exception_active;
volatile uint32_t g_e902_exception_halted;
volatile uint32_t g_e902_exception_output_ready;

__attribute__((weak)) void e902_exception_putchar(char ch)
{
    (void)ch;
}

void e902_exception_set_output_ready(int ready)
{
    g_e902_exception_output_ready = (ready != 0) ? 1U : 0U;
}

static void exception_putc(char ch)
{
    if (g_e902_exception_output_ready != 0U)
    {
        e902_exception_putchar(ch);
    }
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
        exception_putc(digits[(value >> (uint32_t)shift) & 0xFU]);
    }
}

static void exception_put_register(const char *name, uint32_t value)
{
    exception_puts(name);
    exception_putc('=');
    exception_put_hex32(value);
    exception_putc(' ');
}

static void exception_record_frame(const struct e902_exception_frame *frame)
{
    volatile unsigned char *destination =
        (volatile unsigned char *)&g_e902_last_exception;
    const unsigned char *source = (const unsigned char *)frame;
    size_t index;

    for (index = 0U; index < sizeof(*frame); index++)
    {
        destination[index] = source[index];
    }
}

static void exception_report(const struct e902_exception_frame *frame)
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

#if defined(CONFIG_E902_EXCEPTION_SELF_TEST)
static uint32_t breakpoint_instruction_length(uint32_t address)
{
    const volatile uint16_t *instruction =
        (const volatile uint16_t *)(uintptr_t)address;
    uint32_t low_halfword;
    uint32_t encoding;

    if ((address & 1U) != 0U)
    {
        return 0U;
    }

    low_halfword = instruction[0];
    if (low_halfword == E902_C_EBREAK_ENCODING)
    {
        return 2U;
    }

    if ((low_halfword & 0x3U) != 0x3U)
    {
        return 0U;
    }

    encoding = low_halfword | ((uint32_t)instruction[1] << 16);
    if (encoding == E902_EBREAK_ENCODING)
    {
        return 4U;
    }

    return 0U;
}
#endif

uint32_t e902_exception_dispatch(struct e902_exception_frame *frame)
{
    uint32_t exception_code;
#if defined(CONFIG_E902_EXCEPTION_SELF_TEST)
    uint32_t instruction_length;
#endif

    if (g_e902_exception_active != 0U)
    {
        g_e902_exception_count++;
        g_e902_exception_output_ready = 0U;
        g_e902_exception_halted = 1U;
        return 0U;
    }

    g_e902_exception_active = 1U;
    g_e902_exception_count++;
    exception_record_frame(frame);
    exception_report(frame);

    if ((frame->mcause & E902_MCAUSE_INTERRUPT_MASK) != 0U)
    {
        exception_puts("action=halt reason=unexpected-interrupt\n");
        g_e902_exception_halted = 1U;
        return 0U;
    }

    exception_code = frame->mcause & E902_MCAUSE_CODE_MASK;
#if defined(CONFIG_E902_EXCEPTION_SELF_TEST)
    if (exception_code == E902_EXCEPTION_BREAKPOINT)
    {
        instruction_length = breakpoint_instruction_length(frame->mepc);
        if (instruction_length != 0U)
        {
            frame->mepc += instruction_length;
            exception_puts("action=resume reason=ebreak next_mepc=");
            exception_put_hex32(frame->mepc);
            exception_putc('\n');
            g_e902_exception_active = 0U;
            return 1U;
        }
    }
#else
    (void)exception_code;
#endif

    exception_puts("action=halt reason=unrecoverable-exception\n");
    g_e902_exception_halted = 1U;
    return 0U;
}
