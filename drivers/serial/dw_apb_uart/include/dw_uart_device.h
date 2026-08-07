#ifndef DW_UART_DEVICE_H
#define DW_UART_DEVICE_H

#include <rtthread.h>

#include "dw_apb_uart.h"

struct dw_uart_device
{
    struct rt_device parent;
    struct dw_apb_uart uart;
    struct dw_apb_uart_config config;
};

rt_err_t dw_uart_device_register(
    struct dw_uart_device *device,
    const char *name,
    const struct dw_apb_uart_config *config);

#endif
