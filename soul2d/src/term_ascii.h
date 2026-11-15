#ifndef SOUL2D_TERM_ASCII_H
#define SOUL2D_TERM_ASCII_H

#include "platform.h"

#define TERM_W 80
#define TERM_H 24

void term_clear(uint8_t fg, uint8_t bg);
void term_put_cell(int x, int y, char ch, uint8_t fg, uint8_t bg);
void term_put_str(int x, int y, const char *s, uint8_t fg, uint8_t bg);
void term_present(void);

#endif /* SOUL2D_TERM_ASCII_H */
