/*
 * lang_port.h - functions the host must provide to the language core.
 *
 * The core never prints directly. Each host (Linux, ESP32, unit tests)
 * implements these to send text wherever it belongs.
 */
#ifndef LANG_PORT_H
#define LANG_PORT_H

/* Program output (from print). The host prints s exactly as given. */
void lang_port_write(const char *s);

/* Error text (runtime errors). The host prints s exactly as given. */
void lang_port_error(const char *s);

#endif /* LANG_PORT_H */
