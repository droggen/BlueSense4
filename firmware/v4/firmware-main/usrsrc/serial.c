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
	Serial functions
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "serial.h"
#include "main.h"
//#include "serial_uart.h"
//#include "serial_usb.h"
#include "usrmain.h"
#include "global.h"
//#include "serial_dbg.h"

unsigned char serial_numopen=0;				// Keep track of open buffers

//FILE *file_bt,*file_usb,*file_fb,*file_dbg,*file_pri,*file_itm;
FILE *file_bt,*file_usb,*file_fb,*file_pri,*file_itm;

//FILEANDSERIALPARAMMAP FileAndSerialParamMap[SERIAL_MAXSERIAL];


// 0: success. 1: failure
/*unsigned char serial_associate(FILE *f,SERIALPARAM *sp)
{
	for(int i=0;i<SERIAL_MAXSERIAL;i++)
	{
		if(FileAndSerialParamMap[i].used)
			continue;
		// Associate
		FileAndSerialParamMap[i].used=1;
		FileAndSerialParamMap[i].f = f;
		FileAndSerialParamMap[i].sp = sp;
		return 0;
	}
	return 1;
}
SERIALPARAM *serial_findserialparamfromfile(FILE *f)
{
	// This is slow, but is only used for fputbuf which writes chucks to data, hence overhead is small.
	for(int i=0;i<SERIAL_MAXSERIAL;i++)
	{
		if(!FileAndSerialParamMap[i].used)
			return 0;
		if(FileAndSerialParamMap[i].f == f)
			return FileAndSerialParamMap[i].sp;
	}
	return 0;
}
*/
/******************************************************************************
	serial_open
*******************************************************************************
	Opens a serial communication peripheral.

	periph:				SERIAL_UART0:		UART0
						SERIAL_UART1:		UART1
						SERIAL_USB:			USB interface
						SERIAL_DBG:			ITM interface
	interrupt:
						1:		indicates that an interrupt-driven function is desired (if available)
						0:		indicates that a non-interrupt-driven function is desired (if available)

	Return value:
		0:				success
		nonzero:	error
******************************************************************************/
/*
FILE *serial_open(unsigned char periph,unsigned char interrupt)
{

	FILE *f=0;
	SERIALPARAM *p=0;

	p = (SERIALPARAM*)malloc(sizeof(SERIALPARAM));

	p->blocking=1;
	switch(periph)
	{
		#if ENABLE_SERIAL0==1
		case SERIAL_UART0:
			// UART0
			if(interrupt)
				//f = fdevopen(uart0_fputchar_int,uart0_fgetchar_int);
				fdev_setup_stream(f,uart0_fputchar_int, uart0_fgetchar_int,_FDEV_SETUP_RW);
			else
				//f = fdevopen(uart0_fputchar,uart0_fgetchar);
				fdev_setup_stream(f,uart0_fputchar, uart0_fgetchar,_FDEV_SETUP_RW);
			p->putbuf = uart0_fputbuf_int;
			p->txbuf = uart0_get_txbuf();
			p->rxbuf = uart0_get_rxbuf();
			break;
		#endif
		#if ENABLE_SERIAL1==1
		case SERIAL_UART1:
			// UART1
			#if BOOTLOADER==1
			//f = fdevopen(uart1_fputchar_int,uart1_fgetchar_int);
			fdev_setup_stream(f,uart1_fputchar_int, uart1_fgetchar_int,_FDEV_SETUP_RW);
			#else
			if(interrupt)
				//f = fdevopen(uart1_fputchar_int,uart1_fgetchar_int);
				fdev_setup_stream(f,uart1_fputchar_int, uart1_fgetchar_int,_FDEV_SETUP_RW);
			else
				//f = fdevopen(uart1_fputchar,uart1_fgetchar);
				fdev_setup_stream(f,uart1_fputchar, uart1_fgetchar,_FDEV_SETUP_RW);
			p->putbuf = uart1_fputbuf_int;
			#endif
			p->txbuf = uart1_get_txbuf();
			p->rxbuf = uart1_get_rxbuf();
			break;
		#endif
		case SERIAL_USB:
		{
			// USB
			cookie_io_functions_t iof;
			iof.read = &serial_usb_read;
			iof.write = &serial_usb_write;
			iof.close = 0;
			iof.seek = 0;
			f = fopencookie((void*)&SERIALPARAM_USB,"w+",iof);
			setvbuf (f, 0, _IONBF, 0 );	// No buffering
			//setvbuf (f, 0, _IOLBF, 1024);	// Line buffer buffering
			break;
		}
		case SERIAL_ITM:
		default:
		{
			// ITM interface
			cookie_io_functions_t iof;
			iof.read = &serial_dbg_read;
			iof.write = &serial_dbg_write;
			iof.close = 0;
			iof.seek = 0;
			f = fopencookie((void*)0,"w+",iof);
			//setvbuf (f, 0, _IONBF, 0 );	// No buffering
			break;
		}
	}

	//fdev_set_udata(f,(void*)p);
	//if(serial_numopen==0)
	//{
		// Assign standard io
		//stdin = f;
		//stdout = f;
		//stderr = f;
	//}
	//serial_numopen++;

	return f;
}*/


/*
	Sets whether the read functions must be blocking or non blocking.
	In non-blocking mode, when no characters are available, read functions return EOF.
*/
/*void serial_setblocking(FILE *file,unsigned char blocking)
{
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(file);
	p->blocking = blocking;
}
unsigned char serial_isblocking(FILE *file)
{
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(file);
	return p->blocking;
}
*/

unsigned char fputbuf(FILE *stream,char *data,unsigned short n)
{
	SERIALPARAM *p = (SERIALPARAM*)stream->_cookie;

	//fprintf(file_pri,"in fputbuf. Pointer to serialparam: %p. pointer to func: %p. (%p %p)\n",p,p->putbuf,&SERIALPARAM_USB,&serial_usb_putbuf);

	//fprintf(file_dbg,"fputbuf FILE %p cookie %p putbuf %p\n",stream,p,p->putbuf);

	if(p->putbuf)						// Check if putbuf is available
		return p->putbuf(p,data,n);		// Call putbuf
	return 0;
}
/******************************************************************************
	function: fischar
*******************************************************************************
	Checks whether a character is in the receive buffer.

	This function is faster than using fgetc and checking its return value.
	(fgetc: ~30uS/call, fischar: 3uS/call)

	This only checks for data in the "serial" buffers; data may have been
	read by stdio librairies (fgetc, fread) in their own buffer.

	Therefore, this function should typically be called when the stdio buffers
	are empty and waiting for new data.



	Parameters:
		stream			-			FILE pointer

	Returns:
		0				-			No data available
		1				-			Data available

******************************************************************************/
unsigned char fischar(FILE *stream)
{
	SERIALPARAM *p = (SERIALPARAM*)stream->_cookie;


	if(p->fischar)						// Check if fischar is available
		return p->fischar(p);			// Call fischar
	return 0;
}

void fbufferwhendisconnected(FILE *stream,unsigned char bufferwhendisconnected)
{
	SERIALPARAM *p = (SERIALPARAM*)stream->_cookie;
	p->bufferwhendisconnected = bufferwhendisconnected;
}


/*unsigned char fputbuf(FILE *stream,char *data,unsigned char n)
{
	if(!stream)
		return 0;
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(stream);
	return p->putbuf(data,n);
}*/

/*
unsigned short fgettxbuflevel(FILE *stream)
{
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(stream);
	return buffer_level(p->txbuf);
}
unsigned short fgetrxbuflevel(FILE *stream)
{
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(stream);
	return buffer_level(p->rxbuf);
}
unsigned short fgettxbuffree(FILE *stream)
{
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(stream);
	return buffer_freespace(p->txbuf);
}
unsigned short fgetrxbuffree(FILE *stream)
{
	SERIALPARAM *p = (SERIALPARAM*)fdev_get_udata(stream);
	return buffer_freespace(p->rxbuf);
}
*/




/*
	Sets whether the read functions must be blocking or non blocking.
	In non-blocking mode, when no characters are available, read functions return EOF.
*/
/*void serial_setblocking(FILE *file,unsigned char blocking)
{
	SERIALPARAM *p = (SERIALPARAM*)file->_cookie;
	p->blocking = blocking;
}
unsigned char serial_isblocking(FILE *file)
{
	SERIALPARAM *p = (SERIALPARAM*)file->_cookie;
	return p->blocking;
}*/
void serial_setblockingwrite(FILE *file,unsigned char blocking)
{
	SERIALPARAM *p = (SERIALPARAM*)file->_cookie;
	p->blockingwrite = blocking;
}

/*char *fgets_timeout( char *str, int num, FILE *stream, int timeout)
{
	// Emulates fgets but waits timeout miliseconds for a carriage return
	uint32_t t1 = HAL_GetTick(),t2;
	char *rv;
	char *tmpstr = str;
	int tmpnum = num;

	for(int i=0;i<num;i++)
		str[i] = 0;

	int it=0;
	while(1)
	{
		itmprintf("it %d: at %p for %d\n",it,tmpstr,tmpnum);
		rv = fgets(tmpstr,tmpnum,stream);
		itmprintf("it %d: rv: %p. l: %d\n",it,rv,strlen(tmpstr));

		itmprintf("it %d: ",it);
		for(int i=0;i<num;i++)
		{
			itmprintf("%02X ",str[i]);
		}
		itmprintf("\n");

		it++;

		//if(strlen(tmpstr>2))
		//{
		//	itmprintf("\t%d %d\n",tmpstr[strlen(tmpstr)-2],tmpstr[strlen(tmpstr)-1]);
		//}

		if(strlen(tmpstr))
		{
			// Received some characters - update tmpstr and tmpnum
			int n = strlen(tmpstr);
			tmpstr = tmpstr+n;
			tmpnum-=n;
			if(tmpnum==0)
				break;

			// Check if last character is newline
			if(tmpstr[-1]=='\n')
				break;
		}
		// Check if timeout
		if( (HAL_GetTick()-t1) > timeout)
			break;

		HAL_Delay(5);
	}

	// Check what to return
	if(strlen(tmpstr)==0)
		return 0;


	return tmpstr;
}*/

/*

 */
int fgetc_timeout(FILE *stream, int timeout)
{
	uint32_t t1 = HAL_GetTick();
	while((HAL_GetTick()-t1) < timeout)
	{
		int c = fgetc(stream);
		if(c!=-1)
			return c;
	}

	// Return failure
	return -1;
}

char *fgets_timeout( char *str, int num, FILE *stream, int timeout)
{
	// Emulates fgets but waits timeout miliseconds for a carriage return
	uint32_t t1 = HAL_GetTick();
	int nr=0;

	for(int i=0;i<num;i++)
		str[i] = 0;

	while(1)
	{
		int c = fgetc(stream);

		//itmprintf("%04X\n",c);

		if(c!=-1)
		{
			str[nr] = c;
			nr++;
			// Return in case of size
			if(nr>=num-1)
			{
				// Terminate with null and return
				str[num-1] = 0;
				return str;
			}
			// Return in case of newline
			if(c=='\n')
				return str;
		}

		// Check if timeout
		if( (HAL_GetTick()-t1) > timeout)
			return str;

		//HAL_Delay(10);
	}

	return 0;
}

char *fgets_timeout_1s( char *str, int num, FILE *stream)
{
	return fgets_timeout(str,num,stream,1000);
}
char *fgets_timeout_100ms( char *str, int num, FILE *stream)
{
	return fgets_timeout(str,num,stream,100);
}

