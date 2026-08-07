#ifndef T22_DESERIALIZER_EVB_BOARD_H
#define T22_DESERIALIZER_EVB_BOARD_H

#include <stdint.h>

#define BOARD_TICK_TIMER_CHANNEL_INDEX  0U

void board_early_init(void);
void rt_hw_board_init(void);
int rt_hw_tick_init(void);

#endif
