#include <rthw.h>
#include <rtthread.h>

#include "dw_uart_device.h"

static rt_err_t dw_uart_device_init(rt_device_t rt_device)
{
    struct dw_uart_device *device =
        (struct dw_uart_device *)rt_device;

    if (dw_uart_hw_init(&device->uart, &device->config) != DW_APB_UART_OK)
    {
        return -RT_EIO;
    }

    return RT_EOK;
}

static void dw_uart_irq_handler(int vector, void *parameter)
{
    struct dw_uart_device *device = parameter;
    rt_size_t received = 0U;
    int ch;

    (void)vector;

    while ((ch = dw_apb_uart_getc(&device->uart)) >= 0)
    {
        rt_uint16_t next = (rt_uint16_t)(device->rx_write + 1U);

        if (next >= DW_UART_RX_BUFFER_SIZE)
        {
            next = 0U;
        }

        if (next != device->rx_read)
        {
            device->rx_buffer[device->rx_write] = (rt_uint8_t)ch;
            device->rx_write = next;
            received++;
        }
        /* Drain the hardware FIFO even when the software buffer is full. */
    }

    if ((received != 0U) && (device->parent.rx_indicate != RT_NULL))
    {
        device->parent.rx_indicate(&device->parent, received);
    }
}

static rt_err_t dw_uart_open(rt_device_t rt_device, rt_uint16_t oflag)
{
    struct dw_uart_device *device =
        (struct dw_uart_device *)rt_device;

    /* RT_DEVICE_OFLAG_MASK does not include the STREAM open option. */
    rt_device->open_flag =
        oflag & (RT_DEVICE_OFLAG_MASK | RT_DEVICE_FLAG_STREAM);

    if ((oflag & RT_DEVICE_FLAG_INT_RX) == 0U)
    {
        return RT_EOK;
    }

    if (device->rx_irq_installed == 0U)
    {
        device->rx_read = 0U;
        device->rx_write = 0U;
        (void)rt_hw_interrupt_install(device->irq,
                                       dw_uart_irq_handler,
                                       device,
                                       "uart");
        device->rx_irq_installed = 1U;
    }

    dw_apb_uart_set_rx_interrupt(&device->uart, 1U);
    rt_hw_interrupt_umask(device->irq);
    return RT_EOK;
}

static rt_err_t dw_uart_close(rt_device_t rt_device)
{
    struct dw_uart_device *device =
        (struct dw_uart_device *)rt_device;

    if (device->rx_irq_installed != 0U)
    {
        dw_apb_uart_set_rx_interrupt(&device->uart, 0U);
        rt_hw_interrupt_mask(device->irq);
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
    rt_base_t level;

    (void)pos;

    if ((rt_device->open_flag & RT_DEVICE_FLAG_INT_RX) != 0U)
    {
        level = rt_hw_interrupt_disable();
        while ((count < size) &&
               (device->rx_read != device->rx_write))
        {
            data[count++] = device->rx_buffer[device->rx_read];
            device->rx_read++;
            if (device->rx_read >= DW_UART_RX_BUFFER_SIZE)
            {
                device->rx_read = 0U;
            }
        }
        rt_hw_interrupt_enable(level);
        return count;
    }

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
    .init = dw_uart_device_init,
    .open = dw_uart_open,
    .close = dw_uart_close,
    .read = dw_uart_read,
    .write = dw_uart_write,
    .control = RT_NULL
};

rt_err_t dw_uart_device_register(
    struct dw_uart_device *device,
    const char *name,
    const struct dw_apb_uart_config *config,
    int irq)
{
    device->config.base = config->base;
    device->config.clock_hz = config->clock_hz;
    device->config.baud_rate = config->baud_rate;
    device->config.poll_limit = config->poll_limit;
    device->irq = irq;
    device->parent.type = RT_Device_Class_Char;
    device->parent.ops = &dw_uart_ops;
    device->parent.user_data = device;
    device->rx_read = 0U;
    device->rx_write = 0U;
    device->rx_irq_installed = 0U;

    return rt_device_register(&device->parent,
                              name,
                              RT_DEVICE_FLAG_RDWR |
                              RT_DEVICE_FLAG_INT_RX);
}
