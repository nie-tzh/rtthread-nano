#include <stddef.h>
#include <stdint.h>
#include <rthw.h>

#include "dw_apb_timer.h"
#include "t22_serdes.h"
#include "t22_serdes_irq.h"
#include "t22_serdes_timer.h"

#define T22_SERDES_DW_TIMER_COMBINED_STATUS      0xA0UL
#define T22_SERDES_DW_TIMER_CHANNEL_MASK         \
    ((1U << T22_SERDES_DW_TIMER_CHANNEL_COUNT) - 1U)
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

struct t22_serdes_dw_timer_registers
{
    uint8_t reserved[T22_SERDES_DW_TIMER_COMBINED_STATUS];
    const volatile uint32_t combined_interrupt_status;
};

#define T22_SERDES_DW_TIMER \
    ((struct t22_serdes_dw_timer_registers *) \
     (uintptr_t)T22_SERDES_TIMER_BASE)

_Static_assert(offsetof(struct t22_serdes_dw_timer_registers,
                        combined_interrupt_status) ==
               T22_SERDES_DW_TIMER_COMBINED_STATUS,
               "T22 DW Timer combined-status offset mismatch");

static struct dw_apb_timer
    t22_timers[T22_SERDES_DW_TIMER_CHANNEL_COUNT];
static struct t22_serdes_timer_callback
    t22_timer_callbacks[T22_SERDES_DW_TIMER_CHANNEL_COUNT];
static volatile uint32_t t22_timer_active_mask;
static uint32_t t22_timer_initialized;

static uintptr_t timer_channel_base(uint32_t channel)
{
    return T22_SERDES_TIMER_BASE +
           ((uintptr_t)channel * DW_APB_TIMER_CHANNEL_STRIDE);
}

static uint32_t timer_combined_status(void)
{
    return T22_SERDES_DW_TIMER->combined_interrupt_status &
           T22_SERDES_DW_TIMER_CHANNEL_MASK;
}

static int timer_channel_is_valid(uint32_t channel)
{
    return channel < T22_SERDES_DW_TIMER_CHANNEL_COUNT;
}

static void t22_serdes_timer_irq_handler(
    int irq,
    void *parameter)
{
    uint32_t channel;
    uint32_t pending;

    (void)irq;
    (void)parameter;

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

        dw_apb_timer_acknowledge(&t22_timers[channel]);
        if (((t22_timer_active_mask & channel_mask) != 0U) &&
            (t22_timer_callbacks[channel].handler != 0))
        {
            t22_timer_callbacks[channel].handler(
                channel, t22_timer_callbacks[channel].parameter);
        }
        else
        {
            dw_apb_timer_stop(&t22_timers[channel]);
            t22_timer_active_mask &= ~channel_mask;
        }
    }

    if (t22_timer_active_mask == 0U)
    {
        rt_hw_interrupt_mask(T22_SERDES_IRQ_DW_TIMER);
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

    (void)rt_hw_interrupt_install(
        T22_SERDES_IRQ_DW_TIMER,
        t22_serdes_timer_irq_handler,
        0,
        "timer");

    t22_timer_active_mask = 0U;
    t22_timer_initialized = 1U;

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_config_periodic(
    uint32_t channel,
    uint32_t frequency_hz,
    t22_serdes_timer_handler_t handler,
    void *parameter)
{
    uint32_t load_count;
    rt_base_t previous_mstatus;
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

    previous_mstatus = rt_hw_interrupt_disable();
    if ((t22_timer_active_mask & (1U << channel)) != 0U)
    {
        rt_hw_interrupt_enable(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    result = dw_apb_timer_configure(
        &t22_timers[channel],
        DW_APB_TIMER_MODE_PERIODIC,
        load_count);
    if (result != DW_APB_TIMER_OK)
    {
        rt_hw_interrupt_enable(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_DRIVER;
    }

    t22_timer_callbacks[channel].parameter = parameter;
    t22_timer_callbacks[channel].handler = handler;
    rt_hw_interrupt_enable(previous_mstatus);

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_start(uint32_t channel)
{
    uint32_t channel_mask;
    rt_base_t previous_mstatus;
    uint32_t enable_shared_irq;

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
    previous_mstatus = rt_hw_interrupt_disable();
    if ((t22_timer_active_mask & channel_mask) != 0U)
    {
        rt_hw_interrupt_enable(previous_mstatus);
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    dw_apb_timer_start(&t22_timers[channel]);

    enable_shared_irq = (t22_timer_active_mask == 0U);
    t22_timer_active_mask |= channel_mask;
    if (enable_shared_irq != 0U)
    {
        rt_hw_interrupt_umask(T22_SERDES_IRQ_DW_TIMER);
    }
    rt_hw_interrupt_enable(previous_mstatus);

    return T22_SERDES_TIMER_OK;
}

int t22_serdes_timer_stop(uint32_t channel)
{
    rt_base_t previous_mstatus;

    if (!timer_channel_is_valid(channel))
    {
        return T22_SERDES_TIMER_ERROR_ARGUMENT;
    }
    if (t22_timer_initialized == 0U)
    {
        return T22_SERDES_TIMER_ERROR_STATE;
    }

    previous_mstatus = rt_hw_interrupt_disable();
    dw_apb_timer_stop(&t22_timers[channel]);

    t22_timer_active_mask &= ~(1U << channel);
    if (t22_timer_active_mask == 0U)
    {
        rt_hw_interrupt_mask(T22_SERDES_IRQ_DW_TIMER);
    }
    rt_hw_interrupt_enable(previous_mstatus);

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
    *current = dw_apb_timer_get_current(&t22_timers[channel]);

    return T22_SERDES_TIMER_OK;
}
