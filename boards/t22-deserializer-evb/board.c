#include "board.h"
#include "dw_apb_uart.h"
#include "t22_serdes.h"

#define BOARD_UART_BAUD_RATE     115200U
#define BOARD_UART_POLL_LIMIT    1000000U

static struct dw_apb_uart board_uart;
static int board_uart_ready;
static const struct dw_apb_uart_config board_uart_config =
{
    .base = T22_SERDES_UART2_BASE,
    .clock_hz = T22_SERDES_APB_CLOCK_HZ,
    .baud_rate = BOARD_UART_BAUD_RATE,
    .poll_limit = BOARD_UART_POLL_LIMIT
};

static void board_early_uart_init(void)
{
    t22_serdes_early_uart2_tx_pin_init();
    t22_serdes_early_uart2_reset();

    board_uart_ready =
        (dw_apb_uart_init(&board_uart, &board_uart_config) == DW_APB_UART_OK);
}

void board_init(void)
{
    t22_serdes_soc_early_init();
    board_early_uart_init();
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
