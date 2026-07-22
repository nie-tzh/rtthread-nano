#include <stdint.h>
#include <rthw.h>

#include "board.h"
#include "e902.h"
#include "e902_irq.h"
#include "riscv_clic.h"
#include "t22_serdes_irq.h"

#define CLIC_TEST_IRQ                  T22_SERDES_IRQ_CORE_TIMER
#define CLIC_TEST_TIMEOUT              1000000U
#define CLIC_TEST_PARAMETER_VALUE      0xC11C0007U
#define CLIC_TEST_MIN_CONTROL_BITS     2U
#define CLIC_TEST_MAX_CONTROL_BITS     5U

#define CLIC_TEST_OK                   0U
#define CLIC_TEST_FAIL_INFO            1U
#define CLIC_TEST_FAIL_CONFIGURE       2U
#define CLIC_TEST_FAIL_INSTALL         3U
#define CLIC_TEST_FAIL_PENDING_CLEAR   4U
#define CLIC_TEST_FAIL_PENDING_LATCH   5U
#define CLIC_TEST_FAIL_TIMEOUT         6U
#define CLIC_TEST_FAIL_RESULT          7U

volatile uint32_t g_clic_test_stage;
volatile uint32_t g_clic_test_handler_count;
volatile uint32_t g_clic_test_handler_irq;
volatile uint32_t g_clic_test_parameter_valid;
volatile uint32_t g_clic_test_pending_before_enable;
volatile uint32_t g_clic_test_pending_on_entry;
volatile uint32_t g_clic_test_result;

static uint32_t clic_test_parameter = CLIC_TEST_PARAMETER_VALUE;
static struct riscv_clic *clic_test_clic;

static void clic_test_enable_global_irq(void)
{
    rt_base_t level = rt_hw_interrupt_disable();

    rt_hw_interrupt_enable(level | E902_MSTATUS_MIE_MASK);
}

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

static void generic_irq_handler(
    int irq,
    void *parameter)
{
    g_clic_test_handler_count++;
    g_clic_test_handler_irq = (uint32_t)irq;
    g_clic_test_parameter_valid =
        ((parameter == &clic_test_parameter) &&
         (*(const uint32_t *)parameter == CLIC_TEST_PARAMETER_VALUE)) ? 1U : 0U;

    g_clic_test_pending_on_entry =
        riscv_clic_get_pending(clic_test_clic, (uint32_t)irq);

    g_clic_test_stage = 2U;
}

static uint32_t run_clic_self_test(void)
{
    const struct riscv_clic_info *info;
    uint32_t pending;
    uint32_t timeout;
    int result;

    (void)rt_hw_interrupt_disable();
    rt_hw_interrupt_init();

    clic_test_clic = t22_serdes_clic();
    info = riscv_clic_get_info(clic_test_clic);
    if ((info == 0) ||
        (info->hw_irq_count <= CLIC_TEST_IRQ) ||
        (info->hw_irq_count != T22_SERDES_IRQ_VECTOR_COUNT) ||
        (info->control_bits < CLIC_TEST_MIN_CONTROL_BITS) ||
        (info->control_bits > CLIC_TEST_MAX_CONTROL_BITS))
    {
        return CLIC_TEST_FAIL_INFO;
    }

    (void)board_early_puts("E902 CLIC self-test: CLICINFO=");
    put_hex32(info->raw);
    (void)board_early_puts(" irq_count=");
    put_hex32(info->hw_irq_count);
    (void)board_early_puts(" ctlbits=");
    put_hex32(info->control_bits);
    (void)board_early_puts("\n");

    result = riscv_clic_configure_irq(
        clic_test_clic,
        CLIC_TEST_IRQ,
        RISCV_CLIC_TRIGGER_POSITIVE_EDGE,
        E902_IRQ_LEVEL0);
    if (result != RISCV_CLIC_OK)
    {
        return CLIC_TEST_FAIL_CONFIGURE;
    }
    if (rt_hw_interrupt_install((int)CLIC_TEST_IRQ,
                                generic_irq_handler,
                                &clic_test_parameter,
                                "clic-test") == RT_NULL)
    {
        return CLIC_TEST_FAIL_INSTALL;
    }

    riscv_clic_clear_pending(clic_test_clic, CLIC_TEST_IRQ);
    if (riscv_clic_get_pending(clic_test_clic, CLIC_TEST_IRQ) != 0U)
    {
        return CLIC_TEST_FAIL_PENDING_CLEAR;
    }

    rt_hw_interrupt_umask((int)CLIC_TEST_IRQ);

    g_clic_test_stage = 1U;
    (void)board_early_puts("E902 CLIC self-test: trigger IRQ 7\n");

    riscv_clic_set_pending(clic_test_clic, CLIC_TEST_IRQ);

    pending = riscv_clic_get_pending(clic_test_clic, CLIC_TEST_IRQ);
    if (pending != 1U)
    {
        return CLIC_TEST_FAIL_PENDING_LATCH;
    }
    g_clic_test_pending_before_enable = pending;

    clic_test_enable_global_irq();

    timeout = CLIC_TEST_TIMEOUT;
    while ((g_clic_test_handler_count == 0U) && (timeout != 0U))
    {
        timeout--;
        __asm__ volatile ("nop");
    }

    (void)rt_hw_interrupt_disable();
    rt_hw_interrupt_mask((int)CLIC_TEST_IRQ);

    if (g_clic_test_handler_count == 0U)
    {
        return CLIC_TEST_FAIL_TIMEOUT;
    }

    if ((g_clic_test_stage != 2U) ||
        (g_clic_test_handler_count != 1U) ||
        (g_clic_test_handler_irq != CLIC_TEST_IRQ) ||
        (g_clic_test_parameter_valid != 1U) ||
        (g_clic_test_pending_before_enable != 1U) ||
        (g_clic_test_pending_on_entry != 0U))
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
        (void)board_early_puts("E902 CLIC self-test: handled IRQ 7\n");
        (void)board_early_puts("E902 CLIC self-test: PASS\n");
    }
    else
    {
        (void)board_early_puts("E902 CLIC self-test: FAIL result=");
        put_hex32(g_clic_test_result);
        (void)board_early_puts("\n");
    }

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
