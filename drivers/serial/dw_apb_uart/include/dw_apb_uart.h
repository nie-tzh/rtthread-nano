#ifndef DW_APB_UART_H
#define DW_APB_UART_H

#include <stdint.h>

enum dw_apb_uart_result
{
    DW_APB_UART_OK = 0,
    DW_APB_UART_ERROR_INVALID = -1,
    DW_APB_UART_ERROR_TIMEOUT = -2
};

struct dw_apb_uart
{
    uintptr_t base;
    uint32_t poll_limit;
};

struct dw_apb_uart_config
{
    uintptr_t base;
    uint32_t clock_hz;
    uint32_t baud_rate;
    uint32_t poll_limit;
};

int dw_uart_hw_init(struct dw_apb_uart *uart,
                    const struct dw_apb_uart_config *config);
int dw_apb_uart_wait_tx_idle(struct dw_apb_uart *uart);
int dw_apb_uart_putc(struct dw_apb_uart *uart, char ch);
int dw_apb_uart_getc(struct dw_apb_uart *uart);
void dw_apb_uart_set_rx_interrupt(struct dw_apb_uart *uart,
                                  uint32_t enabled);

#endif
