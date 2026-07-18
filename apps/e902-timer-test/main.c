#include <stdint.h>

#include "board.h"
#include "e902_clic.h"
#include "t22_serdes.h"
#include "t22_serdes_irq.h"
#include "t22_serdes_timer.h"

#define TIMER_TEST_TICK_FREQUENCY_HZ       1000U
#define TIMER_TEST_AUX_FREQUENCY_HZ        500U
#define TIMER_TEST_AUX_CHANNEL_INDEX       1U
#define TIMER_TEST_MEASURE_INTERVALS       100U
#define TIMER_TEST_MEASURE_CALLBACKS       \
    (TIMER_TEST_MEASURE_INTERVALS + 1U)
#define TIMER_TEST_RESTART_CALLBACKS       20U
#define TIMER_TEST_STOP_AUX_CALLBACKS      5U
#define TIMER_TEST_TIMEOUT_CYCLES          T22_SERDES_AHB_CLOCK_HZ
#define TIMER_TEST_SPIN_LIMIT              20000000U
#define TIMER_TEST_EXPECTED_CYCLES         \
    ((T22_SERDES_AHB_CLOCK_HZ / TIMER_TEST_TICK_FREQUENCY_HZ) * \
     TIMER_TEST_MEASURE_INTERVALS)
#define TIMER_TEST_CYCLE_TOLERANCE         \
    (TIMER_TEST_EXPECTED_CYCLES / 50U)
#define TIMER_TEST_PARAMETER_VALUE         0x71CE0002U
#define TIMER_TEST_AUX_PARAMETER_VALUE     0xA1170001U

#define TIMER_TEST_OK                      0U
#define TIMER_TEST_FAIL_IRQ_INIT           1U
#define TIMER_TEST_FAIL_TICK_INIT          2U
#define TIMER_TEST_FAIL_AUX_CONFIG         3U
#define TIMER_TEST_FAIL_AUX_START          4U
#define TIMER_TEST_FAIL_TICK_START         5U
#define TIMER_TEST_FAIL_INITIAL_TIMEOUT    6U
#define TIMER_TEST_FAIL_PERIOD             7U
#define TIMER_TEST_FAIL_TICK_STOP          8U
#define TIMER_TEST_FAIL_CURRENT_READ       9U
#define TIMER_TEST_FAIL_STOP_TIMEOUT       10U
#define TIMER_TEST_FAIL_STOP_BEHAVIOR      11U
#define TIMER_TEST_FAIL_RESTART            12U
#define TIMER_TEST_FAIL_RESTART_TIMEOUT    13U
#define TIMER_TEST_FAIL_FINAL_STOP         14U
#define TIMER_TEST_FAIL_AUX_STOP           15U
#define TIMER_TEST_FAIL_RESULT             16U

_Static_assert((T22_SERDES_AHB_CLOCK_HZ %
                TIMER_TEST_TICK_FREQUENCY_HZ) == 0U,
               "CPU clock must divide the timer-test frequency");
_Static_assert((T22_SERDES_APB_CLOCK_HZ %
                TIMER_TEST_TICK_FREQUENCY_HZ) == 0U,
               "Timer clock must divide the Tick frequency");
_Static_assert((T22_SERDES_APB_CLOCK_HZ %
                TIMER_TEST_AUX_FREQUENCY_HZ) == 0U,
               "Timer clock must divide the auxiliary frequency");
_Static_assert(TIMER_TEST_AUX_CHANNEL_INDEX !=
               BOARD_TICK_TIMER_CHANNEL_INDEX,
               "Timer test channels must be different");
_Static_assert(TIMER_TEST_AUX_CHANNEL_INDEX <
               T22_SERDES_DW_TIMER_CHANNEL_COUNT,
               "Auxiliary timer channel is outside the T22 timer block");

volatile uint32_t g_timer_test_tick_count;
volatile uint32_t g_timer_test_aux_count;
volatile uint32_t g_timer_test_first_cycle;
volatile uint32_t g_timer_test_last_cycle;
volatile uint32_t g_timer_test_measured_cycles;
volatile uint32_t g_timer_test_stopped_current_before;
volatile uint32_t g_timer_test_stopped_current_after;
volatile uint32_t g_timer_test_tick_parameter_valid;
volatile uint32_t g_timer_test_aux_parameter_valid;
volatile uint32_t g_timer_test_aux_channel_valid;
volatile uint32_t g_timer_test_pending_after_stop;
volatile uint32_t g_timer_test_result;
volatile int32_t g_timer_test_last_status;

static uint32_t timer_test_parameter = TIMER_TEST_PARAMETER_VALUE;
static uint32_t timer_test_aux_parameter = TIMER_TEST_AUX_PARAMETER_VALUE;

static uint32_t read_mcycle(void)
{
    uint32_t value;

    __asm__ volatile ("csrr %0, mcycle" : "=r" (value));
    return value;
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

static int wait_for_count(volatile uint32_t *count,
                          uint32_t target,
                          uint32_t timeout_cycles)
{
    uint32_t start = read_mcycle();
    uint32_t spins = TIMER_TEST_SPIN_LIMIT;

    while (*count < target)
    {
        if (((uint32_t)(read_mcycle() - start) >= timeout_cycles) ||
            (--spins == 0U))
        {
            return -1;
        }
    }

    return 0;
}

static void timer_test_tick_handler(void *parameter)
{
    uint32_t next_count = g_timer_test_tick_count + 1U;

    if ((parameter != &timer_test_parameter) ||
        (*(const uint32_t *)parameter != TIMER_TEST_PARAMETER_VALUE))
    {
        g_timer_test_tick_parameter_valid = 0U;
    }

    if (next_count == 1U)
    {
        g_timer_test_first_cycle = read_mcycle();
    }
    else if (next_count == TIMER_TEST_MEASURE_CALLBACKS)
    {
        g_timer_test_last_cycle = read_mcycle();
    }

    g_timer_test_tick_count = next_count;
}

static void timer_test_aux_handler(uint32_t channel, void *parameter)
{
    if (channel != TIMER_TEST_AUX_CHANNEL_INDEX)
    {
        g_timer_test_aux_channel_valid = 0U;
    }
    if ((parameter != &timer_test_aux_parameter) ||
        (*(const uint32_t *)parameter != TIMER_TEST_AUX_PARAMETER_VALUE))
    {
        g_timer_test_aux_parameter_valid = 0U;
    }

    g_timer_test_aux_count++;
}

static uint32_t run_timer_self_test(void)
{
    uint32_t tick_started = 0U;
    uint32_t aux_started = 0U;
    uint32_t stopped_tick_count;
    uint32_t aux_stop_target;
    uint32_t aux_restart_count;
    uint32_t restart_target;
    uint32_t minimum_cycles;
    uint32_t maximum_cycles;
    uint32_t current;
    uint32_t pending;
    uint32_t result = TIMER_TEST_OK;
    int status;

    g_timer_test_tick_parameter_valid = 1U;
    g_timer_test_aux_parameter_valid = 1U;
    g_timer_test_aux_channel_valid = 1U;
    g_timer_test_pending_after_stop = 0xFFFFFFFFU;
    g_timer_test_last_status = 0;
    (void)e902_global_irq_disable();

    status = t22_serdes_irq_init();
    if (status != E902_CLIC_OK)
    {
        g_timer_test_last_status = status;
        return TIMER_TEST_FAIL_IRQ_INIT;
    }

    status = board_tick_init(TIMER_TEST_TICK_FREQUENCY_HZ,
                             timer_test_tick_handler,
                             &timer_test_parameter);
    if (status != BOARD_TICK_OK)
    {
        g_timer_test_last_status = status;
        return TIMER_TEST_FAIL_TICK_INIT;
    }

    status = t22_serdes_timer_configure_periodic(
        TIMER_TEST_AUX_CHANNEL_INDEX,
        TIMER_TEST_AUX_FREQUENCY_HZ,
        timer_test_aux_handler,
        &timer_test_aux_parameter);
    if (status != T22_SERDES_TIMER_OK)
    {
        g_timer_test_last_status = status;
        return TIMER_TEST_FAIL_AUX_CONFIG;
    }

    status = t22_serdes_timer_start(TIMER_TEST_AUX_CHANNEL_INDEX);
    if (status != T22_SERDES_TIMER_OK)
    {
        g_timer_test_last_status = status;
        return TIMER_TEST_FAIL_AUX_START;
    }
    aux_started = 1U;

    status = board_tick_start();
    if (status != BOARD_TICK_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_TICK_START;
        goto cleanup;
    }
    tick_started = 1U;

    e902_global_irq_enable();
    if (wait_for_count(&g_timer_test_tick_count,
                       TIMER_TEST_MEASURE_CALLBACKS,
                       TIMER_TEST_TIMEOUT_CYCLES) != 0)
    {
        result = TIMER_TEST_FAIL_INITIAL_TIMEOUT;
        goto cleanup;
    }
    (void)e902_global_irq_disable();

    g_timer_test_measured_cycles =
        g_timer_test_last_cycle - g_timer_test_first_cycle;
    minimum_cycles = TIMER_TEST_EXPECTED_CYCLES -
                     TIMER_TEST_CYCLE_TOLERANCE;
    maximum_cycles = TIMER_TEST_EXPECTED_CYCLES +
                     TIMER_TEST_CYCLE_TOLERANCE;
    if ((g_timer_test_measured_cycles < minimum_cycles) ||
        (g_timer_test_measured_cycles > maximum_cycles))
    {
        result = TIMER_TEST_FAIL_PERIOD;
        goto cleanup;
    }

    status = board_tick_stop();
    if (status != BOARD_TICK_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_TICK_STOP;
        goto cleanup;
    }
    tick_started = 0U;

    status = t22_serdes_timer_get_current(
        BOARD_TICK_TIMER_CHANNEL_INDEX,
        &current);
    if (status != T22_SERDES_TIMER_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_CURRENT_READ;
        goto cleanup;
    }
    g_timer_test_stopped_current_before = current;

    stopped_tick_count = g_timer_test_tick_count;
    aux_stop_target = g_timer_test_aux_count +
                      TIMER_TEST_STOP_AUX_CALLBACKS;
    e902_global_irq_enable();
    if (wait_for_count(&g_timer_test_aux_count,
                       aux_stop_target,
                       TIMER_TEST_TIMEOUT_CYCLES) != 0)
    {
        result = TIMER_TEST_FAIL_STOP_TIMEOUT;
        goto cleanup;
    }
    (void)e902_global_irq_disable();

    status = t22_serdes_timer_get_current(
        BOARD_TICK_TIMER_CHANNEL_INDEX,
        &current);
    if (status != T22_SERDES_TIMER_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_CURRENT_READ;
        goto cleanup;
    }
    g_timer_test_stopped_current_after = current;
    if ((g_timer_test_tick_count != stopped_tick_count) ||
        (g_timer_test_stopped_current_before !=
         g_timer_test_stopped_current_after))
    {
        result = TIMER_TEST_FAIL_STOP_BEHAVIOR;
        goto cleanup;
    }

    aux_restart_count = g_timer_test_aux_count;
    restart_target = g_timer_test_tick_count +
                     TIMER_TEST_RESTART_CALLBACKS;
    status = board_tick_start();
    if (status != BOARD_TICK_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_RESTART;
        goto cleanup;
    }
    tick_started = 1U;

    e902_global_irq_enable();
    if (wait_for_count(&g_timer_test_tick_count,
                       restart_target,
                       TIMER_TEST_TIMEOUT_CYCLES) != 0)
    {
        result = TIMER_TEST_FAIL_RESTART_TIMEOUT;
        goto cleanup;
    }
    (void)e902_global_irq_disable();
    if (g_timer_test_aux_count <= aux_restart_count)
    {
        result = TIMER_TEST_FAIL_RESTART_TIMEOUT;
        goto cleanup;
    }

    status = board_tick_stop();
    if (status != BOARD_TICK_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_FINAL_STOP;
        goto cleanup;
    }
    tick_started = 0U;

    status = t22_serdes_timer_stop(TIMER_TEST_AUX_CHANNEL_INDEX);
    if (status != T22_SERDES_TIMER_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_AUX_STOP;
        goto cleanup;
    }
    aux_started = 0U;

    status = e902_clic_get_pending(T22_SERDES_IRQ_DW_TIMER, &pending);
    if (status != E902_CLIC_OK)
    {
        g_timer_test_last_status = status;
        result = TIMER_TEST_FAIL_RESULT;
        goto cleanup;
    }
    g_timer_test_pending_after_stop = pending;

    if ((g_timer_test_tick_parameter_valid == 0U) ||
        (g_timer_test_aux_parameter_valid == 0U) ||
        (g_timer_test_aux_channel_valid == 0U) ||
        (g_timer_test_pending_after_stop != 0U) ||
        (g_timer_test_aux_count == 0U) ||
        (g_e902_irq_count < g_timer_test_tick_count) ||
        (g_e902_irq_count >
         (g_timer_test_tick_count + g_timer_test_aux_count)) ||
        (g_e902_last_irq != T22_SERDES_IRQ_DW_TIMER) ||
        (g_e902_unhandled_irq_count != 0U))
    {
        result = TIMER_TEST_FAIL_RESULT;
    }

cleanup:
    (void)e902_global_irq_disable();
    if (tick_started != 0U)
    {
        (void)board_tick_stop();
    }
    if (aux_started != 0U)
    {
        (void)t22_serdes_timer_stop(TIMER_TEST_AUX_CHANNEL_INDEX);
    }

    return result;
}

int main(void)
{
    (void)board_early_puts("T22 deserializer EVB booting...\n");
    (void)board_early_puts("E902 DW Timer self-test: init\n");
    (void)board_early_puts("E902 DW Timer self-test: tick_hz=");
    put_hex32(TIMER_TEST_TICK_FREQUENCY_HZ);
    (void)board_early_puts(" aux_hz=");
    put_hex32(TIMER_TEST_AUX_FREQUENCY_HZ);
    (void)board_early_puts("\n");

    g_timer_test_result = run_timer_self_test();

    (void)board_early_puts("E902 DW Timer self-test: cycles=");
    put_hex32(g_timer_test_measured_cycles);
    (void)board_early_puts(" expected=");
    put_hex32(TIMER_TEST_EXPECTED_CYCLES);
    (void)board_early_puts(" tolerance=");
    put_hex32(TIMER_TEST_CYCLE_TOLERANCE);
    (void)board_early_puts("\n");

    (void)board_early_puts("E902 DW Timer self-test: tick_count=");
    put_hex32(g_timer_test_tick_count);
    (void)board_early_puts(" aux_count=");
    put_hex32(g_timer_test_aux_count);
    (void)board_early_puts(" irq_count=");
    put_hex32(g_e902_irq_count);
    (void)board_early_puts("\n");

    (void)board_early_puts("E902 DW Timer self-test: stopped_current=");
    put_hex32(g_timer_test_stopped_current_before);
    (void)board_early_puts("/");
    put_hex32(g_timer_test_stopped_current_after);
    (void)board_early_puts(" pending=");
    put_hex32(g_timer_test_pending_after_stop);
    (void)board_early_puts("\n");

    if (g_timer_test_result == TIMER_TEST_OK)
    {
        (void)board_early_puts("E902 DW Timer self-test: PASS\n");
    }
    else
    {
        (void)board_early_puts("E902 DW Timer self-test: FAIL result=");
        put_hex32(g_timer_test_result);
        (void)board_early_puts(" status=");
        put_hex32((uint32_t)g_timer_test_last_status);
        (void)board_early_puts("\n");
    }

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
