#include <rthw.h>
#include <rtthread.h>

#include "board.h"
#include "dw_apb_uart.h"
#include "e902.h"
#include "t22_serdes.h"
#include "t22_serdes_timer.h"

#define BOARD_UART_BAUD_RATE     115200U
#define BOARD_UART_POLL_LIMIT    1000000U

_Static_assert(BOARD_TICK_TIMER_CHANNEL_INDEX <
               T22_SERDES_DW_TIMER_CHANNEL_COUNT,
               "RT-Thread Tick channel is outside the T22 timer block");

static struct dw_apb_uart board_uart;
static int board_uart_ready;
static const struct dw_apb_uart_config board_uart_config =
{
    .base = T22_SERDES_UART2_BASE,
    .clock_hz = T22_SERDES_APB_CLOCK_HZ,
    .baud_rate = BOARD_UART_BAUD_RATE,
    .poll_limit = BOARD_UART_POLL_LIMIT
};

static void board_early_console_init(void)
{
    t22_serdes_uart2_tx_pin_init();
    t22_serdes_uart2_reset();

    board_uart_ready =
        (dw_apb_uart_init(&board_uart, &board_uart_config) == DW_APB_UART_OK);
}

void board_early_init(void)
{
    t22_serdes_system_init();
    board_early_console_init();
}

static void rt_hw_tick_handler(uint32_t channel, void *parameter)
{
    (void)channel;
    (void)parameter;
    rt_tick_increase();
}

void rt_hw_board_init(void)
{
    rt_hw_interrupt_init();
    t22_serdes_timer_init();
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

int board_early_putc(char ch)
{
    int result;

    if (board_uart_ready == 0)
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    if (ch == '\n')
    {
        result = dw_apb_uart_putc(&board_uart, '\r');
        if (result != DW_APB_UART_OK)
        {
            return result;
        }
    }

    return dw_apb_uart_putc(&board_uart, ch);
}

int board_early_puts(const char *text)
{
    int result;

    if (text == 0)
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    while (*text != '\0')
    {
        result = board_early_putc(*text++);
        if (result != DW_APB_UART_OK)
        {
            return result;
        }
    }

    return DW_APB_UART_OK;
}

void e902_exception_putchar(char ch)
{
    (void)board_early_putc(ch);
}
