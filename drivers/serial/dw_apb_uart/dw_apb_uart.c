#include <stddef.h>

#include "dw_apb_uart.h"

#define UART_FCR_FIFO_ENABLE     (1U << 0)
#define UART_FCR_RX_FIFO_RESET   (1U << 1)
#define UART_FCR_TX_FIFO_RESET   (1U << 2)

#define UART_LCR_WORD_LEN_8      (3U << 0)
#define UART_LCR_DLAB            (1U << 7)

#define UART_LSR_DATA_READY      (1U << 0)
#define UART_LSR_THRE            (1U << 5)
#define UART_LSR_TEMT            (1U << 6)
#define UART_IER_RX_AVAILABLE    (1U << 0)
#define UART_USR_BUSY            (1U << 0)

struct dw_apb_uart_registers
{
    union
    {
        const volatile uint32_t receive_buffer;
        volatile uint32_t transmit_holding;
        volatile uint32_t divisor_latch_low;
    } data;

    union
    {
        volatile uint32_t interrupt_enable;
        volatile uint32_t divisor_latch_high;
    } interrupt;

    union
    {
        const volatile uint32_t interrupt_identification;
        volatile uint32_t fifo_control;
    } fifo;

    volatile uint32_t line_control;
    volatile uint32_t modem_control;
    const volatile uint32_t line_status;
    uint32_t reserved0[25];
    const volatile uint32_t user_status;
};

_Static_assert(offsetof(struct dw_apb_uart_registers, line_status) ==
               0x14U,
               "DW APB UART line-status offset mismatch");
_Static_assert(offsetof(struct dw_apb_uart_registers, user_status) ==
               0x7CU,
               "DW APB UART user-status offset mismatch");
_Static_assert(sizeof(struct dw_apb_uart_registers) == 0x80U,
               "DW APB UART register block size mismatch");

static int uart_wait_idle(
    const struct dw_apb_uart_registers *registers,
    uint32_t poll_limit)
{
    uint32_t remaining = poll_limit;

    while ((registers->user_status & UART_USR_BUSY) != 0U)
    {
        if (remaining-- == 0U)
        {
            return DW_APB_UART_ERROR_TIMEOUT;
        }
    }

    return DW_APB_UART_OK;
}

int dw_uart_hw_init(struct dw_apb_uart *uart,
                    const struct dw_apb_uart_config *config)
{
    struct dw_apb_uart_registers *registers;
    uint32_t baud_clock;
    uint32_t divisor;

    if (uart == 0)
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    uart->base = 0U;
    uart->poll_limit = 0U;

    if ((config == 0) || (config->base == 0U) ||
        ((config->base & 0x3U) != 0U) ||
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

    registers =
        (struct dw_apb_uart_registers *)(uintptr_t)config->base;

    if (uart_wait_idle(registers, config->poll_limit) != DW_APB_UART_OK)
    {
        return DW_APB_UART_ERROR_TIMEOUT;
    }

    registers->line_control = UART_LCR_WORD_LEN_8;
    registers->interrupt.interrupt_enable = 0U;
    registers->modem_control = 0U;

    registers->line_control =
        UART_LCR_DLAB | UART_LCR_WORD_LEN_8;
    registers->data.divisor_latch_low = divisor & 0xFFU;
    registers->interrupt.divisor_latch_high = (divisor >> 8) & 0xFFU;
    registers->line_control = UART_LCR_WORD_LEN_8;

    registers->fifo.fifo_control = UART_FCR_FIFO_ENABLE |
                                   UART_FCR_RX_FIFO_RESET |
                                   UART_FCR_TX_FIFO_RESET;
    registers->fifo.fifo_control = UART_FCR_FIFO_ENABLE;

    uart->base = config->base;
    uart->poll_limit = config->poll_limit;
    return DW_APB_UART_OK;
}

int dw_apb_uart_putc(struct dw_apb_uart *uart, char ch)
{
    struct dw_apb_uart_registers *registers =
        (struct dw_apb_uart_registers *)(uintptr_t)uart->base;
    uint32_t remaining;

    remaining = uart->poll_limit;
    while ((registers->line_status & UART_LSR_THRE) == 0U)
    {
        if (remaining-- == 0U)
        {
            return DW_APB_UART_ERROR_TIMEOUT;
        }
    }

    registers->data.transmit_holding = (uint8_t)ch;
    return DW_APB_UART_OK;
}

int dw_apb_uart_wait_tx_idle(struct dw_apb_uart *uart)
{
    struct dw_apb_uart_registers *registers;
    uint32_t remaining;

    if ((uart == 0) || (uart->base == 0U) || (uart->poll_limit == 0U))
    {
        return DW_APB_UART_ERROR_INVALID;
    }

    registers =
        (struct dw_apb_uart_registers *)(uintptr_t)uart->base;
    remaining = uart->poll_limit;
    while ((registers->line_status & UART_LSR_TEMT) == 0U)
    {
        if (remaining-- == 0U)
        {
            return DW_APB_UART_ERROR_TIMEOUT;
        }
    }

    return DW_APB_UART_OK;
}

int dw_apb_uart_getc(struct dw_apb_uart *uart)
{
    const struct dw_apb_uart_registers *registers;

    if ((uart == 0) || (uart->base == 0U))
    {
        return -1;
    }

    registers =
        (const struct dw_apb_uart_registers *)(uintptr_t)uart->base;
    if ((registers->line_status & UART_LSR_DATA_READY) == 0U)
    {
        return -1;
    }

    return (int)(registers->data.receive_buffer & 0xFFU);
}

void dw_apb_uart_set_rx_interrupt(struct dw_apb_uart *uart,
                                  uint32_t enabled)
{
    struct dw_apb_uart_registers *registers =
        (struct dw_apb_uart_registers *)(uintptr_t)uart->base;
    uint32_t value = registers->interrupt.interrupt_enable;

    if (enabled != 0U)
    {
        value |= UART_IER_RX_AVAILABLE;
    }
    else
    {
        value &= ~UART_IER_RX_AVAILABLE;
    }
    registers->interrupt.interrupt_enable = value;
}
