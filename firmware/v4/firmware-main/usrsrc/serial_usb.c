/*
 * serial_usb.c
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */
#include <serial_usb.h>
#include <stdio.h>
#include "usbd_cdc_if.h"
//#include "usb_device.h"

//#include "usbd_def.h"
#include "atomicop.h"
#include "system.h"
#include "wait.h"
#include "global.h"


/******************************************************************************
	file: serial_usb
*******************************************************************************


	*Implementation notes*


	There are two possibilities to send data from the circular bfufer to usb:
	1 Call at regular interval the CDC_TryTransmit_FS
	2 Call CDC_TryTransmit_FS whenever a cookie_write or putbuf is called; and also on a Transfer complete (to handle additional data put in circular buffer in the meantime).
	By default this uses implementations 2: part (2) allows higher throughput.

	The transmit implementation is non-blocking by default. This means that if the TX circular buffer is full then data is lost.
	This is required so that text printed but not read from the host (e.g. when USB disconnected and buffering when disconnected) does not block
	the operation of the node.
	This requires a large enough transmit buffer (USB_BUFFERSIZE) to hold all the data which may be rapidly printed (e.g. the help screen).


	A blocking write option is implemented in cookie_write for benchmarking purposes. If stdio functions are used in interrupts and the writes are blocking
	then the firmware hangs. Avoid blocking write unless used for benchmarking.



	Bug in the ST LL driver: modify stm32l4xx_ll_usb.c with the line
	else if ((hclk >= 27700000U) && (hclk <= 32000000U)) // Dan: fix bug in ST driver
	to avoid USB hanging when the HCLK is exactly 32MHz. This is an occasional hanging
	with high transfer rate. See:
	https://community.st.com/s/question/0D50X00009XkaCXSAZ/otg-documentation-cube-otghsgusbcfgtrdt-values






******************************************************************************/

/*
Modifications required to the ST USB CDC stack:
- Modify USBD_CDC_DataIn in usbd_cdc.c to call usb_cdc_txready_callback
- Function CDC_TryTransmit_FS to transmit via circular buffer
- usbd_cdc_if.c: Modify CDC_Receive_FS to get data in circular buffer


*/

SERIALPARAM SERIALPARAM_USB;

unsigned char USB_RX_DataBuffer[USB_BUFFERSIZE];
unsigned char USB_TX_DataBuffer[USB_BUFFERSIZE];

uint8_t CDC_TryTransmit_FS();

unsigned char _serial_usb_write_enabled=1;			// Debug parameter to disable data write in cookie_write

extern USBD_HandleTypeDef hUsbDeviceFS;
#if 0
void trytxbletooth()
{
	// Not busy - move as much data from circualr buffer to local buffer
	int nwrite = buffer_level(&SERIALPARAM_USB.txbuf);


	for(int i=0;i<nwrite;i++)
	{
		ITM_SendChar(buffer_get(&SERIALPARAM_USB.txbuf));
	}

}
#endif

#if 0
/*
	The rationale for _serial_usb_trigger_background_tx is to trigger background fflush to empty the stdio buffer.
	However, fflush does not appear thread-safe with the other stdio functions. Therefore, this should not be used.
*/
unsigned char _serial_usb_trigger_background_tx(unsigned char p)
{
	(void)p;

	struct _reent r;

	//fflush(file_usb);
	//fflush_unlocked(file_usb);
	//_fflush_r(&r,file_usb);
	//if(buffer_level(&SERIALPARAM_USB.txbuf))
		//CDC_TryTransmit_FS();

	return 0;
}
#endif

char stdiobuf[64];

FILE *serial_open_usb()
{

	serial_usb_initbuffers();

	// USB
	cookie_io_functions_t iof;
	iof.read = &serial_usb_cookie_read;
	iof.write = &serial_usb_cookie_write;
	iof.close = 0;
	iof.seek = 0;
	FILE *f = fopencookie((void*)&SERIALPARAM_USB,"w+",iof);
	// Line buffering speeds up writes by calling cookie_write with up to a buffer-length large payload.
	// However, it
	setvbuf (f, 0, _IONBF, 0 );	// No buffering
	//setvbuf (f, 0, _IOLBF, 64);	// Line buffer buffering

	//serial_associate(f,&SERIALPARAM_USB);			// Use big hack with f->_cookie below

	// Or: big hack - _cookie seems unused in this libc - this is not portable.
	f->_cookie = &SERIALPARAM_USB;

	/*fprintf(f,"cookie: %p\n",&SERIALPARAM_USB);
	fprintf(f,"file: %d\n",f->_file);
	fprintf(f,"cookie_io_function: %p %p %p\n",&iof,iof.read,iof.write);
	fprintf(f,"cookie in file: %p (offset: %d)\n",f->_cookie,(void*)&(f->_cookie)-(void*)f);
	fprintf(f,"_read in file: %p (%p)\n",*f->_read,&serial_usb_cookie_read);
	fprintf(f,"_write in file: %p (%p)\n",f->_write,&serial_usb_cookie_write);
	fprintf(f,"_close in file: %p (%p)\n",f->_close,0);
	fprintf(f,"_seek in file: %p (%p)\n",f->_seek,0);
	fprintf(f,"sizeof file: %d\n",sizeof(FILE));
	char *ptr = (char*)f;
	for(int i=0;i<sizeof(FILE);i++)
	{
		fprintf(f,"%02X ",ptr[i]);
	}
	fprintf(f,"\n");

	fprintf(f,"Find from file %p, serialparam: %p\n",f,serial_findserialparamfromfile(f));*/

	// [Register USB write callback at low rate to flush stdio and write data]: Do not use - not thread safe
	//timer_register_callback(_serial_usb_trigger_background_tx,9);		// 1000Hz/10 = 100Hz
	//timer_register_callback(_serial_usb_trigger_background_tx,199);		// 1000Hz/200 = 5Hz (200ms latency)
	//timer_register_callback(serial_usb_txcallback,3999);		// 1000Hz/2000 = 0.5Hz


	return f;
}
void serial_usb_initbuffers()
{
	// Initialise
	SERIALPARAM_USB.blocking = 0;
	SERIALPARAM_USB.blockingwrite = 0;
	SERIALPARAM_USB.bufferwhendisconnected = 0;
	SERIALPARAM_USB.putbuf = serial_usb_putbuf;
	SERIALPARAM_USB.fischar = serial_usb_fischar;

	// Initialise the circular buffers with the data buffers
	buffer_init(&SERIALPARAM_USB.rxbuf,USB_RX_DataBuffer,USB_BUFFERSIZE);
	buffer_init(&SERIALPARAM_USB.txbuf,USB_TX_DataBuffer,USB_BUFFERSIZE);
}


/*
 *	The read function should return the number of bytes copied into buf, 0 on end of file, or -1 on error.
 *	Following experiment, if no bytes are available, this function should return -1 to prevent flagging as EOF.
 */
ssize_t serial_usb_cookie_read(void *__cookie, char *__buf, size_t __n)
{
	(void) __cookie;
	// Return the minimum betwen __n and the available data in the receive buffer
	//printf("usbrd: %p %p %d\n",__cookie,__buf,__n);
	int nread=buffer_level(&SERIALPARAM_USB.rxbuf);
	if(__n < nread)
		nread = __n;

	for(int i=0;i<nread;i++)
	{
		__buf[i] = buffer_get(&SERIALPARAM_USB.rxbuf);
	}

	// If no data, should return error (-1) instead of eof (0).
	// Returning eof has side effects on subsequent calls to fgetc: it returns eof forever, until some write is done.
	if(nread==0)
		nread=-1;

	//printf("usbrd ret: %d\n",nread);
	return nread;


}

extern uint8_t UserTxBufferFS[];
ssize_t serial_usb_cookie_write(void *__cookie, const char *__buf, size_t __n)
{
	size_t i=0;


	// Debug: "silent writes"
	if(!_serial_usb_write_enabled)
		return __n;



	/*ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Test if enough space
		unsigned f = buffer_freespace(&SERIALPARAM_USB.txbuf);
		//if(__n>f)
			//itmprintf("cookiewrite: not en spc %d %d\n",f,__n);
	}*/

	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;

	// Check if must block until space available
	if(sp->blockingwrite)
	{
		// Block only if not in interrupt (e.g. if fprintf called from interrupt), otherwise buffers will never empty
		if(!is_in_interrupt())
		{
			// Block only if the buffer is large enough to have a chance to hold the data
			if(sp->txbuf.size>__n)
			{
				while(buffer_freespace(&sp->txbuf)<__n)
					__NOP();
			}
		}
	}

	// If no buffering when disconnected return
	if( (sp->bufferwhendisconnected==0) && (system_isusbconnected()==0) )
	{
		//itmprintf("\tusbnowr\n");
		return __n;		// Simulate we wrote all data to empty the FILE buffer.
	}


	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)		// Overall lock to use the faster non-lock _buffer_put
	{
		// Get space in buffer
		unsigned maxn = buffer_freespace(&SERIALPARAM_USB.txbuf);
		// Compute how much can be written
		if(maxn<__n)
			__n = maxn;

		// Put the data in buffer
		for(i=0;i<__n;i++)
		{
			//buffer_put(&SERIALPARAM_USB.txbuf,__buf[i]);
			_buffer_put(&SERIALPARAM_USB.txbuf,__buf[i]);		// Can use the non-locking version
			//printf("%c ",__buf[i]);
		}
	}
	//printf("wr %d bl %d\n",__n,buffer_level(&SERIALPARAM_USB.txbuf));

	// Try to send
	CDC_TryTransmit_FS();

	return i; 		// Return true number put in the usb circular buffer

	//return __n;	//
}
/******************************************************************************
	function: _serial_usb_enable_write
*******************************************************************************
	Disable cookie_write (fputs, fprintf, etc) for debugging/benchmarking purposes.


	Return value:
		-
******************************************************************************/
void _serial_usb_enable_write(unsigned char en)
{
	_serial_usb_write_enabled=en;
}
/******************************************************************************
	function: serial_usb_putbuf
*******************************************************************************
	Atomically writes a buffer to a stream, or fails if the buffer is full.

	Return value:
		0			-	Success
		nonzero		-	Error
******************************************************************************/
unsigned char serial_usb_putbuf(SERIALPARAM *sp,char *data,unsigned short n)
{
	(void) sp;
	unsigned char isnewline=0;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(!system_isusbconnected())
			return 1;
		if(buffer_freespace(&SERIALPARAM_USB.txbuf)<n)
			return 1;
		for(unsigned short i=0;i<n;i++)
		{
			if(data[i]==13 || data[i]==10)
				isnewline=1;
			buffer_put(&SERIALPARAM_USB.txbuf,data[i]);
		}
	}

	// Send only sporadically to speed up
	if(isnewline || buffer_level(&SERIALPARAM_USB.txbuf)>=16)
		CDC_TryTransmit_FS();

	// Send always
	//CDC_TryTransmit_FS();
	return 0;
}

/******************************************************************************
	function: serial_usb_fischar
*******************************************************************************
	Checks if character in receive buffer.

	Return value:
		0			-	No data
		nonzero		-	Data
******************************************************************************/
unsigned char serial_usb_fischar(SERIALPARAM *sp)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(buffer_level(&SERIALPARAM_USB.rxbuf))
			return 1;
	}
	return 0;
}


/******************************************************************************
*******************************************************************************
BUFFER MANIPULATION --- BUFFER MANIPULATION --- BUFFER MANIPULATION --- BUFFER
*******************************************************************************
******************************************************************************/

/******************************************************************************
	serial_usb_clearbuffers
*******************************************************************************
	Clear all the data in the RX and TX buffers.
******************************************************************************/
void serial_usb_clearbuffers(void)
{
	buffer_clear(&SERIALPARAM_USB.txbuf);
	buffer_clear(&SERIALPARAM_USB.rxbuf);
}


