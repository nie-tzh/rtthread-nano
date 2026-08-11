#ifndef T22_RESET_H
#define T22_RESET_H

#include <stdint.h>

#include "reset.h"

struct t22_reset_soc_data
{
    uintptr_t reset_reg;
    uint32_t nr_resets;
};

struct t22_reset
{
    struct reset_controller_dev rcdev;
    const struct t22_reset_soc_data *soc;
};

void t22_reset_init(struct t22_reset *reset,
                    const struct t22_reset_soc_data *soc);

#endif
