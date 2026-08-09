#ifndef T22_DESERIALIZER_PINCTRL_H
#define T22_DESERIALIZER_PINCTRL_H

#define T22_DESERIALIZER_PIN_COUNT              17U

#define T22_DESERIALIZER_UART2_RX_PIN           4U
#define T22_DESERIALIZER_UART2_RX_FUNCTION      9U

#define T22_DESERIALIZER_UART2_TX_PIN           14U
#define T22_DESERIALIZER_UART2_TX_FUNCTION      2U

#include "t22_pinctrl.h"

extern const struct t22_pinctrl_soc_data t22_deserializer_pinctrl_data;

#endif
