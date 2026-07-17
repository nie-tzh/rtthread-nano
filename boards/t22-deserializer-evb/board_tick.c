#include "board.h"
#include "t22_serdes_timer.h"

struct board_tick_context
{
    board_tick_handler_t handler;
    void *parameter;
};

static struct board_tick_context board_tick;
static uint32_t board_tick_initialized;

_Static_assert(BOARD_TICK_TIMER_CHANNEL_INDEX <
               T22_SERDES_DW_TIMER_CHANNEL_COUNT,
               "Board Tick channel is outside the T22 timer block");

static int board_tick_map_result(int result)
{
    switch (result)
    {
    case T22_SERDES_TIMER_OK:
        return BOARD_TICK_OK;
    case T22_SERDES_TIMER_ERROR_ARGUMENT:
        return BOARD_TICK_ERROR_ARGUMENT;
    case T22_SERDES_TIMER_ERROR_STATE:
        return BOARD_TICK_ERROR_STATE;
    case T22_SERDES_TIMER_ERROR_FREQUENCY:
        return BOARD_TICK_ERROR_FREQUENCY;
    case T22_SERDES_TIMER_ERROR_INTERRUPT:
        return BOARD_TICK_ERROR_INTERRUPT;
    case T22_SERDES_TIMER_ERROR_DRIVER:
    default:
        return BOARD_TICK_ERROR_DRIVER;
    }
}

static void board_tick_dispatch(uint32_t channel, void *parameter)
{
    struct board_tick_context *context = parameter;

    (void)channel;
    context->handler(context->parameter);
}

int board_tick_init(uint32_t frequency_hz,
                    board_tick_handler_t handler,
                    void *parameter)
{
    int result;

    if ((frequency_hz == 0U) || (handler == 0))
    {
        return BOARD_TICK_ERROR_ARGUMENT;
    }
    if (board_tick_initialized != 0U)
    {
        return BOARD_TICK_ERROR_STATE;
    }

    result = t22_serdes_timer_init();
    if (result != T22_SERDES_TIMER_OK)
    {
        return board_tick_map_result(result);
    }

    board_tick.handler = handler;
    board_tick.parameter = parameter;
    result = t22_serdes_timer_configure_periodic(
        BOARD_TICK_TIMER_CHANNEL_INDEX,
        frequency_hz,
        board_tick_dispatch,
        &board_tick);
    if (result != T22_SERDES_TIMER_OK)
    {
        board_tick.handler = 0;
        board_tick.parameter = 0;
        return board_tick_map_result(result);
    }

    board_tick_initialized = 1U;
    return BOARD_TICK_OK;
}

int board_tick_start(void)
{
    int result;

    if (board_tick_initialized == 0U)
    {
        return BOARD_TICK_ERROR_STATE;
    }
    result = t22_serdes_timer_start(BOARD_TICK_TIMER_CHANNEL_INDEX);
    if (result != T22_SERDES_TIMER_OK)
    {
        return board_tick_map_result(result);
    }

    return BOARD_TICK_OK;
}

int board_tick_stop(void)
{
    int result;

    if (board_tick_initialized == 0U)
    {
        return BOARD_TICK_ERROR_STATE;
    }
    result = t22_serdes_timer_stop(BOARD_TICK_TIMER_CHANNEL_INDEX);
    if (result != T22_SERDES_TIMER_OK)
    {
        return board_tick_map_result(result);
    }

    return BOARD_TICK_OK;
}
