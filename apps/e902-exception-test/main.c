#include <stdint.h>

#include <rthw.h>

#include "board.h"
#include "e902.h"

#define E902_C_EBREAK_ENCODING  0x00009002U
#define E902_EBREAK_ENCODING    0x00100073U
#define E902_BREAKPOINT_CAUSE   3U

extern uint32_t e902_exception_register_self_test(void);
extern const uint8_t e902_exception_self_test_ebreak[];

volatile uint32_t g_exception_test_stage;
volatile uint32_t g_exception_register_test_result;

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
    return (encoding == E902_EBREAK_ENCODING) ? 4U : 0U;
}

static rt_err_t exception_test_hook(void *context)
{
    struct e902_frame *frame = context;
    uint32_t instruction_length;

    if ((frame == RT_NULL) ||
        ((frame->mcause & E902_MCAUSE_INTERRUPT_MASK) != 0U) ||
        ((frame->mcause & E902_MCAUSE_CODE_MASK) !=
         E902_BREAKPOINT_CAUSE) ||
        (frame->mepc !=
         (uint32_t)(uintptr_t)e902_exception_self_test_ebreak))
    {
        return -RT_ERROR;
    }

    instruction_length = breakpoint_instruction_length(frame->mepc);
    if (instruction_length == 0U)
    {
        return -RT_ERROR;
    }

    frame->mepc += instruction_length;
    return RT_EOK;
}

static void run_exception_self_test(void)
{
    rt_hw_exception_install(exception_test_hook);
    g_exception_test_stage = 1U;
    (void)board_early_puts("E902 exception self-test: trigger ebreak\n");
    g_exception_register_test_result =
        e902_exception_register_self_test();

    g_exception_test_stage = 2U;
    rt_hw_exception_install(RT_NULL);
    (void)board_early_puts("E902 exception self-test: resumed\n");

    if (g_exception_register_test_result == 0U)
    {
        (void)board_early_puts("E902 exception self-test: PASS\n");
    }
    else
    {
        (void)board_early_puts("E902 exception self-test: FAIL\n");
    }
}

int main(void)
{
    (void)board_early_puts("T22 deserializer EVB booting...\n");
    (void)rt_hw_interrupt_disable();
    rt_hw_interrupt_init();
    run_exception_self_test();

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
