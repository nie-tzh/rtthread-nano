#ifndef T22_DESERIALIZER_EVB_BOARD_H
#define T22_DESERIALIZER_EVB_BOARD_H

#include <stdint.h>

#define BOARD_TICK_TIMER_CHANNEL_INDEX  0U

enum board_tick_result
{
    BOARD_TICK_OK = 0,
    BOARD_TICK_ERROR_ARGUMENT = -1,
    BOARD_TICK_ERROR_STATE = -2,
    BOARD_TICK_ERROR_FREQUENCY = -3,
    BOARD_TICK_ERROR_INTERRUPT = -4,
    BOARD_TICK_ERROR_DRIVER = -5
};

typedef void (*board_tick_handler_t)(void *parameter);

void board_init(void);
int board_early_putc(char ch);
int board_early_puts(const char *text);
int board_tick_init(uint32_t frequency_hz,
                    board_tick_handler_t handler,
                    void *parameter);
int board_tick_start(void);
int board_tick_stop(void);

#endif
