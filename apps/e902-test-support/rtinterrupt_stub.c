/* Standalone validation links the BSP IRQ adapter, but not the RT IRQ core. */

void rt_interrupt_enter(void) __attribute__((weak));
void rt_interrupt_leave(void) __attribute__((weak));

void rt_interrupt_enter(void)
{
}

void rt_interrupt_leave(void)
{
}
