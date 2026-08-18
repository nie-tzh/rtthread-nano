#include "t22_serdes.h"

#define T22_SERDES_CSRAO_WRITE_CONTROL_OFFSET  0x024U

struct t22_serdes_csrao_registers
{
    uint32_t reserved0[T22_SERDES_CSRAO_WRITE_CONTROL_OFFSET /
                       sizeof(uint32_t)];
    volatile uint32_t write_control;
};

#define T22_SERDES_CSRAO \
    ((struct t22_serdes_csrao_registers *)(uintptr_t)T22_SERDES_CSRAO_BASE)

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
