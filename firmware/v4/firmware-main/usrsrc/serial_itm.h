/*
 * dbg.h
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */

#ifndef SERIAL_ITM_H_
#define SERIAL_ITM_H_

#include <stdio.h>


FILE *serial_open_itm();
ssize_t serial_itm_cookie_write(void *__cookie, const char *__buf, size_t __n);
ssize_t serial_itm_cookie_read(void *__cookie, char *__buf, size_t __n);

void itmprintf( const char* format, ... );

#endif /* SERIAL_ITM_H_ */
