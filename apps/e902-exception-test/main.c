#include <stdint.h>

#include "board.h"
#include "e902_exception.h"

extern uint32_t e902_exception_register_self_test(void);
extern const uint8_t e902_exception_self_test_ebreak[];

volatile uint32_t g_exception_test_stage;
volatile uint32_t g_exception_register_test_result;

static void run_exception_self_test(void)
{
    g_exception_test_stage = 1U;
    (void)board_early_puts("E902 exception self-test: trigger ebreak\n");
    g_exception_register_test_result =
        e902_exception_register_self_test();

    g_exception_test_stage = 2U;
    (void)board_early_puts("E902 exception self-test: resumed\n");

    if ((g_exception_register_test_result == 0U) &&
        (g_e902_exception_count == 1U) &&
        (g_e902_exception_halted == 0U) &&
        ((g_e902_last_exception.mcause & E902_MCAUSE_INTERRUPT_MASK) == 0U) &&
        ((g_e902_last_exception.mcause & E902_MCAUSE_CODE_MASK) ==
         E902_EXCEPTION_BREAKPOINT) &&
        (g_e902_last_exception.mepc ==
         (uint32_t)(uintptr_t)e902_exception_self_test_ebreak))
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
    run_exception_self_test();

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
