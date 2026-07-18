#ifndef E902_CONTEXT_H
#define E902_CONTEXT_H

#include "e902_exception.h"

#define E902_CONTEXT_SWITCH_IRQ          3
#define E902_CONTEXT_INITIAL_MSTATUS     0x00001880
#define E902_CONTEXT_STACK_ALIGNMENT     4
#define E902_CONTEXT_REGISTER_FILL       0xDEADBEEF
#define E902_CONTEXT_ERROR_STATE_VALUE   -1
#define E902_CONTEXT_ERROR_IRQ_VALUE     -2

#if !defined(__ASSEMBLER__)

#include <stdint.h>

typedef unsigned long e902_ubase_t;
typedef long e902_base_t;
typedef struct e902_exception_frame e902_context_frame_t;

enum e902_context_result
{
    E902_CONTEXT_OK = 0,
    E902_CONTEXT_ERROR_STATE = E902_CONTEXT_ERROR_STATE_VALUE,
    E902_CONTEXT_ERROR_INTERRUPT = E902_CONTEXT_ERROR_IRQ_VALUE
};

_Static_assert(sizeof(e902_ubase_t) == sizeof(uint32_t),
               "E902 context requires a 32-bit base type");
_Static_assert(sizeof(e902_context_frame_t) == E902_EXCEPTION_FRAME_SIZE,
               "E902 thread and interrupt frames must match");
_Static_assert((E902_EXCEPTION_FRAME_SIZE %
                E902_CONTEXT_STACK_ALIGNMENT) == 0,
               "E902 context frame breaks stack alignment");

extern volatile e902_ubase_t rt_interrupt_from_thread;
extern volatile e902_ubase_t rt_interrupt_to_thread;
extern volatile e902_ubase_t rt_thread_switch_interrupt_flag;

extern volatile uint32_t g_e902_context_request_count;
extern volatile uint32_t g_e902_context_irq_count;
extern volatile uint32_t g_e902_context_switch_count;
extern volatile int32_t g_e902_context_last_error;
extern volatile uint32_t g_e902_context_halted;

int e902_context_switch_init(void);

uint8_t *rt_hw_stack_init(void *entry,
                          void *parameter,
                          uint8_t *stack_addr,
                          void *exit);
void rt_hw_context_switch(e902_ubase_t from, e902_ubase_t to);
void rt_hw_context_switch_interrupt(e902_ubase_t from, e902_ubase_t to);
void rt_hw_context_switch_to(e902_ubase_t to);
e902_base_t rt_hw_interrupt_disable(void);
void rt_hw_interrupt_enable(e902_base_t level);

#endif

#endif
