/*
 * serial_usb.h
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */


#include <stdio.h>
#include "circbuf.h"
#include "serial.h"

#define USB_BUFFERSIZE 8192

extern SERIALPARAM SERIALPARAM_USB;



void serial_usb_initbuffers();


FILE *serial_open_usb();
ssize_t serial_usb_cookie_read(void *__cookie, char *__buf, size_t __n);
ssize_t serial_usb_cookie_write(void *__cookie, const char *__buf, size_t __n);


/*
int usb_fputchar(char c, FILE*stream);
int usb_fputchar_nonblock(char c, FILE*stream);
int usb_fgetchar(FILE *stream);
int usb_fgetchar_nonblock(FILE *stream);*/
void serial_usb_clearbuffers(void);
unsigned char serial_usb_putbuf(SERIALPARAM *sp,char *data,unsigned short n);
unsigned char serial_usb_fischar(SERIALPARAM *sp);

