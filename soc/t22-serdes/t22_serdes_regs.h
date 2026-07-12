#ifndef T22_SERDES_REGS_H
#define T22_SERDES_REGS_H

#define CSRAO_WRITE_CONTROL_OFFSET       0x024U

#define CSR_PLL_OFFSET                   0x000U
#define CSR_SOFT_RESET_OFFSET            0x024U
#define CSR_MFP_CONTROL_OFFSET           0x0D4U
#define CSR_MFP_MODE1_OFFSET             0x0DCU

#define CSR_PLL_AHB_MASK                 (0x3U << 2)
#define CSR_PLL_AHB_320MHZ               (0x3U << 2)
#define CSR_PLL_APB_MASK                 (0x3U << 4)
#define CSR_PLL_APB_200MHZ               (0x2U << 4)
#define CSR_PLL_SPI_MASK                 (0x3U << 6)
#define CSR_PLL_SPI_200MHZ               (0x2U << 6)
#define CSR_PLL_CLOCK_MASK               (CSR_PLL_AHB_MASK | \
                                          CSR_PLL_APB_MASK | \
                                          CSR_PLL_SPI_MASK)
#define CSR_PLL_CLOCK_VALUE              (CSR_PLL_AHB_320MHZ | \
                                          CSR_PLL_APB_200MHZ | \
                                          CSR_PLL_SPI_200MHZ)

#define CSR_UART2_RESET_N                (1U << 10)
#define CSR_MFP_SOFTWARE_CONTROL         (1U << 0)
#define DESERIALIZER_MFP14_MODE_MASK     (0xFU << 4)
#define DESERIALIZER_MFP14_MODE_UART2_TX (0x2U << 4)

#define EFUSE_SOFTWARE_WORD1_OFFSET      0x034U
#define EFUSE_ECO_UPDATED                (1U << 3)

#define DESERIALIZER_PIN_CONTROL_OFFSET  0x0B8U
#define DESERIALIZER_PIN_CONTROL_VALUE   0x0001F9FFU
#define DESERIALIZER_MFP14_CONFIG_OFFSET 0x064U
#define MISC_PLL_CONFIG19_OFFSET         0x1ACU
#define MISC_PLL_SYSTEM_CLOCK2_ENABLE    (1U << 8)

#define DESERIALIZER_UART2_TX_CONFIG     0x000048B4U
#define DESERIALIZER_MFP14_ECO_MASK      0x00000010U

#endif
