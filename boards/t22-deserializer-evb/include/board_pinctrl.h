#ifndef T22_DESERIALIZER_EVB_PINCTRL_H
#define T22_DESERIALIZER_EVB_PINCTRL_H

#include "pinctrl.h"

void board_pinctrl_init(void);
int board_uart2_pinctrl_select_state(enum pinctrl_state_id state_id);

#endif
