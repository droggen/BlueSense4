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
/*
Modifications required to the ST USB CDC stack:
- Modify USBD_CDC_DataIn in usbd_cdc.c to call usb_cdc_txready_callback
- Function CDC_TryTransmit_FS to transmit via circular buffer
- usbd_cdc_if.c: Modify CDC_Receive_FS to get data in circular buffer


*/

SERIALPARAM SERIALPARAM_USB;

volatile unsigned char USB_RX_DataBuffer[USB_BUFFERSIZE];
volatile unsigned char USB_TX_DataBuffer[USB_BUFFERSIZE];

uint8_t CDC_TryTransmit_FS();

extern USBD_HandleTypeDef hUsbDeviceFS;
void trytxbletooth()
{
	// Not busy - move as much data from circualr buffer to local buffer
	int nwrite = buffer_level(&SERIALPARAM_USB.txbuf);


	for(int i=0;i<nwrite;i++)
	{
		ITM_SendChar(buffer_get(&SERIALPARAM_USB.txbuf));
	}

}
unsigned char serial_usb_txcallback(unsigned char p)
{
	(void)p;
	static unsigned priorbusy=0;

	//itmprintf("c\n");
	//fflush(file_usb);

	int busy=0;
	USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
	if (hcdc->TxState != 0){
		busy=1;
		if(priorbusy==1 && busy==1)
			itmprintf("busy\n");
		priorbusy=busy;
		return;
	}
	priorbusy=0;

	/*if(system_isbtconnected())
	{
		char s[256];
		sprintf(s,"cb: lvl: %d bsy: %d\n",buffer_level(&SERIALPARAM_USB.txbuf),busy);
		fputbuf(file_bt,s,strlen(s));
		//fprintf(file_bt,"cb: lvl: %d bsy: %d\n",buffer_level(&SERIALPARAM_USB.txbuf),busy);
	}*/

	if(buffer_level(&SERIALPARAM_USB.txbuf))

		CDC_TryTransmit_FS();
		//trytxbletooth();

	return 0;
}

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
	//setvbuf (f, 0, _IONBF, 0 );	// No buffering
	setvbuf (f, 0, _IOLBF, 64);	// Line buffer buffering
	//setvbuf (f, stdiobuf, _IOLBF, 64);	// Line buffer buffering
	//setvbuf (f, 0, _IOLBF, 4);	// Line buffer buffering

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

	// Register a callback

	//timer_register_callback(serial_usb_txcallback,0);


	return f;
}
void serial_usb_initbuffers()
{
	// Initialise
	SERIALPARAM_USB.blocking = 0;
	SERIALPARAM_USB.blockingwrite = 0;
	SERIALPARAM_USB.bufferwhendisconnected = 0;
	//SERIALPARAM_USB.bufferwhendisconnected = 1;
	SERIALPARAM_USB.putbuf = serial_usb_putbuf;
	SERIALPARAM_USB.fischar = serial_usb_fischar;

	// Initialise the circular buffers with the data buffers
	buffer_init(&SERIALPARAM_USB.rxbuf,USB_RX_DataBuffer,USB_BUFFERSIZE);
	buffer_init(&SERIALPARAM_USB.txbuf,USB_TX_DataBuffer,USB_BUFFERSIZE);

	memset(USB_TX_DataBuffer,'@',USB_BUFFERSIZE);
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


	__buf[0]='A';
	return 1;
}

extern uint8_t UserTxBufferFS[];
ssize_t serial_usb_cookie_write(void *__cookie, const char *__buf, size_t __n)
{
	// New version!
	//HAL_Delay(2);
	// Check if USB ongoing
	/*USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
	if (hcdc->TxState != 0){
		return 0;	// We loose all the data - don't care
	}*/
	/*if(__n>2048)
		__n=2048;
	memcpy(UserTxBufferFS,__buf,__n);
	CDC_Transmit_FS(UserTxBufferFS,__n);
	return __n;*/



	//printf("usbwr\n");
	size_t i;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Test if enough space
		unsigned f = buffer_freespace(&SERIALPARAM_USB.txbuf);
		//if(__n>f)
			//itmprintf("cookiewrite: not en spc %d %d\n",f,__n);
	}

	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;

	// Check if must block until space available
	/*if(sp->blockingwrite)
	{
		// Block only if not in interrupt, otherwise buffers will never empty
		if(!is_in_interrupt())
		{
			// Block only if the buffer is large enough to have a chance to hold the data
			if(USB_BUFFERSIZE>__n)
			{
				while(buffer_freespace(&SERIALPARAM_USB.txbuf)<__n)
					HAL_Delay(1);
			}
		}
	}*/


	//itmprintf("usbwr n: %d. cookie: %p buffer?: %d usbcon: %d\n",__n,__cookie,sp->bufferwhendisconnected,system_isusbconnected());
	/*if(system_isbtconnected())
	{
		char s[256];
		int busy=0;
		USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
		if (hcdc->TxState != 0){
			busy=1;
		}
		sprintf(s,"cookwr: %d lvl: %d bsy: %d\n",__n,buffer_level(&SERIALPARAM_USB.txbuf),busy);
		fputbuf(file_bt,s,strlen(s));
	}*/

	// If no buffering when disconnected return
	if( (sp->bufferwhendisconnected==0) && (system_isusbconnected()==0) )
	{
		//itmprintf("\tusbnowr\n");
		return __n;		// Simulate we wrote all data to empty the FILE buffer.
	}


	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)		// Not needed as isfull and buffer_put deactivate int
	{
		// Put the data in buffer
		for(i=0;i<__n;i++)
		{
			// Non-blocking, but data buffered by FILE so no data loss.
			if(buffer_isfull(&SERIALPARAM_USB.txbuf))
				break;
			buffer_put(&SERIALPARAM_USB.txbuf,__buf[i]);
			//printf("%c ",__buf[i]);
		}
	}
	//printf("wr %d bl %d\n",__n,buffer_level(&SERIALPARAM_USB.txbuf));

	// Try to send
	CDC_TryTransmit_FS();

	//return i; // return number put in write buffer


	/*uint32_t rv = CDC_Transmit_FS(__buf,__n);
	if(rv==USBD_OK)
	{
		//printf("serial_usb_write ok %d\n",__n);
		return __n;
	}*/

	//printf("serial_usb_write fail %d\n",__n);
	// Otherwise: USBD_FAIL or USBD_BUSY
	//return i;
	return __n;
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

	// Send only sporadically
	/*if(isnewline || buffer_level(&SERIALPARAM_USB.txbuf)>=64)
		CDC_TryTransmit_FS();*/

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

