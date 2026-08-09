#ifndef T22_SERDES_REGS_H
#define T22_SERDES_REGS_H

#define CSRAO_WRITE_CONTROL_OFFSET       0x024U

#define CSR_PLL_OFFSET                   0x000U
#define CSR_SOFT_RESET_OFFSET            0x024U

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

#define MISC_PLL_CONFIG19_OFFSET         0x1ACU
#define MISC_PLL_SYSTEM_CLOCK2_ENABLE    (1U << 8)

#endif
