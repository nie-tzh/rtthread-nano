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
    uint32_t reserved0[8];
    volatile uint32_t soft_reset;
    uint32_t reserved1[43];
    volatile uint32_t mfp_control;
    uint32_t reserved2[1];
    volatile uint32_t mfp_mode1;
};

struct t22_serdes_misc_registers
{
    uint32_t reserved0[DESERIALIZER_MFP14_CONFIG_OFFSET /
                        sizeof(uint32_t)];
    volatile uint32_t mfp14_config;
    uint32_t reserved1[20];
    volatile uint32_t pin_control;
    uint32_t reserved2[60];
    volatile uint32_t pll_config19;
};

_Static_assert(offsetof(struct t22_serdes_csrao_registers,
                        write_control) == CSRAO_WRITE_CONTROL_OFFSET,
               "T22 CSRAO write-control offset mismatch");
_Static_assert(offsetof(struct t22_serdes_csr_registers, soft_reset) ==
               CSR_SOFT_RESET_OFFSET,
               "T22 CSR soft-reset offset mismatch");
_Static_assert(offsetof(struct t22_serdes_csr_registers, mfp_control) ==
               CSR_MFP_CONTROL_OFFSET,
               "T22 CSR MFP-control offset mismatch");
_Static_assert(offsetof(struct t22_serdes_csr_registers, mfp_mode1) ==
               CSR_MFP_MODE1_OFFSET,
               "T22 CSR MFP-mode1 offset mismatch");
_Static_assert(offsetof(struct t22_serdes_misc_registers, mfp14_config) ==
               DESERIALIZER_MFP14_CONFIG_OFFSET,
               "T22 MISC MFP14 offset mismatch");
_Static_assert(offsetof(struct t22_serdes_misc_registers, pin_control) ==
               DESERIALIZER_PIN_CONTROL_OFFSET,
               "T22 MISC pin-control offset mismatch");
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

static void csr_update32(volatile uint32_t *addr, uint32_t mask, 
                         uint32_t value)
{
    uint32_t reg = *addr;

    reg = (reg & ~mask) | (value & mask);
    csr_write32(addr, reg);
}

static void t22_serdes_clock_init(void)
{
    T22_SERDES_MISC->pll_config19 |= MISC_PLL_SYSTEM_CLOCK2_ENABLE;
    csr_update32(&T22_SERDES_CSR->pll,
                 CSR_PLL_CLOCK_MASK,
                 CSR_PLL_CLOCK_VALUE);
}

static void t22_serdes_mfp_control_init(void)
{
    T22_SERDES_MISC->pin_control = DESERIALIZER_PIN_CONTROL_VALUE;
}

void t22_serdes_system_init(void)
{
    t22_serdes_clock_init();
    t22_serdes_mfp_control_init();
}

void t22_serdes_uart2_tx_pin_init(void)
{
    csr_update32(&T22_SERDES_CSR->mfp_mode1,
                 DESERIALIZER_MFP14_MODE_MASK,
                 DESERIALIZER_MFP14_MODE_UART2_TX);

    /* T22 production parts use the eFuse=0 MFP14 ECO polarity. */
    T22_SERDES_MISC->mfp14_config = DESERIALIZER_UART2_TX_CONFIG;
    csr_update32(&T22_SERDES_CSR->mfp_control,
                 CSR_MFP_SOFTWARE_CONTROL,
                 CSR_MFP_SOFTWARE_CONTROL);
}

void t22_serdes_uart2_reset(void)
{
    csr_update32(&T22_SERDES_CSR->soft_reset,
                 CSR_UART2_RESET_N,
                 0U);
    csr_update32(&T22_SERDES_CSR->soft_reset,
                 CSR_UART2_RESET_N,
                 CSR_UART2_RESET_N);
}
