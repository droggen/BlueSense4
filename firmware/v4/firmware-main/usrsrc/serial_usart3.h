/*
 * serial_usart3.h
 *
 *  Created on: 28 juin 2021
 *      Author: droggen
 */

#ifndef SERIAL_USART3_H_
#define SERIAL_USART3_H_

#include <stdio.h>
#include "serial.h"
// Minimalistic set of functions
extern SERIALPARAM _serial_usart3_param;
extern FILE *_serial_uasrt3_file;

FILE *serial_usart3_init(unsigned baudrate,unsigned stopbit);
void serial_usart3_deinit();
void serial_usart3_send(unsigned char ch);

unsigned char serial_usart3_putbuf(SERIALPARAM *sp,char *data,unsigned short n);
ssize_t serial_usart3_cookiewrite_int(void *__cookie, const char *__buf, size_t __nbytes);
void serial_usart3_enablecksum(unsigned en);

#endif /* SERIAL_USART3_H_ */
