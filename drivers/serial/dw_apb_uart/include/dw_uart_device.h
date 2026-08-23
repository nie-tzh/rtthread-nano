#ifndef DW_UART_DEVICE_H
#define DW_UART_DEVICE_H

#include <rtthread.h>

#include "dw_apb_uart.h"

#define DW_UART_RX_BUFFER_SIZE  128U

struct dw_uart_device
{
    struct rt_device parent;
    struct dw_apb_uart uart;
    struct dw_apb_uart_config config;
    rt_uint8_t rx_buffer[DW_UART_RX_BUFFER_SIZE];
    volatile rt_uint16_t rx_read;
    volatile rt_uint16_t rx_write;
    int irq;
    rt_uint8_t rx_irq_installed;
};

rt_err_t dw_uart_device_register(
    struct dw_uart_device *device,
    const char *name,
    const struct dw_apb_uart_config *config,
    int irq);

#endif
