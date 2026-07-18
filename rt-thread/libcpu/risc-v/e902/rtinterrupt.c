/*
 * Keep the common E902 IRQ entry usable before the RT-Thread kernel is linked.
 * A kernel build supplies the strong implementations from src/irq.c.
 */

void rt_interrupt_enter(void) __attribute__((weak));
void rt_interrupt_leave(void) __attribute__((weak));

void rt_interrupt_enter(void)
{
}

void rt_interrupt_leave(void)
{
}
