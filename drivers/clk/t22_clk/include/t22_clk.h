#ifndef T22_CLK_H
#define T22_CLK_H

#include "clk.h"

enum t22_clk_id
{
    T22_CLK_AHB = 0,
    T22_CLK_APB,
    T22_CLK_SPI,
    T22_CLK_COUNT
};

void t22_clk_init(void);
const struct clk *t22_clk_get(enum t22_clk_id id);

#endif
