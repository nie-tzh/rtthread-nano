#include <errno.h>

#include "reset.h"

int reset_control_reset(const struct reset_control *rstc)
{
    const struct reset_controller_dev *rcdev;

    if ((rstc == NULL) || (rstc->rcdev == NULL))
    {
        return -EINVAL;
    }

    rcdev = rstc->rcdev;
    if (rstc->id >= rcdev->nr_resets)
    {
        return -EINVAL;
    }

    if ((rcdev->ops == NULL) || (rcdev->ops->reset == NULL))
    {
        return -ENOSYS;
    }

    return rcdev->ops->reset(rstc->rcdev, rstc->id);
}
