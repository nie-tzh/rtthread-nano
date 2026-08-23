#include <rthw.h>
#include <rtthread.h>

#include "board.h"
#include "board_pinctrl.h"
#include "board_reset.h"
#include "clk.h"
#include "dw_apb_uart.h"
#include "dw_uart_device.h"
#include "e902.h"
#include "t22_clk.h"
#include "t22_serdes.h"
#include "t22_serdes_irq.h"
#include "t22_serdes_timer.h"

#define BOARD_UART_BAUD_RATE     115200U
#define BOARD_UART_POLL_LIMIT    1000000U
#define BOARD_CONSOLE_DEVICE     "uart2"

_Static_assert(BOARD_TICK_TIMER_CHANNEL_INDEX <
               T22_SERDES_DW_TIMER_CHANNEL_COUNT,
               "RT-Thread Tick channel is outside the T22 timer block");

static struct dw_uart_device board_uart;
static int board_uart_ready;

static void board_uart_config_init(struct dw_apb_uart_config *config)
{
    config->base = T22_SERDES_UART2_BASE;
    config->clock_hz =
        (uint32_t)clk_get_rate(t22_clk_get(T22_CLK_APB));
    config->baud_rate = BOARD_UART_BAUD_RATE;
    config->poll_limit = BOARD_UART_POLL_LIMIT;
}

#ifdef BSP_USING_EARLY_CONSOLE
static void board_early_console_init(void)
{
    struct dw_apb_uart_config config;

    (void)board_uart2_pinctrl_select_state(PINCTRL_STATE_DEFAULT);
    (void)board_uart2_reset();

    board_uart_config_init(&config);
    board_uart_ready =
        (dw_uart_hw_init(&board_uart.uart, &config) ==
         DW_APB_UART_OK);
}
#endif

void board_early_init(void)
{
    t22_clk_init();
    board_reset_init();
    board_pinctrl_init();
#ifdef BSP_USING_EARLY_CONSOLE
    board_early_console_init();
#endif
}

static int board_uart_init(void)
{
    struct dw_apb_uart_config config;
    rt_err_t result;

    if ((board_uart_ready != 0) &&
        (dw_apb_uart_wait_tx_idle(&board_uart.uart) != DW_APB_UART_OK))
    {
        return -RT_ETIMEOUT;
    }

    (void)board_uart2_pinctrl_select_state(PINCTRL_STATE_DEFAULT);
    (void)board_uart2_reset();

    board_uart_config_init(&config);
    result = dw_uart_device_register(&board_uart,
                                     BOARD_CONSOLE_DEVICE,
                                     &config,
                                     T22_SERDES_IRQ_UART2);
    if (result != RT_EOK)
    {
        return result;
    }

    result = rt_device_init(&board_uart.parent);
    if (result != RT_EOK)
    {
        return result;
    }

    board_uart_ready = 1;
    (void)rt_console_set_device(BOARD_CONSOLE_DEVICE);
    return RT_EOK;
}

static void rt_hw_tick_handler(uint32_t channel, void *parameter)
{
    (void)channel;
    (void)parameter;
    rt_tick_increase();
}

void rt_hw_board_init(void)
{
    if (board_uart_init() != RT_EOK)
    {
        rt_hw_console_output("RT-Thread UART init failed\n");
        (void)rt_hw_interrupt_disable();
        for (;;)
        {
            __asm__ volatile ("nop");
        }
    }

    rt_hw_interrupt_init();
    t22_serdes_timer_init();
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif
}

int rt_hw_tick_init(void)
{
    int result;

    result = t22_serdes_timer_config_periodic(
        BOARD_TICK_TIMER_CHANNEL_INDEX,
        RT_TICK_PER_SECOND,
        rt_hw_tick_handler,
        0);
    if (result != T22_SERDES_TIMER_OK)
    {
        return result;
    }

    return t22_serdes_timer_start(BOARD_TICK_TIMER_CHANNEL_INDEX);
}

static int board_console_putc(char ch)
{
    int result;

    if (board_uart_ready == 0)
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    if (ch == '\n')
    {
        result = dw_apb_uart_putc(&board_uart.uart, '\r');
        if (result != DW_APB_UART_OK)
        {
            return result;
        }
    }

    return dw_apb_uart_putc(&board_uart.uart, ch);
}

void rt_hw_console_output(const char *text)
{
    if (text == 0)
    {
        return;
    }

    while (*text != '\0')
    {
        if (board_console_putc(*text++) != DW_APB_UART_OK)
        {
            return;
        }
    }
}

void e902_exception_putchar(char ch)
{
    (void)board_console_putc(ch);
}
