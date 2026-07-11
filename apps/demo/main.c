#include <stdint.h>

volatile uint32_t g_boot_marker = 0x45563930U;
volatile uint32_t g_runtime_marker;

int main(void)
{
    g_runtime_marker = g_boot_marker;

    for (;;)
    {
        __asm__ volatile ("nop");
    }
}
