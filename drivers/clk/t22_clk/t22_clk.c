#include <stdint.h>

#include "clk-provider.h"
#include "t22_clk.h"
#include "t22_serdes.h"

#define T22_CLK_PLL_AHB_MASK                 (0x3U << 2)
#define T22_CLK_PLL_AHB_320MHZ               (0x3U << 2)
#define T22_CLK_PLL_APB_MASK                 (0x3U << 4)
#define T22_CLK_PLL_APB_200MHZ               (0x2U << 4)
#define T22_CLK_PLL_SPI_MASK                 (0x3U << 6)
#define T22_CLK_PLL_SPI_200MHZ               (0x2U << 6)
#define T22_CLK_PLL_MASK                     (T22_CLK_PLL_AHB_MASK | \
                                              T22_CLK_PLL_APB_MASK | \
                                              T22_CLK_PLL_SPI_MASK)
#define T22_CLK_PLL_VALUE                    (T22_CLK_PLL_AHB_320MHZ | \
                                              T22_CLK_PLL_APB_200MHZ | \
                                              T22_CLK_PLL_SPI_200MHZ)
#define T22_CLK_MISC_PLL_CONFIG19_OFFSET     0x1ACU
#define T22_CLK_MISC_SYSTEM_CLOCK2_ENABLE    (1U << 8)

struct t22_clk_registers
{
    volatile uint32_t pll;
};

struct t22_clk_misc_registers
{
    uint32_t reserved0[T22_CLK_MISC_PLL_CONFIG19_OFFSET /
                       sizeof(uint32_t)];
    volatile uint32_t pll_config19;
};

#define T22_CLK_REGS \
    ((struct t22_clk_registers *)(uintptr_t)T22_SERDES_CSR_BASE)
#define T22_CLK_MISC_REGS \
    ((struct t22_clk_misc_registers *)(uintptr_t)T22_SERDES_MISC_BASE)

#define T22_FIXED_RATE(_rate)                       \
    {                                               \
        .hw =                                       \
        {                                           \
            .ops = &clk_fixed_rate_ops,             \
            .parent = 0                             \
        },                                          \
        .fixed_rate = (_rate)                       \
    }

static const struct clk_fixed_rate t22_clk_hws[T22_CLK_COUNT] =
{
    [T22_CLK_AHB] = T22_FIXED_RATE(T22_SERDES_AHB_CLOCK_HZ),
    [T22_CLK_APB] = T22_FIXED_RATE(T22_SERDES_APB_CLOCK_HZ),
    [T22_CLK_SPI] = T22_FIXED_RATE(T22_SERDES_SPI_CLOCK_HZ)
};

static const struct clk t22_clks[T22_CLK_COUNT] =
{
    [T22_CLK_AHB] = { .hw = &t22_clk_hws[T22_CLK_AHB].hw },
    [T22_CLK_APB] = { .hw = &t22_clk_hws[T22_CLK_APB].hw },
    [T22_CLK_SPI] = { .hw = &t22_clk_hws[T22_CLK_SPI].hw }
};

void t22_clk_init(void)
{
    T22_CLK_MISC_REGS->pll_config19 |=
        T22_CLK_MISC_SYSTEM_CLOCK2_ENABLE;
    t22_serdes_csr_update32(&T22_CLK_REGS->pll,
                            T22_CLK_PLL_MASK,
                            T22_CLK_PLL_VALUE);
}

const struct clk *t22_clk_get(enum t22_clk_id id)
{
    return &t22_clks[id];
}
