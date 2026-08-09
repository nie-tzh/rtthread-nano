#include "board_pinctrl.h"
#include "pinctrl.h"
#include "t22_deserializer_pinctrl.h"
#include "t22_pinctrl.h"

#define BOARD_PIN_CONTROL_MASK       0x0001F9FFU
#define BOARD_UART2_RX_PAD_CONFIG    0x00004AA4U
/* Production MFP14 uses the eFuse=0 ECO polarity. */
#define BOARD_UART2_TX_PAD_CONFIG    0x000048A4U

enum board_uart2_setting
{
    BOARD_UART2_SETTING_RX = 0,
    BOARD_UART2_SETTING_TX
};

static struct t22_pinctrl board_pinctrl;

static const struct pinctrl_setting board_uart2_settings[] =
{
    [BOARD_UART2_SETTING_RX] =
    {
        .pin = T22_DESERIALIZER_UART2_RX_PIN,
        .function = T22_DESERIALIZER_UART2_RX_FUNCTION,
        .config = BOARD_UART2_RX_PAD_CONFIG
    },
    [BOARD_UART2_SETTING_TX] =
    {
        .pin = T22_DESERIALIZER_UART2_TX_PIN,
        .function = T22_DESERIALIZER_UART2_TX_FUNCTION,
        .config = BOARD_UART2_TX_PAD_CONFIG
    }
};

static const struct pinctrl_state board_uart2_states[] =
{
    {
        .id = PINCTRL_STATE_DEFAULT,
        .settings = board_uart2_settings,
        .nsettings = sizeof(board_uart2_settings) /
                     sizeof(board_uart2_settings[0])
    }
};

static const struct pinctrl board_uart2_pinctrl =
{
    .pctldev = &board_pinctrl.pctldev,
    .states = board_uart2_states,
    .nstates = sizeof(board_uart2_states) /
               sizeof(board_uart2_states[0])
};

void board_pinctrl_init(void)
{
    t22_pinctrl_init(&board_pinctrl,
                     &t22_deserializer_pinctrl_data,
                     BOARD_PIN_CONTROL_MASK);
}

int board_uart2_pinctrl_select_state(enum pinctrl_state_id state_id)
{
    return pinctrl_select_state(&board_uart2_pinctrl, state_id);
}
