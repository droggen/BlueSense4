/*
 * serial_usb.h
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */


#include <stdio.h>
#include "circbuf.h"
#include "serial.h"

#define USB_BUFFERSIZE 2048
//#define USB_BUFFERSIZE 512

extern SERIALPARAM SERIALPARAM_USB;

extern unsigned char USB_RX_DataBuffer[];
extern unsigned char USB_TX_DataBuffer[];


void serial_usb_initbuffers();
unsigned char _serial_usb_trigger_background_tx(unsigned char p);

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

void _serial_usb_enable_write(unsigned char en);

