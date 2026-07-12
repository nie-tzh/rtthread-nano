#include <stdint.h>

#include "board.h"

volatile uint32_t g_boot_marker = 0x45563930U;
volatile uint32_t g_runtime_marker;
volatile int32_t g_uart_status = -1;

int main(void)
{
    g_runtime_marker = g_boot_marker;
    g_uart_status = board_early_puts("T22 deserializer EVB booting...\n");

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
