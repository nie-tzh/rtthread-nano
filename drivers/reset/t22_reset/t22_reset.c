#include <stdint.h>

#include "t22_reset.h"
#include "t22_serdes.h"

static int t22_reset_reset(struct reset_controller_dev *rcdev,
                           uint32_t id)
{
    struct t22_reset *reset = rcdev->driver_data;
    const struct t22_reset_soc_data *soc = reset->soc;
    volatile uint32_t *reset_reg =
        (volatile uint32_t *)(uintptr_t)soc->reset_reg;
    uint32_t mask = 1U << id;

    t22_serdes_csr_update32(reset_reg, mask, 0U);
    t22_serdes_csr_update32(reset_reg, mask, mask);
    return 0;
}

static const struct reset_control_ops t22_reset_ops =
{
    .reset = t22_reset_reset
};

void t22_reset_init(struct t22_reset *reset,
                    const struct t22_reset_soc_data *soc)
{
    reset->soc = soc;
    reset->rcdev.ops = &t22_reset_ops;
    reset->rcdev.nr_resets = soc->nr_resets;
    reset->rcdev.driver_data = reset;
}
