#include <stddef.h>

#include "t22_serdes.h"
#include "t22_serdes_regs.h"

struct t22_serdes_csrao_registers
{
    uint32_t reserved0[CSRAO_WRITE_CONTROL_OFFSET / sizeof(uint32_t)];
    volatile uint32_t write_control;
};

struct t22_serdes_csr_registers
{
    volatile uint32_t pll;
};

struct t22_serdes_misc_registers
{
    uint32_t reserved0[MISC_PLL_CONFIG19_OFFSET / sizeof(uint32_t)];
    volatile uint32_t pll_config19;
};

_Static_assert(offsetof(struct t22_serdes_csrao_registers,
                        write_control) == CSRAO_WRITE_CONTROL_OFFSET,
               "T22 CSRAO write-control offset mismatch");
_Static_assert(offsetof(struct t22_serdes_misc_registers, pll_config19) ==
               MISC_PLL_CONFIG19_OFFSET,
               "T22 MISC PLL-config19 offset mismatch");

#define T22_SERDES_CSRAO \
    ((struct t22_serdes_csrao_registers *)(uintptr_t)T22_SERDES_CSRAO_BASE)
#define T22_SERDES_CSR \
    ((struct t22_serdes_csr_registers *)(uintptr_t)T22_SERDES_CSR_BASE)
#define T22_SERDES_MISC \
    ((struct t22_serdes_misc_registers *)(uintptr_t)T22_SERDES_MISC_BASE)

static void csr_write32(volatile uint32_t *addr, uint32_t value)
{
    T22_SERDES_CSRAO->write_control = 1U;
    *addr = value;
}

void t22_serdes_csr_update32(volatile uint32_t *addr,
                             uint32_t mask,
                             uint32_t value)
{
    uint32_t reg = *addr;

    reg = (reg & ~mask) | (value & mask);
    csr_write32(addr, reg);
}

static void t22_serdes_clock_init(void)
{
    T22_SERDES_MISC->pll_config19 |= MISC_PLL_SYSTEM_CLOCK2_ENABLE;
    t22_serdes_csr_update32(&T22_SERDES_CSR->pll,
                            CSR_PLL_CLOCK_MASK,
                            CSR_PLL_CLOCK_VALUE);
}

void t22_serdes_system_init(void)
{
    t22_serdes_clock_init();
}
