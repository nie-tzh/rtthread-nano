#include <rtthread.h>

#include "dw_uart_device.h"

static rt_err_t dw_uart_init(rt_device_t rt_device)
{
    struct dw_uart_device *device =
        (struct dw_uart_device *)rt_device;

    if (dw_apb_uart_init(&device->uart, &device->config) != DW_APB_UART_OK)
    {
        return -RT_EIO;
    }

    return RT_EOK;
}

static rt_size_t dw_uart_read(rt_device_t rt_device,
                              rt_off_t pos,
                              void *buffer,
                              rt_size_t size)
{
    struct dw_uart_device *device =
        (struct dw_uart_device *)rt_device;
    rt_uint8_t *data = buffer;
    rt_size_t count = 0U;
    int ch;

    (void)pos;
    while (count < size)
    {
        ch = dw_apb_uart_getc(&device->uart);
        if (ch < 0)
        {
            break;
        }

        data[count++] = (rt_uint8_t)ch;
    }

    return count;
}

static rt_size_t dw_uart_write(rt_device_t rt_device,
                               rt_off_t pos,
                               const void *buffer,
                               rt_size_t size)
{
    struct dw_uart_device *device =
        (struct dw_uart_device *)rt_device;
    const rt_uint8_t *data = buffer;
    rt_size_t count = 0U;

    (void)pos;
    while (count < size)
    {
        if ((data[count] == (rt_uint8_t)'\n') &&
            ((rt_device->open_flag & RT_DEVICE_FLAG_STREAM) != 0U) &&
            (dw_apb_uart_putc(&device->uart, '\r') != DW_APB_UART_OK))
        {
            break;
        }

        if (dw_apb_uart_putc(&device->uart, (char)data[count]) !=
            DW_APB_UART_OK)
        {
            break;
        }

        count++;
    }

    return count;
}

static const struct rt_device_ops dw_uart_ops =
{
    .init = dw_uart_init,
    .open = RT_NULL,
    .close = RT_NULL,
    .read = dw_uart_read,
    .write = dw_uart_write,
    .control = RT_NULL
};

rt_err_t dw_uart_device_register(
    struct dw_uart_device *device,
    const char *name,
    const struct dw_apb_uart_config *config)
{
    device->config.base = config->base;
    device->config.clock_hz = config->clock_hz;
    device->config.baud_rate = config->baud_rate;
    device->config.poll_limit = config->poll_limit;
    device->parent.type = RT_Device_Class_Char;
    device->parent.ops = &dw_uart_ops;
    device->parent.user_data = device;

    return rt_device_register(&device->parent,
                              name,
                              RT_DEVICE_FLAG_RDWR);
}
