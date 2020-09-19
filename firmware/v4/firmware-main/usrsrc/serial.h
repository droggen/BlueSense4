/*
   Copyright (C) 2019:
         Daniel Roggen, droggen@gmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/*
	Generic serial functions - to phase out
*/
#ifndef __SERIAL_H
#define __SERIAL_H


#include "main.h"
//#include <serial_dbg.h>
//#include <stdio.h>
#include "circbuf.h"
//#include "serial0.h"
//#include "serial1.h"


//#define SERIAL_UART0 0
//#define SERIAL_UART1 1
//#define SERIAL_USB 10
//#define SERIAL_ITM 11


// Common for uart, dbg(i2c) and others

#define SERIAL_MAXSERIAL 10

typedef struct _SERIALPARAM
{
	unsigned char blocking,blockingwrite;
	unsigned char bufferwhendisconnected;
	unsigned char dma1_or_int0;
	unsigned char dma1_or_int0_tx;
	CIRCULARBUFFER txbuf;
	CIRCULARBUFFER rxbuf;

	unsigned char (*putbuf)(struct _SERIALPARAM *sp, char *data,unsigned short n);
	unsigned char (*fischar)(struct _SERIALPARAM *sp);

	// Device is memory address of peripheral or some other relevant info
	void *device;
} SERIALPARAM;


typedef struct
{
	FILE *f;
	SERIALPARAM *sp;
	unsigned char used;
} FILEANDSERIALPARAMMAP;

extern FILEANDSERIALPARAMMAP FileAndSerialParamMap[];


unsigned char serial_associate(FILE *f,SERIALPARAM *sp);
SERIALPARAM *serial_findserialparamfromfile(FILE *f);

FILE *serial_open_uart(USART_TypeDef *periph,int *__p);

//FILE *serial_open(unsigned char periph,unsigned char interrupt);

//extern FILE *file_bt,*file_usb,*file_fb,*file_dbg,*file_pri,*file_itm;
//extern FILE *file_bt,*file_usb,*file_fb,*file_pri,*file_itm;

/******************************************************************************
*******************************************************************************
UART MANAGEMENT   UART MANAGEMENT   UART MANAGEMENT   UART MANAGEMENT
*******************************************************************************
******************************************************************************/

//void uart_getbuffersize(unsigned short *rx0,unsigned short *tx0,unsigned short *rx1,unsigned short *tx1);

//void serial_setblocking(FILE *file,unsigned char blocking);
//unsigned char serial_isblocking(FILE *file);

int fgetc_timeout(FILE *stream, int timeout);
char *fgets_timeout( char *str, int num, FILE *stream, int timeout);
char *fgets_timeout_1s( char *str, int num, FILE *stream);
char *fgets_timeout_100ms( char *str, int num, FILE *stream);

unsigned char fputbuf(FILE *stream,char *data,unsigned short n);
unsigned char fischar(FILE *stream);
void fbufferwhendisconnected(FILE *stream,unsigned char bufferwhendisconnected);
void serial_setblockingwrite(FILE *file,unsigned char blocking);
/*unsigned short fgettxbuflevel(FILE *stream);
unsigned short fgetrxbuflevel(FILE *stream);
unsigned short fgettxbuffree(FILE *stream);
unsigned short fgetrxbuffree(FILE *stream);*/

//void flush(FILE *f);



#endif
