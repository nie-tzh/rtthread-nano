#ifndef T22_DESERIALIZER_EVB_BOARD_H
#define T22_DESERIALIZER_EVB_BOARD_H

void board_init(void);
int board_early_putc(char ch);
int board_early_puts(const char *text);

#endif
