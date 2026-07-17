#ifndef E902_CLIC_H
#define E902_CLIC_H

#include <stdint.h>

#include "e902_exception.h"

#define E902_CLIC_BASE                  0xE0800000UL
#define E902_CLIC_VECTOR_ALIGNMENT      64U

enum e902_clic_result
{
    E902_CLIC_OK = 0,
    E902_CLIC_ERROR_ARGUMENT = -1,
    E902_CLIC_ERROR_INFO = -2,
    E902_CLIC_ERROR_VECTOR = -3,
    E902_CLIC_ERROR_STATE = -4,
    E902_CLIC_ERROR_CONFIG = -5
};

enum e902_clic_trigger
{
    E902_CLIC_TRIGGER_HIGH_LEVEL = 0,
    E902_CLIC_TRIGGER_POSITIVE_EDGE = 1,
    E902_CLIC_TRIGGER_NEGATIVE_EDGE = 3
};

typedef void (*e902_irq_handler_t)(
    uint32_t irq,
    void *parameter,
    const struct e902_exception_frame *frame);

struct e902_irq_descriptor
{
    e902_irq_handler_t handler;
    void *parameter;
};

struct e902_clic_info
{
    uint32_t raw;
    uint32_t hardware_irq_count;
    uint32_t vector_count;
    uint8_t control_bits;
};

extern volatile uint32_t g_e902_irq_count;
extern volatile uint32_t g_e902_last_irq;
extern volatile uint32_t g_e902_unhandled_irq_count;

int e902_clic_init(const uintptr_t *vector_table,
                   uint32_t vector_count,
                   struct e902_irq_descriptor *descriptors,
                   uint32_t descriptor_count);
int e902_clic_register_irq(uint32_t irq,
                           e902_irq_handler_t handler,
                           void *parameter,
                           enum e902_clic_trigger trigger,
                           uint8_t control);
int e902_clic_enable_irq(uint32_t irq);
int e902_clic_disable_irq(uint32_t irq);
int e902_clic_set_pending(uint32_t irq);
int e902_clic_clear_pending(uint32_t irq);
int e902_clic_get_pending(uint32_t irq, uint32_t *pending);
const struct e902_clic_info *e902_clic_get_info(void);

uint32_t e902_global_irq_disable(void);
void e902_global_irq_enable(void);
void e902_global_irq_restore(uint32_t previous_mstatus);

void e902_irq_dispatch(struct e902_exception_frame *frame);

#endif
