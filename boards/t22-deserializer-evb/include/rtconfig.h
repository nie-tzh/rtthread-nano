#ifndef __RTTHREAD_CFG_H__
#define __RTTHREAD_CFG_H__

#define RT_THREAD_PRIORITY_MAX  32
#define RT_TICK_PER_SECOND      1000
#define RT_ALIGN_SIZE           4
#define RT_NAME_MAX             8
/* IRQ frames and ISR call chains use the interrupted thread's stack. */
#define IDLE_THREAD_STACK_SIZE  512
#define RT_USING_OVERFLOW_CHECK

#endif
