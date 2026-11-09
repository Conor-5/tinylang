/*
 * port_capture.h - output captured by the unit-test port (port_capture.c).
 */
#ifndef PORT_CAPTURE_H
#define PORT_CAPTURE_H

extern char capture_out[4096];  /* everything sent to lang_port_write */
extern char capture_err[4096];  /* everything sent to lang_port_error */

void capture_reset(void);

#endif /* PORT_CAPTURE_H */
