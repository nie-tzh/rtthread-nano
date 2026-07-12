#include "dw_apb_uart.h"

#define UART_RBR_THR_DLL_OFFSET  0x00U
#define UART_IER_DLH_OFFSET      0x04U
#define UART_FCR_OFFSET          0x08U
#define UART_LCR_OFFSET          0x0CU
#define UART_MCR_OFFSET          0x10U
#define UART_LSR_OFFSET          0x14U
#define UART_USR_OFFSET          0x7CU

#define UART_FCR_FIFO_ENABLE     (1U << 0)
#define UART_FCR_RX_FIFO_RESET   (1U << 1)
#define UART_FCR_TX_FIFO_RESET   (1U << 2)

#define UART_LCR_WORD_LEN_8      (3U << 0)
#define UART_LCR_DLAB            (1U << 7)

#define UART_LSR_THRE            (1U << 5)
#define UART_USR_BUSY            (1U << 0)

static volatile uint32_t *uart_reg(const struct dw_apb_uart *uart,
                                   uint32_t offset)
{
    return (volatile uint32_t *)(uart->base + offset);
}

static int uart_wait_idle(const struct dw_apb_uart *uart)
{
    uint32_t remaining = uart->poll_limit;

    while ((*uart_reg(uart, UART_USR_OFFSET) & UART_USR_BUSY) != 0U)
    {
        if (remaining-- == 0U)
        {
            return DW_APB_UART_ERROR_TIMEOUT;
        }
    }

    return DW_APB_UART_OK;
}

int dw_apb_uart_init(struct dw_apb_uart *uart,
                     const struct dw_apb_uart_config *config)
{
    struct dw_apb_uart candidate;
    uint32_t baud_clock;
    uint32_t divisor;

    if (uart == 0)
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    uart->base = 0U;
    uart->poll_limit = 0U;

    if ((config == 0) || (config->base == 0U) ||
        (config->clock_hz == 0U) || (config->baud_rate == 0U) ||
        (config->baud_rate > (UINT32_MAX / 16U)) ||
        (config->poll_limit == 0U))
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    baud_clock = config->baud_rate * 16U;
    divisor = config->clock_hz / baud_clock;
    if ((config->clock_hz % baud_clock) >= ((baud_clock + 1U) / 2U))
    {
        ++divisor;
    }
    if ((divisor == 0U) || (divisor > UINT16_MAX))
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    candidate.base = config->base;
    candidate.poll_limit = config->poll_limit;

    if (uart_wait_idle(&candidate) != DW_APB_UART_OK)
    {
        return DW_APB_UART_ERROR_TIMEOUT;
    }

    *uart_reg(&candidate, UART_LCR_OFFSET) = UART_LCR_WORD_LEN_8;
    *uart_reg(&candidate, UART_IER_DLH_OFFSET) = 0U;
    *uart_reg(&candidate, UART_MCR_OFFSET) = 0U;

    *uart_reg(&candidate, UART_LCR_OFFSET) =
        UART_LCR_DLAB | UART_LCR_WORD_LEN_8;
    *uart_reg(&candidate, UART_RBR_THR_DLL_OFFSET) = divisor & 0xFFU;
    *uart_reg(&candidate, UART_IER_DLH_OFFSET) = (divisor >> 8) & 0xFFU;
    *uart_reg(&candidate, UART_LCR_OFFSET) = UART_LCR_WORD_LEN_8;

    *uart_reg(&candidate, UART_FCR_OFFSET) = UART_FCR_FIFO_ENABLE |
                                             UART_FCR_RX_FIFO_RESET |
                                             UART_FCR_TX_FIFO_RESET;
    *uart_reg(&candidate, UART_FCR_OFFSET) = UART_FCR_FIFO_ENABLE;

    *uart = candidate;
    return DW_APB_UART_OK;
}

int dw_apb_uart_putc(struct dw_apb_uart *uart, char ch)
{
    uint32_t remaining;

    if ((uart == 0) || (uart->base == 0U) || (uart->poll_limit == 0U))
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    remaining = uart->poll_limit;
    while ((*uart_reg(uart, UART_LSR_OFFSET) & UART_LSR_THRE) == 0U)
    {
        if (remaining-- == 0U)
        {
            return DW_APB_UART_ERROR_TIMEOUT;
        }
    }

    *uart_reg(uart, UART_RBR_THR_DLL_OFFSET) = (uint8_t)ch;
    return DW_APB_UART_OK;
}

int dw_apb_uart_write(struct dw_apb_uart *uart,
                      const char *data,
                      uint32_t length)
{
    uint32_t index;
    int result;

    if ((data == 0) && (length != 0U))
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    for (index = 0U; index < length; ++index)
    {
        result = dw_apb_uart_putc(uart, data[index]);
        if (result != DW_APB_UART_OK)
        {
            return result;
        }
    }

    return DW_APB_UART_OK;
}
