/*
 * format.c - integer to decimal text, without printf (not allowed in src/).
 */
#include "format.h"

char *lang_fmt_uint(char *buf, uint32_t v)
{
    char tmp[LANG_FMT_INT_SIZE];
    int n = 0;
    int i = 0;
    do {
        tmp[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v != 0);
    while (n > 0) {
        buf[i++] = tmp[--n];
    }
    buf[i] = '\0';
    return buf;
}

char *lang_fmt_int(char *buf, int32_t v)
{
    if (v < 0) {
        /* Negate in unsigned arithmetic: -INT32_MIN does not fit in an
         * int32_t, but 0u - (uint32_t)v is always defined. */
        buf[0] = '-';
        lang_fmt_uint(buf + 1, 0u - (uint32_t)v);
    } else {
        lang_fmt_uint(buf, (uint32_t)v);
    }
    return buf;
}
