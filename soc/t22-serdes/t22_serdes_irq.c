#include <stdint.h>

#include "e902_clic.h"
#include "t22_serdes_irq.h"

extern const uintptr_t t22_serdes_irq_vectors[];
extern const uintptr_t t22_serdes_irq_vectors_end[];

static struct e902_irq_descriptor
    t22_serdes_irq_descriptors[T22_SERDES_IRQ_VECTOR_COUNT];

_Static_assert(T22_SERDES_IRQ_HIGHEST < T22_SERDES_IRQ_VECTOR_COUNT,
               "T22 IRQ vector table does not cover the highest IRQ");

int t22_serdes_irq_init(void)
{
    const struct e902_clic_info *info;
    uintptr_t vector_size =
        (uintptr_t)t22_serdes_irq_vectors_end -
        (uintptr_t)t22_serdes_irq_vectors;
    int result;

    if (vector_size !=
        (T22_SERDES_IRQ_VECTOR_COUNT * sizeof(t22_serdes_irq_vectors[0])))
    {
        return E902_CLIC_ERROR_VECTOR;
    }

    result = e902_clic_init(t22_serdes_irq_vectors,
                            T22_SERDES_IRQ_VECTOR_COUNT,
                            t22_serdes_irq_descriptors,
                            T22_SERDES_IRQ_VECTOR_COUNT);
    if (result != E902_CLIC_OK)
    {
        return result;
    }

    info = e902_clic_get_info();
    if ((info == 0) ||
        (info->hardware_irq_count <= T22_SERDES_IRQ_HIGHEST))
    {
        return E902_CLIC_ERROR_INFO;
    }

    return E902_CLIC_OK;
}
