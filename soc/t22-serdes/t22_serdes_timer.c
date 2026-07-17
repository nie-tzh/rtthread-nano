#include <stdint.h>

#include "dw_apb_timer.h"
#include "e902_clic.h"
#include "t22_serdes.h"
#include "t22_serdes_irq.h"
#include "t22_serdes_timer.h"

#define T22_SERDES_DW_TIMER_COMBINED_STATUS      0xA0UL
#define T22_SERDES_DW_TIMER_CHANNEL_MASK         \
    ((1U << T22_SERDES_DW_TIMER_CHANNEL_COUNT) - 1U)
#define T22_SERDES_DW_TIMER_INTERRUPT_CONTROL    0xFFU

_Static_assert(T22_SERDES_DW_TIMER_CHANNEL_COUNT < 32U,
               "T22 DW Timer channel mask exceeds 32 bits");
_Static_assert((T22_SERDES_DW_TIMER_CHANNEL_COUNT *
                DW_APB_TIMER_CHANNEL_STRIDE) ==
               T22_SERDES_DW_TIMER_COMBINED_STATUS,
               "T22 DW Timer common-register offset mismatch");

struct t22_serdes_timer_callback
{
    t22_serdes_timer_handler_t handler;
    void *parameter;
};

static struct dw_apb_timer
    t22_timers[T22_SERDES_DW_TIMER_CHANNEL_COUNT];
static struct t22_serdes_timer_callback
    t22_timer_callbacks[T22_SERDES_DW_TIMER_CHANNEL_COUNT];
static volatile uint32_t t22_timer_active_mask;
static uint32_t t22_timer_initialized;

static uintptr_t timer_channel_base(uint32_t channel)
{
    return T22_SERDES_DW_TIMER_BASE +
           ((uintptr_t)channel * DW_APB_TIMER_CHANNEL_STRIDE);
}

static uint32_t timer_combined_status(void)
{
    return *(volatile uint32_t *)(
               T22_SERDES_DW_TIMER_BASE +
               T22_SERDES_DW_TIMER_COMBINED_STATUS) &
           T22_SERDES_DW_TIMER_CHANNEL_MASK;
}

static int timer_channel_is_valid(uint32_t channel)
{
    return channel < T22_SERDES_DW_TIMER_CHANNEL_COUNT;
}

static void t22_serdes_timer_irq_handler(
    uint32_t irq,
    void *parameter,
    const struct e902_exception_frame *frame)
{
    uint32_t channel;
    uint32_t pending;

    (void)parameter;
    (void)frame;

    if (irq != T22_SERDES_IRQ_DW_TIMER)
    {
        return;
    }

    pending = timer_combined_status();
    for (channel = 0U;
         channel < T22_SERDES_DW_TIMER_CHANNEL_COUNT;
         channel++)
    {
        uint32_t channel_mask = 1U << channel;

        if ((pending & channel_mask) == 0U)
        {
            continue;
        }

        (void)dw_apb_timer_acknowledge(&t22_timers[channel]);
        if (((t22_timer_active_mask & channel_mask) != 0U) &&
            (t22_timer_callbacks[channel].handler != 0))
        {
            t22_timer_callbacks[channel].handler(
                channel, t22_timer_callbacks[channel].parameter);
        }
        else
        {
            (void)dw_apb_timer_stop(&t22_timers[channel]);
            t22_timer_active_mask &= ~channel_mask;
        }
    }

    if (t22_timer_active_mask == 0U)
    {
        (void)e902_clic_disable_irq(T22_SERDES_IRQ_DW_TIMER);
    }
}

int t22_serdes_timer_init(void)
{
    uint32_t channel;
    int result;

    if (t22_timer_initialized != 0U)
    {
        return T22_SERDES_TIMER_OK;
    }

    for (channel = 0U;
         channel < T22_SERDES_DW_TIMER_CHANNEL_COUNT;
         channel++)
    {
        result = dw_apb_timer_init(&t22_timers[channel],
                                   timer_channel_base(channel));
        if (result != DW_APB_TIMER_OK)
        {
            return T22_SERDES_TIMER_ERROR_DRIVER;
        }

        t22_timer_callbacks[channel].handler = 0;
        t22_timer_callbacks[channel].parameter = 0;
    }

    result = e902_clic_register_irq(
        T22_SERDES_IRQ_DW_TIMER,
        t22_serdes_timer_irq_handler,
        0,
        E902_CLIC_TRIGGER_HIGH_LEVEL,
        T22_SERDES_DW_TIMER_INTERRUPT_CONTROL);
    if (result != E902_CLIC_OK)
    {
        return T22_SERDES_TIMER_ERROR_INTERRUPT;
    }

    t22_timer_active_mask = 0U;
    t22_timer_initialized = 1U;

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_configure_periodic(
    uint32_t channel,
    uint32_t frequency_hz,
    t22_serdes_timer_handler_t handler,
    void *parameter)
{
    uint32_t load_count;
    uint32_t previous_mstatus;
    int result;

    if (!timer_channel_is_valid(channel) || (frequency_hz == 0U) ||
        (handler == 0))
    {
        return T22_SERDES_TIMER_ERROR_ARGUMENT;
    }
    if (t22_timer_initialized == 0U)
    {
        return T22_SERDES_TIMER_ERROR_STATE;
    }
    if ((T22_SERDES_APB_CLOCK_HZ % frequency_hz) != 0U)
    {
        return T22_SERDES_TIMER_ERROR_FREQUENCY;
    }

    load_count = T22_SERDES_APB_CLOCK_HZ / frequency_hz;
    if (load_count == 0U)
    {
        return T22_SERDES_TIMER_ERROR_FREQUENCY;
    }

    previous_mstatus = e902_global_irq_disable();
    if ((t22_timer_active_mask & (1U << channel)) != 0U)
    {
        e902_global_irq_restore(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    result = dw_apb_timer_configure(
        &t22_timers[channel],
        DW_APB_TIMER_MODE_PERIODIC,
        load_count);
    if (result != DW_APB_TIMER_OK)
    {
        e902_global_irq_restore(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_DRIVER;
    }

    t22_timer_callbacks[channel].parameter = parameter;
    t22_timer_callbacks[channel].handler = handler;
    e902_global_irq_restore(previous_mstatus);

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_start(uint32_t channel)
{
    uint32_t channel_mask;
    uint32_t previous_mstatus;
    int result;

    if (!timer_channel_is_valid(channel))
    {
        return T22_SERDES_TIMER_ERROR_ARGUMENT;
    }
    if ((t22_timer_initialized == 0U) ||
        (t22_timer_callbacks[channel].handler == 0))
    {
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    channel_mask = 1U << channel;
    previous_mstatus = e902_global_irq_disable();
    if ((t22_timer_active_mask & channel_mask) != 0U)
    {
        e902_global_irq_restore(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    result = dw_apb_timer_start(&t22_timers[channel]);
    if (result != DW_APB_TIMER_OK)
    {
        e902_global_irq_restore(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_DRIVER;
    }

    t22_timer_active_mask |= channel_mask;
    result = e902_clic_enable_irq(T22_SERDES_IRQ_DW_TIMER);
    if (result != E902_CLIC_OK)
    {
        t22_timer_active_mask &= ~channel_mask;
        (void)dw_apb_timer_stop(&t22_timers[channel]);
        e902_global_irq_restore(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_INTERRUPT;
    }
    e902_global_irq_restore(previous_mstatus);

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_stop(uint32_t channel)
{
    uint32_t previous_mstatus;
    int result;

    if (!timer_channel_is_valid(channel))
    {
        return T22_SERDES_TIMER_ERROR_ARGUMENT;
    }
    if (t22_timer_initialized == 0U)
    {
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    previous_mstatus = e902_global_irq_disable();
    result = dw_apb_timer_stop(&t22_timers[channel]);
    if (result != DW_APB_TIMER_OK)
    {
        e902_global_irq_restore(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_DRIVER;
    }

    t22_timer_active_mask &= ~(1U << channel);
    if (t22_timer_active_mask == 0U)
    {
        (void)e902_clic_disable_irq(T22_SERDES_IRQ_DW_TIMER);
    }
    e902_global_irq_restore(previous_mstatus);

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_get_current(uint32_t channel, uint32_t *current)
{
    if (!timer_channel_is_valid(channel) || (current == 0))
    {
        return T22_SERDES_TIMER_ERROR_ARGUMENT;
    }
    if (t22_timer_initialized == 0U)
    {
        return T22_SERDES_TIMER_ERROR_STATE;
    }
    if (dw_apb_timer_get_current(&t22_timers[channel], current) !=
        DW_APB_TIMER_OK)
    {
        return T22_SERDES_TIMER_ERROR_DRIVER;
    }

    return T22_SERDES_TIMER_OK;
}
