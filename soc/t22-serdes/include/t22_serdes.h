#ifndef T22_SERDES_H
#define T22_SERDES_H

#include <stdint.h>

#define T22_SERDES_CSRAO_BASE         0x00100C00UL
#define T22_SERDES_UART2_BASE         0x00102000UL
#define T22_SERDES_CSR_BASE           0x00103000UL
#define T22_SERDES_EFUSE_BASE         0x00103C00UL
#define T22_SERDES_MISC_BASE          0x0025C000UL

#define T22_SERDES_AHB_CLOCK_HZ       320000000U
#define T22_SERDES_APB_CLOCK_HZ       200000000U
#define T22_SERDES_SPI_CLOCK_HZ       200000000U

void t22_serdes_soc_early_init(void);
void t22_serdes_early_uart2_tx_pin_init(void);
void t22_serdes_early_uart2_reset(void);

#endif
