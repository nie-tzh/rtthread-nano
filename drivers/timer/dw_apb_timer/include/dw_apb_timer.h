#ifndef DW_APB_TIMER_H
#define DW_APB_TIMER_H

#include <stdint.h>

#define DW_APB_TIMER_CHANNEL_STRIDE  0x14UL
#define DW_APB_TIMER_FREE_RUNNING_LOAD_COUNT  UINT32_MAX

enum dw_apb_timer_result
{
    DW_APB_TIMER_OK = 0,
    DW_APB_TIMER_ERROR_ARGUMENT = -1,
    DW_APB_TIMER_ERROR_STATE = -2
};

enum dw_apb_timer_mode
{
    DW_APB_TIMER_MODE_FREE_RUNNING = 0,
    DW_APB_TIMER_MODE_PERIODIC = 1
};

struct dw_apb_timer
{
    uintptr_t base;
    uint32_t initialized;
    uint32_t configured;
};

int dw_apb_timer_init(struct dw_apb_timer *timer, uintptr_t base);
int dw_apb_timer_configure(struct dw_apb_timer *timer,
                           enum dw_apb_timer_mode mode,
                           uint32_t load_count);
int dw_apb_timer_start(struct dw_apb_timer *timer);
int dw_apb_timer_stop(struct dw_apb_timer *timer);
int dw_apb_timer_get_current(const struct dw_apb_timer *timer,
                             uint32_t *current);
int dw_apb_timer_get_pending(const struct dw_apb_timer *timer,
                             uint32_t *pending);
int dw_apb_timer_acknowledge(struct dw_apb_timer *timer);

#endif
