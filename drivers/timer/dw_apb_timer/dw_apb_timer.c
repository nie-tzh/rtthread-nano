#include <stddef.h>

#include "dw_apb_timer.h"

#define DW_APB_TIMER_CONTROL_ENABLE          (1U << 0)
#define DW_APB_TIMER_CONTROL_MODE            (1U << 1)
#define DW_APB_TIMER_CONTROL_INTERRUPT_MASK  (1U << 2)
#define DW_APB_TIMER_PENDING                 (1U << 0)

struct dw_apb_timer_registers
{
    volatile uint32_t load_count;
    const volatile uint32_t current_value;
    volatile uint32_t control;
    const volatile uint32_t eoi;
    const volatile uint32_t interrupt_status;
};

_Static_assert(offsetof(struct dw_apb_timer_registers, load_count) == 0x00U,
               "DW APB Timer load-count offset mismatch");
_Static_assert(offsetof(struct dw_apb_timer_registers, current_value) == 0x04U,
               "DW APB Timer current-value offset mismatch");
_Static_assert(offsetof(struct dw_apb_timer_registers, control) == 0x08U,
               "DW APB Timer control offset mismatch");
_Static_assert(offsetof(struct dw_apb_timer_registers, eoi) == 0x0CU,
               "DW APB Timer EOI offset mismatch");
_Static_assert(offsetof(struct dw_apb_timer_registers, interrupt_status) ==
               0x10U,
               "DW APB Timer interrupt-status offset mismatch");
_Static_assert(sizeof(struct dw_apb_timer_registers) ==
               DW_APB_TIMER_CHANNEL_STRIDE,
               "DW APB Timer channel stride mismatch");

static struct dw_apb_timer_registers *timer_get_registers(
    const struct dw_apb_timer *timer)
{
    return (struct dw_apb_timer_registers *)timer->base;
}

static void timer_fence(void)
{
    __asm__ volatile ("fence iorw, iorw" ::: "memory");
}

static int timer_is_initialized(const struct dw_apb_timer *timer)
{
    return (timer != 0) && (timer->initialized != 0U);
}

int dw_apb_timer_init(struct dw_apb_timer *timer, uintptr_t base)
{
    struct dw_apb_timer_registers *registers;
    uint32_t eoi;

    if ((timer == 0) || (base == 0U) || ((base & 0x3U) != 0U))
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }

    timer->base = base;
    timer->initialized = 1U;
    timer->configured = 0U;
    registers = timer_get_registers(timer);

    registers->control = DW_APB_TIMER_CONTROL_INTERRUPT_MASK;
    eoi = registers->eoi;
    (void)eoi;
    timer_fence();

    return DW_APB_TIMER_OK;
}

int dw_apb_timer_configure(struct dw_apb_timer *timer,
                           enum dw_apb_timer_mode mode,
                           uint32_t load_count)
{
    struct dw_apb_timer_registers *registers;
    uint32_t control = DW_APB_TIMER_CONTROL_INTERRUPT_MASK;

    if ((timer == 0) || (load_count == 0U) ||
        ((mode != DW_APB_TIMER_MODE_FREE_RUNNING) &&
         (mode != DW_APB_TIMER_MODE_PERIODIC)) ||
        ((mode == DW_APB_TIMER_MODE_FREE_RUNNING) &&
         (load_count != DW_APB_TIMER_FREE_RUNNING_LOAD_COUNT)))
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }
    if (!timer_is_initialized(timer))
    {
        return DW_APB_TIMER_ERROR_STATE;
    }

    registers = timer_get_registers(timer);
    registers->control = DW_APB_TIMER_CONTROL_INTERRUPT_MASK;
    if (mode == DW_APB_TIMER_MODE_PERIODIC)
    {
        control |= DW_APB_TIMER_CONTROL_MODE;
    }
    registers->load_count = load_count;
    registers->control = control;
    timer->configured = 1U;
    timer_fence();

    return DW_APB_TIMER_OK;
}

int dw_apb_timer_start(struct dw_apb_timer *timer)
{
    struct dw_apb_timer_registers *registers;
    uint32_t eoi;
    uint32_t control;

    if (timer == 0)
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }
    if (!timer_is_initialized(timer) || (timer->configured == 0U))
    {
        return DW_APB_TIMER_ERROR_STATE;
    }

    registers = timer_get_registers(timer);
    eoi = registers->eoi;
    (void)eoi;

    control = registers->control;
    control &= ~DW_APB_TIMER_CONTROL_INTERRUPT_MASK;
    control |= DW_APB_TIMER_CONTROL_ENABLE;
    registers->control = control;
    timer_fence();

    return DW_APB_TIMER_OK;
}

int dw_apb_timer_stop(struct dw_apb_timer *timer)
{
    struct dw_apb_timer_registers *registers;
    uint32_t eoi;
    uint32_t control;

    if (timer == 0)
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }
    if (!timer_is_initialized(timer))
    {
        return DW_APB_TIMER_ERROR_STATE;
    }

    registers = timer_get_registers(timer);
    control = registers->control | DW_APB_TIMER_CONTROL_INTERRUPT_MASK;
    registers->control = control;
    registers->control = control & ~DW_APB_TIMER_CONTROL_ENABLE;
    eoi = registers->eoi;
    (void)eoi;
    timer_fence();

    return DW_APB_TIMER_OK;
}

int dw_apb_timer_get_current(const struct dw_apb_timer *timer,
                             uint32_t *current)
{
    if ((timer == 0) || (current == 0))
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }
    if (!timer_is_initialized(timer))
    {
        return DW_APB_TIMER_ERROR_STATE;
    }

    *current = timer_get_registers(timer)->current_value;
    return DW_APB_TIMER_OK;
}

int dw_apb_timer_get_pending(const struct dw_apb_timer *timer,
                             uint32_t *pending)
{
    if ((timer == 0) || (pending == 0))
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }
    if (!timer_is_initialized(timer))
    {
        return DW_APB_TIMER_ERROR_STATE;
    }

    *pending = timer_get_registers(timer)->interrupt_status &
               DW_APB_TIMER_PENDING;
    return DW_APB_TIMER_OK;
}

int dw_apb_timer_acknowledge(struct dw_apb_timer *timer)
{
    uint32_t eoi;

    if (timer == 0)
    {
        return DW_APB_TIMER_ERROR_ARGUMENT;
    }
    if (!timer_is_initialized(timer))
    {
        return DW_APB_TIMER_ERROR_STATE;
    }

    eoi = timer_get_registers(timer)->eoi;
    (void)eoi;
    timer_fence();

    return DW_APB_TIMER_OK;
}
