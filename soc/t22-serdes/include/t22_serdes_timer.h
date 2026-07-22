#ifndef T22_SERDES_TIMER_H
#define T22_SERDES_TIMER_H

#include <stdint.h>

#define T22_SERDES_DW_TIMER_CHANNEL_COUNT  8U

enum t22_serdes_timer_result
{
    T22_SERDES_TIMER_OK = 0,
    T22_SERDES_TIMER_ERROR_ARGUMENT = -1,
    T22_SERDES_TIMER_ERROR_STATE = -2,
    T22_SERDES_TIMER_ERROR_FREQUENCY = -3,
    T22_SERDES_TIMER_ERROR_DRIVER = -4
};

typedef void (*t22_serdes_timer_handler_t)(uint32_t channel,
                                           void *parameter);

int t22_serdes_timer_init(void);
int t22_serdes_timer_config_periodic(
    uint32_t channel,
    uint32_t frequency_hz,
    t22_serdes_timer_handler_t handler,
    void *parameter);
int t22_serdes_timer_start(uint32_t channel);
int t22_serdes_timer_stop(uint32_t channel);
int t22_serdes_timer_get_current(uint32_t channel, uint32_t *current);

#endif
