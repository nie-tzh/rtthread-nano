#include "t22_serdes.h"
#include "t22_serdes_regs.h"

static uint32_t mmio_read32(uintptr_t address)
{
    return *(volatile uint32_t *)address;
}

static void mmio_write32(uintptr_t address, uint32_t value)
{
    *(volatile uint32_t *)address = value;
}

static void mmio_update32(uintptr_t address, uint32_t mask, uint32_t value)
{
    uint32_t reg = mmio_read32(address);

    reg = (reg & ~mask) | (value & mask);
    mmio_write32(address, reg);
}

static void csr_write32(uintptr_t address, uint32_t value)
{
    mmio_write32(T22_SERDES_CSRAO_BASE + CSRAO_WRITE_CONTROL_OFFSET, 1U);
    mmio_write32(address, value);
}

static void csr_update32(uintptr_t address, uint32_t mask, uint32_t value)
{
    uint32_t reg = mmio_read32(address);

    reg = (reg & ~mask) | (value & mask);
    csr_write32(address, reg);
}

static void t22_serdes_clock_init(void)
{
    mmio_update32(T22_SERDES_MISC_BASE + MISC_PLL_CONFIG19_OFFSET,
                  MISC_PLL_SYSTEM_CLOCK2_ENABLE,
                  MISC_PLL_SYSTEM_CLOCK2_ENABLE);
    csr_update32(T22_SERDES_CSR_BASE + CSR_PLL_OFFSET,
                 CSR_PLL_CLOCK_MASK,
                 CSR_PLL_CLOCK_VALUE);
}

void t22_serdes_soc_early_init(void)
{
    t22_serdes_clock_init();
}

void t22_serdes_early_uart2_tx_pin_init(void)
{
    uint32_t efuse_word1 =
        mmio_read32(T22_SERDES_EFUSE_BASE + EFUSE_SOFTWARE_WORD1_OFFSET);
    uint32_t uart2_tx_config = DESERIALIZER_UART2_TX_CONFIG;

    mmio_write32(T22_SERDES_MISC_BASE + DESERIALIZER_PIN_CONTROL_OFFSET,
                 DESERIALIZER_PIN_CONTROL_VALUE);
    csr_update32(T22_SERDES_CSR_BASE + CSR_MFP_MODE1_OFFSET,
                 DESERIALIZER_MFP14_MODE_MASK,
                 DESERIALIZER_MFP14_MODE_UART2_TX);

    /* Older silicon requires the MFP14 TX-enable ECO inversion. */
    if ((efuse_word1 & EFUSE_ECO_UPDATED) == 0U)
    {
        uart2_tx_config ^= DESERIALIZER_MFP14_ECO_MASK;
    }

    mmio_write32(T22_SERDES_MISC_BASE + DESERIALIZER_MFP14_CONFIG_OFFSET,
                 uart2_tx_config);
    csr_update32(T22_SERDES_CSR_BASE + CSR_MFP_CONTROL_OFFSET,
                 CSR_MFP_SOFTWARE_CONTROL,
                 CSR_MFP_SOFTWARE_CONTROL);
}

void t22_serdes_early_uart2_reset(void)
{
    csr_update32(T22_SERDES_CSR_BASE + CSR_SOFT_RESET_OFFSET,
                 CSR_UART2_RESET_N,
                 0U);
    csr_update32(T22_SERDES_CSR_BASE + CSR_SOFT_RESET_OFFSET,
                 CSR_UART2_RESET_N,
                 CSR_UART2_RESET_N);
}
