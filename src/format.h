/*
 * format.h - integer to decimal text, without printf (not allowed in src/).
 */
#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>

/* Longest result: "-2147483648" plus NUL. */
#define LANG_FMT_INT_SIZE 12

/* Writes v in decimal to buf (at least LANG_FMT_INT_SIZE bytes).
 * Returns buf. */
char *lang_fmt_int(char *buf, int32_t v);

/* Same for an unsigned value (line numbers). */
char *lang_fmt_uint(char *buf, uint32_t v);

#endif /* FORMAT_H */
