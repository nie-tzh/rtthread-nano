#include "board_reset.h"
#include "reset.h"
#include "t22_deserializer_reset.h"
#include "t22_reset.h"
#include "t22_serdes_reset.h"

static struct t22_reset board_reset_controller;

static const struct reset_control board_uart2_reset_control =
{
    .rcdev = &board_reset_controller.rcdev,
    .id = T22_SERDES_RESET_UART2
};

void board_reset_init(void)
{
    t22_reset_init(&board_reset_controller,
                   &t22_deserializer_reset_data);
}

int board_uart2_reset(void)
{
    return reset_control_reset(&board_uart2_reset_control);
}
