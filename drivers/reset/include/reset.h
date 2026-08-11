#ifndef RESET_H
#define RESET_H

#include <stdint.h>

struct reset_controller_dev;

struct reset_control_ops
{
    int (*reset)(struct reset_controller_dev *rcdev,
                 uint32_t id);
};

struct reset_controller_dev
{
    const struct reset_control_ops *ops;
    uint32_t nr_resets;
    void *driver_data;
};

struct reset_control
{
    struct reset_controller_dev *rcdev;
    uint32_t id;
};

int reset_control_reset(const struct reset_control *rstc);

#endif
