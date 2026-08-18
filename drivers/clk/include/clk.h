#ifndef CLK_H
#define CLK_H

struct clk_hw;

struct clk
{
    const struct clk_hw *hw;
};

unsigned long clk_get_rate(const struct clk *clk);

#endif
