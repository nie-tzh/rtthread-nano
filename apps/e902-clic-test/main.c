#include <stdint.h>

#include "board.h"
#include "e902_clic.h"
#include "t22_serdes_irq.h"

#define CLIC_TEST_TIMEOUT              1000000U
#define CLIC_TEST_PARAMETER_VALUE      0xC11C0003U
#define CLIC_TEST_MIN_CONTROL_BITS     2U
#define CLIC_TEST_MAX_CONTROL_BITS     5U

#define CLIC_TEST_OK                   0U
#define CLIC_TEST_FAIL_INIT            1U
#define CLIC_TEST_FAIL_INFO            2U
#define CLIC_TEST_FAIL_REGISTER        3U
#define CLIC_TEST_FAIL_PENDING_CLEAR   4U
#define CLIC_TEST_FAIL_ENABLE          5U
#define CLIC_TEST_FAIL_PENDING_SET     6U
#define CLIC_TEST_FAIL_PENDING_LATCH   7U
#define CLIC_TEST_FAIL_TIMEOUT         8U
#define CLIC_TEST_FAIL_RESULT          9U

volatile uint32_t g_clic_test_stage;
volatile uint32_t g_clic_test_handler_count;
volatile uint32_t g_clic_test_handler_irq;
volatile uint32_t g_clic_test_handler_mcause;
volatile uint32_t g_clic_test_parameter_valid;
volatile uint32_t g_clic_test_pending_before_enable;
volatile uint32_t g_clic_test_pending_on_entry;
volatile uint32_t g_clic_test_pending_after_clear;
volatile uint32_t g_clic_test_result;
volatile int32_t g_clic_test_init_result;

static uint32_t clic_test_parameter = CLIC_TEST_PARAMETER_VALUE;

static void put_hex32(uint32_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    int shift;

    (void)board_early_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4)
    {
        (void)board_early_putc(
            digits[(value >> (uint32_t)shift) & 0xFU]);
    }
}

static void software_irq_handler(
    uint32_t irq,
    void *parameter,
    const struct e902_exception_frame *frame)
{
    uint32_t pending = 0xFFFFFFFFU;

    g_clic_test_handler_count++;
    g_clic_test_handler_irq = irq;
    g_clic_test_handler_mcause = frame->mcause;
    g_clic_test_parameter_valid =
        ((parameter == &clic_test_parameter) &&
         (*(const uint32_t *)parameter == CLIC_TEST_PARAMETER_VALUE)) ? 1U : 0U;

    if (e902_clic_get_pending(irq, &pending) != E902_CLIC_OK)
    {
        pending = 0xFFFFFFFFU;
    }
    g_clic_test_pending_on_entry = pending;

    (void)e902_clic_clear_pending(irq);
    pending = 0xFFFFFFFFU;
    if (e902_clic_get_pending(irq, &pending) != E902_CLIC_OK)
    {
        pending = 0xFFFFFFFFU;
    }
    g_clic_test_pending_after_clear = pending;
    g_clic_test_stage = 2U;
}

static uint32_t run_clic_self_test(void)
{
    const struct e902_clic_info *info;
    uint32_t pending;
    uint32_t timeout;
    int result;

    result = t22_serdes_irq_init();
    g_clic_test_init_result = result;
    if (result != E902_CLIC_OK)
    {
        return CLIC_TEST_FAIL_INIT;
    }

    info = e902_clic_get_info();
    if ((info == 0) ||
        (info->hardware_irq_count <= T22_SERDES_IRQ_MACHINE_SOFTWARE) ||
        (info->vector_count != T22_SERDES_IRQ_VECTOR_COUNT) ||
        (info->control_bits < CLIC_TEST_MIN_CONTROL_BITS) ||
        (info->control_bits > CLIC_TEST_MAX_CONTROL_BITS))
    {
        return CLIC_TEST_FAIL_INFO;
    }

    (void)board_early_puts("E902 CLIC self-test: CLICINFO=");
    put_hex32(info->raw);
    (void)board_early_puts(" irq_count=");
    put_hex32(info->hardware_irq_count);
    (void)board_early_puts(" ctlbits=");
    put_hex32(info->control_bits);
    (void)board_early_puts("\n");

    result = e902_clic_register_irq(
        T22_SERDES_IRQ_MACHINE_SOFTWARE,
        software_irq_handler,
        &clic_test_parameter,
        E902_CLIC_TRIGGER_POSITIVE_EDGE,
        0xFFU);
    if (result != E902_CLIC_OK)
    {
        return CLIC_TEST_FAIL_REGISTER;
    }

    result = e902_clic_clear_pending(T22_SERDES_IRQ_MACHINE_SOFTWARE);
    pending = 1U;
    if ((result != E902_CLIC_OK) ||
        (e902_clic_get_pending(T22_SERDES_IRQ_MACHINE_SOFTWARE,
                               &pending) != E902_CLIC_OK) ||
        (pending != 0U))
    {
        return CLIC_TEST_FAIL_PENDING_CLEAR;
    }

    if (e902_clic_enable_irq(T22_SERDES_IRQ_MACHINE_SOFTWARE) !=
        E902_CLIC_OK)
    {
        return CLIC_TEST_FAIL_ENABLE;
    }

    g_clic_test_stage = 1U;
    (void)board_early_puts("E902 CLIC self-test: trigger IRQ 3\n");

    if (e902_clic_set_pending(T22_SERDES_IRQ_MACHINE_SOFTWARE) !=
        E902_CLIC_OK)
    {
        return CLIC_TEST_FAIL_PENDING_SET;
    }

    pending = 0U;
    if ((e902_clic_get_pending(T22_SERDES_IRQ_MACHINE_SOFTWARE,
                               &pending) != E902_CLIC_OK) ||
        (pending != 1U))
    {
        return CLIC_TEST_FAIL_PENDING_LATCH;
    }
    g_clic_test_pending_before_enable = pending;

    e902_global_irq_enable();

    timeout = CLIC_TEST_TIMEOUT;
    while ((g_clic_test_handler_count == 0U) && (timeout != 0U))
    {
        timeout--;
        __asm__ volatile ("nop");
    }

    (void)e902_global_irq_disable();
    (void)e902_clic_disable_irq(T22_SERDES_IRQ_MACHINE_SOFTWARE);

    if (g_clic_test_handler_count == 0U)
    {
        return CLIC_TEST_FAIL_TIMEOUT;
    }

    if ((g_clic_test_stage != 2U) ||
        (g_clic_test_handler_count != 1U) ||
        (g_clic_test_handler_irq != T22_SERDES_IRQ_MACHINE_SOFTWARE) ||
        ((g_clic_test_handler_mcause & E902_MCAUSE_INTERRUPT_MASK) == 0U) ||
        ((g_clic_test_handler_mcause & E902_MCAUSE_CODE_MASK) !=
         T22_SERDES_IRQ_MACHINE_SOFTWARE) ||
        (g_clic_test_parameter_valid != 1U) ||
        (g_clic_test_pending_before_enable != 1U) ||
        (g_clic_test_pending_on_entry != 0U) ||
        (g_clic_test_pending_after_clear != 0U) ||
        (g_e902_irq_count != 1U) ||
        (g_e902_last_irq != T22_SERDES_IRQ_MACHINE_SOFTWARE) ||
        (g_e902_unhandled_irq_count != 0U))
    {
        return CLIC_TEST_FAIL_RESULT;
    }

    return CLIC_TEST_OK;
}

int main(void)
{
    (void)board_early_puts("T22 deserializer EVB booting...\n");
    (void)board_early_puts("E902 CLIC self-test: init\n");

    g_clic_test_result = run_clic_self_test();
    if (g_clic_test_result == CLIC_TEST_OK)
    {
        (void)board_early_puts("E902 CLIC self-test: handled IRQ 3\n");
        (void)board_early_puts("E902 CLIC self-test: PASS\n");
    }
    else
    {
        (void)board_early_puts("E902 CLIC self-test: FAIL result=");
        put_hex32(g_clic_test_result);
        (void)board_early_puts(" init=");
        put_hex32((uint32_t)g_clic_test_init_result);
        (void)board_early_puts("\n");
    }

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
