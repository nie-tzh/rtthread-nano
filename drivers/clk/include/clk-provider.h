#ifndef CLK_PROVIDER_H
#define CLK_PROVIDER_H

struct clk_hw;

struct clk_ops
{
    unsigned long (*recalc_rate)(const struct clk_hw *hw,
                                 unsigned long parent_rate);
};

struct clk_hw
{
    const struct clk_ops *ops;
    const struct clk_hw *parent;
};

struct clk_fixed_rate
{
    struct clk_hw hw;
    unsigned long fixed_rate;
};

unsigned long clk_hw_get_rate(const struct clk_hw *hw);

extern const struct clk_ops clk_fixed_rate_ops;

#endif
