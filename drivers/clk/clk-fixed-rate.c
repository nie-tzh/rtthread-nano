#include "clk-provider.h"

static unsigned long clk_fixed_rate_recalc_rate(
    const struct clk_hw *hw,
    unsigned long parent_rate)
{
    const struct clk_fixed_rate *fixed =
        (const struct clk_fixed_rate *)hw;

    (void)parent_rate;
    return fixed->fixed_rate;
}

const struct clk_ops clk_fixed_rate_ops =
{
    .recalc_rate = clk_fixed_rate_recalc_rate
};
