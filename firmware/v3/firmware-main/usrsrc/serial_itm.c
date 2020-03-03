/*
* dbg.c
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */

#include "serial_itm.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "main.h"
#include "usrmain.h"


FILE *serial_open_itm()
{
	FILE *f=0;

	// Create cookie
	cookie_io_functions_t iof;

	iof.read = &serial_itm_cookie_read;
	iof.write = &serial_itm_cookie_write;
	iof.close = 0;
	iof.seek = 0;
	f = fopencookie((void*)0,"w+",iof);

	itmprintf("fopencookie: %p\n",f);

	setvbuf (f, 0, _IONBF, 0 );	// No buffering
	//setvbuf (f, 0, _IOLBF, 1024);	// Line buffer buffering



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
}


/*
 * GCC: enable a default printf via the ITM interface
 */

/* newlib?
 *
 */

//volatile int32_t ITM_RxBuffer=ITM_RXBUFFER_EMPTY;


/*
 * Once defined printf will call this function
 */
/*int _write (int fd, const void *buf, size_t count)
{
	for(size_t i=0;i<count;i++)
		ITM_SendChar(((char*)buf)[i]);
	return count;
}*/

/*
 * Serial read not supported over SWI
 */
ssize_t serial_itm_cookie_read(void *__cookie, char *__buf, size_t __n)
{
	// If no data, should return error (-1) instead of eof (0).
	// Returning eof has side effects on subsequent calls to fgetc: it returns eof forever, until some write is done.
	return -1;
}

/*
 * SWI write with ITM_SendChar are blocking if a debugging device is connected
 */
ssize_t serial_itm_cookie_write(void *__cookie, const char *__buf, size_t __n)
{
	for(size_t i=0;i<__n;i++)
	{
		ITM_SendChar(__buf[i]);
	}

	return __n;
}



/*int dbg_fputchar(char c, FILE*stream)
{
	// If not connected: don't fill buffer
	//if(!system_isusbconnected())
//		return 0;

	// Wait until send buffer is free.
	//while( buffer_isfull(&_dbg_tx_state) );
	// Store the character in the buffer
	//buffer_put(&_dbg_tx_state,c);

	// CDC functions to write one character


	return 0;
}

int dbg_fgetchar(FILE *stream)
{
	//int c;
	//do{ c=dbg_fgetchar_nonblock(stream);}
	//while(c==EOF && serial_isblocking(stream));

	return 0;
	//return c;
}
*/




/*
 * Print to ITM
 */
void itmprintf( const char* format, ... )
{
	char string[1024];
	va_list args;
	va_start( args, format );
	vsprintf( string, format, args );

	va_end( args );


	char *ptr = string;
	while(*ptr)
	{
		ITM_SendChar(*ptr);
		ptr++;
	}
}
