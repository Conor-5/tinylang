/*
 * port_capture.c - lang_port.h for unit tests: collects all output in
 * memory so tests can check exactly what the core printed.
 */
#include <string.h>

#include "lang_port.h"
#include "port_capture.h"

char capture_out[4096];
char capture_err[4096];

static void append(char *buf, const char *s)
{
    size_t used = strlen(buf);
    size_t add = strlen(s);
    if (used + add >= sizeof capture_out) {
        add = sizeof capture_out - 1 - used;
    }
    memcpy(buf + used, s, add);
    buf[used + add] = '\0';
}

void capture_reset(void)
{
    capture_out[0] = '\0';
    capture_err[0] = '\0';
}

void lang_port_write(const char *s) { append(capture_out, s); }
void lang_port_error(const char *s) { append(capture_err, s); }
