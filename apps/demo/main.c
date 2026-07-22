#include <stdint.h>

#include <rthw.h>
#include <rtthread.h>

#include "board.h"

#define DEMO_THREAD_STACK_SIZE  1024U
#define DEMO_THREAD_PRIORITY    5U
#define DEMO_THREAD_TIMESLICE   10U

static struct rt_thread demo_thread_5ms;
static struct rt_thread demo_thread_10ms;
static uint8_t demo_thread_5ms_stack[DEMO_THREAD_STACK_SIZE]
    __attribute__((aligned(16)));
static uint8_t demo_thread_10ms_stack[DEMO_THREAD_STACK_SIZE]
    __attribute__((aligned(16)));

static void demo_halt(const char *reason) __attribute__((noreturn));

static void demo_halt(const char *reason)
{
    (void)rt_hw_interrupt_disable();
    (void)board_early_puts(reason);

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}

static void demo_thread_5ms_entry(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        (void)board_early_puts("demo: task5\n");
        (void)rt_thread_mdelay(5);
    }
}

static void demo_thread_10ms_entry(void *parameter)
{
    (void)parameter;

    for (;;)
    {
        (void)board_early_puts("demo: task10\n");
        (void)rt_thread_mdelay(10);
    }
}

int main(void)
{
    rt_err_t result;
    int status;

    (void)board_early_puts("T22 deserializer EVB booting...\n");

    (void)rt_hw_interrupt_disable();
    rt_hw_board_init();
    rt_system_timer_init();
    rt_system_scheduler_init();

    result = rt_thread_init(&demo_thread_5ms,
                            "task5",
                            demo_thread_5ms_entry,
                            0,
                            demo_thread_5ms_stack,
                            sizeof(demo_thread_5ms_stack),
                            DEMO_THREAD_PRIORITY,
                            DEMO_THREAD_TIMESLICE);
    if (result != RT_EOK)
    {
        demo_halt("RT-Thread task5 init failed\n");
    }

    result = rt_thread_init(&demo_thread_10ms,
                            "task10",
                            demo_thread_10ms_entry,
                            0,
                            demo_thread_10ms_stack,
                            sizeof(demo_thread_10ms_stack),
                            DEMO_THREAD_PRIORITY,
                            DEMO_THREAD_TIMESLICE);
    if (result != RT_EOK)
    {
        demo_halt("RT-Thread task10 init failed\n");
    }

    (void)rt_thread_startup(&demo_thread_5ms);
    (void)rt_thread_startup(&demo_thread_10ms);
    rt_thread_idle_init();

    status = rt_hw_tick_init();
    if (status != 0)
    {
        demo_halt("RT-Thread Tick init failed\n");
    }

    (void)board_early_puts("RT-Thread demo scheduler start\n");
    rt_system_scheduler_start();

    demo_halt("RT-Thread scheduler returned\n");
}
