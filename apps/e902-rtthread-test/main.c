#include <stdint.h>

#include <rthw.h>
#include <rtthread.h>

#include "board.h"
#include "riscv_clic.h"
#include "t22_serdes_irq.h"
#include "t22_serdes_timer.h"

#define RTTHREAD_TEST_STACK_SIZE       1024U
#define RTTHREAD_TEST_STACK_GUARD_SIZE   64U
#define RTTHREAD_TEST_TIMESLICE_ROUNDS    4U
#define RTTHREAD_TEST_DELAY_ROUNDS        4U
#define RTTHREAD_TEST_A_DELAY_TICKS    2U
#define RTTHREAD_TEST_B_DELAY_TICKS    3U
#define RTTHREAD_TEST_WAIT_TICKS       1U
#define RTTHREAD_TEST_THREAD_PRIORITY  5U
#define RTTHREAD_TEST_THREAD_SLICE     1U

#define RTTHREAD_TEST_OK               0U
#define RTTHREAD_TEST_FAIL_INIT        1U
#define RTTHREAD_TEST_FAIL_THREAD_INIT 2U
#define RTTHREAD_TEST_FAIL_SCHEDULER   3U
#define RTTHREAD_TEST_FAIL_TICK        4U
#define RTTHREAD_TEST_FAIL_THREADS     5U
#define RTTHREAD_TEST_FAIL_IRQ_STATE   6U
#define RTTHREAD_TEST_FAIL_STACK       7U
#define RTTHREAD_TEST_RUNNING          0xFFFFFFFFU

static struct rt_thread rt_test_thread_a;
static struct rt_thread rt_test_thread_b;
static uint8_t rt_test_stack_a[RTTHREAD_TEST_STACK_SIZE]
    __attribute__((aligned(16)));
static uint8_t rt_test_stack_b[RTTHREAD_TEST_STACK_SIZE]
    __attribute__((aligned(16)));

static volatile uint32_t rt_test_a_timeslices;
static volatile uint32_t rt_test_b_timeslices;
static volatile uint32_t rt_test_a_delays;
static volatile uint32_t rt_test_b_delays;
static volatile uint32_t rt_test_b_done;
static volatile uint32_t rt_test_result;
static volatile int32_t rt_test_last_status;

static void rt_test_put_hex32(uint32_t value)
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

static void rt_test_halt(uint32_t result, int32_t status)
{
    (void)rt_hw_interrupt_disable();
    rt_test_result = result;
    rt_test_last_status = status;
    (void)board_early_puts("E902 RT-Thread self-test: FAIL result=");
    rt_test_put_hex32(result);
    (void)board_early_puts(" status=");
    rt_test_put_hex32((uint32_t)status);
    (void)board_early_puts("\n");

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

static void rt_test_wait_for_next_tick(void)
{
    rt_tick_t start = rt_tick_get();

    while (rt_tick_get() == start)
    {
        __asm__ volatile ("nop");
    }
}

static int rt_test_stack_guard_is_valid(const uint8_t *stack)
{
    uint32_t index;

    for (index = 0U; index < RTTHREAD_TEST_STACK_GUARD_SIZE; index++)
    {
        if (stack[index] != (uint8_t)'#')
        {
            return 0;
        }
    }

    return 1;
}

static void rt_test_thread_b_entry(void *parameter)
{
    uint32_t index;

    (void)parameter;
    for (index = 0U; index < RTTHREAD_TEST_TIMESLICE_ROUNDS; index++)
    {
        rt_test_wait_for_next_tick();
        rt_test_b_timeslices++;
    }

    for (index = 0U; index < RTTHREAD_TEST_DELAY_ROUNDS; index++)
    {
        (void)rt_thread_delay(RTTHREAD_TEST_B_DELAY_TICKS);
        rt_test_b_delays++;
    }

    rt_test_b_done = 1U;
    for (;;)
    {
        (void)rt_thread_delay(RTTHREAD_TEST_B_DELAY_TICKS * 10U);
    }
}

static void rt_test_thread_a_entry(void *parameter)
{
    struct riscv_clic *clic = t22_serdes_clic();
    uint32_t index;
    uint32_t pending_context;
    uint32_t pending_timer;
    uint32_t tick_count;
    uint8_t interrupt_nest;
    int status;

    (void)parameter;
    for (index = 0U; index < RTTHREAD_TEST_TIMESLICE_ROUNDS; index++)
    {
        rt_test_wait_for_next_tick();
        rt_test_a_timeslices++;
    }

    for (index = 0U; index < RTTHREAD_TEST_DELAY_ROUNDS; index++)
    {
        (void)rt_thread_delay(RTTHREAD_TEST_A_DELAY_TICKS);
        rt_test_a_delays++;
    }

    while (rt_test_b_done == 0U)
    {
        (void)rt_thread_delay(RTTHREAD_TEST_WAIT_TICKS);
    }

    tick_count = rt_tick_get();
    interrupt_nest = rt_interrupt_get_nest();
    pending_context = 0xFFFFFFFFU;
    pending_timer = 0xFFFFFFFFU;

    status = t22_serdes_timer_stop(BOARD_TICK_TIMER_CHANNEL_INDEX);
    if (status != T22_SERDES_TIMER_OK)
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_TICK, status);
    }

    if ((rt_test_a_timeslices != RTTHREAD_TEST_TIMESLICE_ROUNDS) ||
        (rt_test_b_timeslices != RTTHREAD_TEST_TIMESLICE_ROUNDS) ||
        (rt_test_a_delays != RTTHREAD_TEST_DELAY_ROUNDS) ||
        (rt_test_b_delays != RTTHREAD_TEST_DELAY_ROUNDS))
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_THREADS, 0);
    }

    if ((tick_count < (2U * RTTHREAD_TEST_TIMESLICE_ROUNDS)) ||
        (rt_thread_self() != &rt_test_thread_a))
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_TICK, 0);
    }

    pending_context = riscv_clic_get_pending(
        clic,
        T22_SERDES_IRQ_MACHINE_SOFTWARE);
    pending_timer = riscv_clic_get_pending(clic,
                                           T22_SERDES_IRQ_DW_TIMER);
    if ((pending_context != 0U) ||
        (pending_timer != 0U) ||
        (interrupt_nest != 0U))
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_IRQ_STATE, 0);
    }

    if (!rt_test_stack_guard_is_valid(rt_test_stack_a) ||
        !rt_test_stack_guard_is_valid(rt_test_stack_b))
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_STACK, 0);
    }

    rt_test_result = RTTHREAD_TEST_OK;
    (void)board_early_puts("E902 RT-Thread self-test: ticks=");
    rt_test_put_hex32(tick_count);
    (void)board_early_puts(" slice_A/B=");
    rt_test_put_hex32(rt_test_a_timeslices);
    (void)board_early_puts("/");
    rt_test_put_hex32(rt_test_b_timeslices);
    (void)board_early_puts(" delay_A/B=");
    rt_test_put_hex32(rt_test_a_delays);
    (void)board_early_puts("/");
    rt_test_put_hex32(rt_test_b_delays);
    (void)board_early_puts(" irq_nest=");
    rt_test_put_hex32((uint32_t)interrupt_nest);
    (void)board_early_puts("\nE902 RT-Thread self-test: PASS\n");

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    rt_err_t result;
    int32_t status;

    (void)board_early_puts("T22 deserializer EVB booting...\n");
    (void)board_early_puts("E902 RT-Thread self-test: init\n");

    (void)rt_hw_interrupt_disable();
    rt_hw_board_init();
    rt_system_timer_init();
    rt_system_scheduler_init();

    result = rt_thread_init(&rt_test_thread_a,
                            "rta",
                            rt_test_thread_a_entry,
                            0,
                            rt_test_stack_a,
                            sizeof(rt_test_stack_a),
                            RTTHREAD_TEST_THREAD_PRIORITY,
                            RTTHREAD_TEST_THREAD_SLICE);
    if (result != RT_EOK)
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_THREAD_INIT, (int32_t)result);
    }

    result = rt_thread_init(&rt_test_thread_b,
                            "rtb",
                            rt_test_thread_b_entry,
                            0,
                            rt_test_stack_b,
                            sizeof(rt_test_stack_b),
                            RTTHREAD_TEST_THREAD_PRIORITY,
                            RTTHREAD_TEST_THREAD_SLICE);
    if (result != RT_EOK)
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_THREAD_INIT, (int32_t)result);
    }

    (void)rt_thread_startup(&rt_test_thread_a);
    (void)rt_thread_startup(&rt_test_thread_b);
    rt_thread_idle_init();

    status = rt_hw_tick_init();
    if (status != 0)
    {
        rt_test_halt(RTTHREAD_TEST_FAIL_INIT, status);
    }

    rt_test_result = RTTHREAD_TEST_RUNNING;
    rt_test_last_status = 0;
    (void)board_early_puts("E902 RT-Thread self-test: scheduler start\n");
    rt_system_scheduler_start();

    rt_test_halt(RTTHREAD_TEST_FAIL_SCHEDULER, 0);
}
