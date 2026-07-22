#ifndef T22_SERDES_IRQ_H
#define T22_SERDES_IRQ_H

#define T22_SERDES_IRQ_MACHINE_SOFTWARE  3
#define T22_SERDES_IRQ_CORE_TIMER        7
#define T22_SERDES_IRQ_DW_TIMER         27
#define T22_SERDES_IRQ_HIGHEST          70
#define T22_SERDES_IRQ_VECTOR_COUNT     80

#if !defined(__ASSEMBLER__)

struct riscv_clic;

int t22_serdes_irq_init(void);
struct riscv_clic *t22_serdes_clic(void);

#endif

#endif
