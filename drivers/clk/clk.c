#include <stddef.h>

#include "clk.h"
#include "clk-provider.h"

unsigned long clk_hw_get_rate(const struct clk_hw *hw)
{
    unsigned long parent_rate = 0UL;

    if (hw == NULL)
    {
        return 0UL;
    }

    if (hw->parent != NULL)
    {
        parent_rate = clk_hw_get_rate(hw->parent);
    }

    if ((hw->ops == NULL) || (hw->ops->recalc_rate == NULL))
    {
        return parent_rate;
    }

    return hw->ops->recalc_rate(hw, parent_rate);
}

unsigned long clk_get_rate(const struct clk *clk)
{
    if (clk == NULL)
    {
        return 0UL;
    }

    return clk_hw_get_rate(clk->hw);
}
