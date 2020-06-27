/*
 * serial_uart.c
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

//#define _GNU_SOURCE
//#define __GNU_VISIBLE 1
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "serial_itm.h"
#include "serial_uart.h"
#include "usrmain.h"
#include "atomicop.h"
#include "serial.h"
#include "system.h"
#include "global.h"

#warning significant overrun errors when receiving data at 460K.

SERIALPARAM _serial_uart_param[SERIALUARTNUMBER];
char _serial_uart_param_used[SERIALUARTNUMBER];

// Memory for the circular buffers
unsigned char _serial_uart_rx_buffer[SERIALUARTNUMBER][SERIAL_UART_RX_BUFFERSIZE];
unsigned char _serial_uart_tx_buffer[SERIALUARTNUMBER][SERIAL_UART_TX_BUFFERSIZE];

volatile unsigned long Serial1DOR=0;				// Data overrun
volatile unsigned long Serial1NR=0;					//
volatile unsigned long Serial1FE=0;					//
volatile unsigned long Serial1PE=0;					//
volatile unsigned long Serial1TXE=0;					//
volatile unsigned long Serial1RXNE=0;					//
volatile unsigned long Serial1CTS=0;					//
volatile unsigned long Serial1Int=0;					// Total interrupt
volatile unsigned long Serial1EvtWithinInt=0;		// Counts if there is a new int-triggering flag set during the interrupt process, could be used to speed up

int staticalloc_alloc(char *allocmap,int max)
{
	for(int i=0;i<max;i++)
	{
		if(allocmap[i]==0)
		{
			allocmap[i]=1;
			return i;
		}
	}
	return -1;
}
//int staticalloc_free(char *allocmap,int max)


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
// Specialised function to open UART
// interrupt: 1 to use interrupts, 0 for polling

FILE *serial_open_uart(USART_TypeDef *periph,int *__p)
{
	FILE *f=0;

	int p = serial_uart_init(periph,1);

	itmprintf("aft serial_uart_init\n");

	// optional
	if(__p)
		*__p = p;

	// Failure
	if(p==-1)
		return (FILE*)-1;

	// Get the serial param structure
	SERIALPARAM *sp = serial_uart_getserialparam(p);

	//

	// USB
	cookie_io_functions_t iof;

	iof.read = &serial_uart_cookieread_int;
	iof.write = &serial_uart_cookiewrite_int;
	iof.close = 0;
	iof.seek = 0;
	f = fopencookie((void*)sp,"w+",iof);

	itmprintf("fopencookie: %p\n",f);

	// Buffering can lead to issues when entering command modes ($$$)
	//setvbuf (f, 0, _IONBF, 0 );	// No buffering
	//setvbuf (f, 0, _IOLBF, 1024);	// Line buffer buffering
	//setvbuf (f, 0, _IOLBF, 16);	// Line buffer buffering
	setvbuf (f, 0, _IOLBF, 64);	// Line buffer buffering



	//serial_associate(f,sp);			// Use big hack with f->_cookie below

	// Or: big hack - _cookie seems unused in this libc - this is not portable.
	f->_cookie = sp;


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

// Init uart
// -1: error
// >=0 ok
int serial_uart_init(USART_TypeDef *h,int interrupt)
{
	// Static allocate a data structure
	int a = staticalloc_alloc(_serial_uart_param_used,SERIALUARTNUMBER);
	if(a==-1)	// Allocation failed
		return -1;

	// Initialise the hardware


	itmprintf("serial_uart_init: ok (%d)\n",a);

	// Initialise the circular buffers with the data buffers
	buffer_init(&_serial_uart_param[a].rxbuf,_serial_uart_rx_buffer[a],SERIAL_UART_RX_BUFFERSIZE);
	buffer_init(&_serial_uart_param[a].txbuf,_serial_uart_tx_buffer[a],SERIAL_UART_TX_BUFFERSIZE);

	// Initialise the handle
	_serial_uart_param[a].device = h;


	_serial_uart_param[a].blocking = 0;
	_serial_uart_param[a].bufferwhendisconnected = 0;
	_serial_uart_param[a].putbuf = &serial_uart_putbuf;
	_serial_uart_param[a].fischar = &serial_uart_fischar;

	// Initialise the interrupts
	itmprintf("before ie\n");
	if(interrupt)
	{
		serial_usart_interruptenable(h);
	}
	itmprintf("after ie\n");

	// Return ID of serial_uart
	return a;

}

SERIALPARAM *serial_uart_getserialparam(int p)
{
	return &_serial_uart_param[p];
}

int serial_uart_findparam(USART_TypeDef *h)
{
	for(int i=0;i<SERIALUARTNUMBER;i++)
	{
		if(_serial_uart_param_used[i] && _serial_uart_param[i].device==h)
			return i;
	}
	return -1;
}

//SERIAL_UART_HANDLE huart2;

//void USART2_IRQHandler(void)
//{
	//itmprintf("usart2irq\n");

  /* USER CODE BEGIN USART2_IRQn 0 */

  /* USER CODE END USART2_IRQn 0 */
  //HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
//}

/*void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	itmprintf("txcb\n");
}*/

/******************************************************************************
   function: serial_usart_txempty
*******************************************************************************
	Checks if TXE (transmit data register empty) is set.

	Parameters:
		h	-		Pointer to a USART

	Returns:
		1	-		Transmit data register empty
		0	-		Transmit data register not empty
******************************************************************************/
int serial_usart_txempty(USART_TypeDef *h)
{
	return (h->ISR & USART_FLAG_TXE);
}
/******************************************************************************
   function: serial_usart_rxnotempty
*******************************************************************************
	Checks if RXNE (Read data register not empty) is set.

	Parameters:
		h	-		Pointer to a USART

	Returns:
		1	-		Received data ready to be read
		0	-		Data not received
******************************************************************************/
int serial_usart_rxnotempty(USART_TypeDef *h)
{
	return (h->ISR & USART_FLAG_RXNE);
}
void serial_usart_waittxempty(USART_TypeDef *h)
{
	while(!serial_usart_txempty(h));
}


void serial_usart_putbuff_block(USART_TypeDef *h, char *buffer,int n)
{
	for(int i=0;i<n;i++)
	{
		serial_usart_putchar_block(h,buffer[i]);
	}
}

// returns number of characters read, with maxn, and timeoutms from the start of the function call
/*int serial_usart_getbuff_nonblock(USART_TypeDef *h, char *buffer,int maxn,int timeoutms)
{
	int t1 = HAL_GetTick();
	int r=0;

	while(1)
	{
		// First check, if maxn is zero.
		if(r>=maxn)
			return r;

		// Wait while not data and until timeout
		while(1)
		{
			if(HAL_GetTick()-t1>timeoutms)
				return r;
			if(serial_usart_rxnotempty(h))
				break;
		}

		buffer[r] = h->DR;
		r++;
	}
}*/

/******************************************************************************
   function: serial_usart_putchar_block
*******************************************************************************
	Wait until the serial TX buffer is empty (blocking) and write a byte

	Parameters:
		h	-		Pointer to a USART
		c	-		Byte to write

	Returns:
		-
******************************************************************************/
void serial_usart_putchar_block(USART_TypeDef *h, char c)
{
	// Wait for empty transmit buffer
	serial_usart_waittxempty(h);

	// Transmit
	h->TDR = c;
}
/******************************************************************************
   function: serial_usart_getchar_block
*******************************************************************************
	Wait until the serial RX buffer is full (blocking), read and return byte read.

	Parameters:
		h	-		Pointer to a USART

	Returns:
		Byte read
******************************************************************************/
unsigned char serial_usart_getchar_block(USART_TypeDef *h)
{
	while(!serial_usart_rxnotempty(h));
	return h->RDR;
}
/******************************************************************************
   function: serial_usart_getchar_block
*******************************************************************************
	Wait until the serial RX buffer is full (blocking), read and return byte read.

	Parameters:
		h	-		Pointer to a USART

	Returns:
		-1	-		No data was read
		Data -		Otherwise
******************************************************************************/
int serial_usart_getchar_nonblock(USART_TypeDef *h)
{
	if(serial_usart_rxnotempty(h))
		return h->RDR;
	return -1;
}


void serial_usart_interruptenable(USART_TypeDef *h)
{
	// RX, TX interrupt enable

	h->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

	h->CR1 |= USART_CR1_RXNEIE;			// RX not empty interrupt enable

	h->CR3 |= USART_CR3_CTSIE;		// Enable CTS interrupt


}
/******************************************************************************
	serial_uart_cookiewrite_block
*******************************************************************************
	Used with fcookieopen to write a buffer to a stream.

	This function is polling.

	Return value:
		number of bytes written (0 if none)
******************************************************************************/
//typedef ssize_t cookie_write_function_t(void *__cookie, const char *__buf, size_t __n);
/*ssize_t serial_uart_cookiewrite_block(void *__cookie, const char *__buf, size_t __nbytes)
{
	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;

	// Get the USART address
	USART_TypeDef *h = (USART_TypeDef*)sp->device;

	// Write byte by byte
	for(size_t i=0;i<__nbytes;i++)
	{
		// Wait for empty transmit buffer
		serial_usart_waittxempty(h);

		// Write one byte
		h->DR = __buf[i];

	}
	return __nbytes;
}*/



/*void serial_uart2_init()
{
	huart2.Instance = USART2;			// Base address of the USART

	//Desired parameters:
	//	huart2.Init.BaudRate = 115200;
	//	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	//	huart2.Init.StopBits = UART_STOPBITS_1;
	//	huart2.Init.Parity = UART_PARITY_NONE;
	//	huart2.Init.Mode = UART_MODE_TX_RX;
	//	huart2.Init.HwFlowCtl = UART_HWCONTROL_RTS_CTS;
	//	huart2.Init.OverSampling = UART_OVERSAMPLING_16;


	// USART2 clock enable
	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	//USART2 GPIO Configuration
	//PA0-WKUP     ------> USART2_CTS
	//PA1     ------> USART2_RTS
	//PA2     ------> USART2_TX
	//PA3     ------> USART2_RX

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// USART2 interrupt Init
	HAL_NVIC_SetPriority(USART2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(USART2_IRQn);

}*/


int serial_uart_gettxbuflevel(int p)
{
	return buffer_level(&_serial_uart_param[p].txbuf);
}
int serial_uart_getrxbuflevel(int p)
{
	return buffer_level(&_serial_uart_param[p].rxbuf);
}


/******************************************************************************
*******************************************************************************
INTERRUPT DRIVEN --- INTERRUPT DRIVEN --- INTERRUPT DRIVEN --- INTERRUPT DRIVEN
*******************************************************************************
******************************************************************************/

/******************************************************************************
	function: serial_usart_irq
*******************************************************************************
	IRQ handler for UART.

	Note: called from USART2_IRQHandler in Src/stm32l4xx_it.c

	h:				base address of uart

	Return value:	-
******************************************************************************/
void serial_usart_irq(USART_TypeDef *h)
{
	//itmprintf("i %04X\n",h->SR);

	Serial1Int++;

	unsigned sr = h->ISR;			// Get SR once

	// Find the data structure corresponding to the UART
	int p = serial_uart_findparam(h);

	//itmprintf("serial_usart_irq. p: %d\n",p);

	// No data structure corresponding to UART (shouldn't happen) -> clear interrupts
	if(p==-1)
	{
		itmprintf("serial_usart_irq: unknown usart\n");
		// Clear all interrupts
		// h->SR=0x0000;		// STM32F4 clear via SR
		h->ICR=0xffff;			// STM32L4 clear via interrupt flag clear register
		return;
	}





	// Find interrupt type

	// CTS interrupt
	if(sr & USART_ISR_CTSIF)
	{
		Serial1CTS++;
		h->ICR|=USART_ICR_CTSCF;
	}

	// RX interrupt?
	if(sr & USART_ISR_RXNE)
	{
		char c = h->RDR;

		Serial1RXNE++;

		//itmprintf("\tRX: %c %02X\n",c,c);
		// Store the data in the receive buffer
		// If rx buffer full: choose to lose rx data
		if(!buffer_isfull(&_serial_uart_param[p].rxbuf))
			buffer_put(&_serial_uart_param[p].rxbuf,c);
	}

	if(sr & USART_ISR_ORE)
	{
		// Overrun error
		//itmprintf("\tORE\n");
#if 0
		// STM32F4: Clear the interrupt by reading SR (done) and reading DR
		char c = h->DR;
		(void)c;
#endif

#if 1
		// STM32L4: clear by writing 1 to ORECF in ICR
		h->ICR=USART_FLAG_ORE;
#endif

		// Increment the overrun counter
		Serial1DOR++;
	}

#if 0
	if(sr & USART_FLAG_NF)
	{
		// Noise flag
		//itmprintf("\tNF\n");
		// STM32L4: clear by writing 1 to NFCF in ICR
		h->ICR=USART_FLAG_NF;

		Serial1NR++;
	}

	if(sr & USART_FLAG_FE)
	{
		// Framing error
		//itmprintf("\tFE\n");
		// STM32L4: clear by writing 1 to FECF in ICR
		h->ICR=USART_FLAG_FE;

		Serial1FE++;
	}
	if(sr & USART_FLAG_PE)
	{
		// Parity error
		//itmprintf("\tPE\n");

		// STM32L4: clear by writing 1 to FECF in ICR
		h->ICR=USART_FLAG_PE;

		Serial1PE++;
	}
#endif
	if( (sr & USART_FLAG_TXE) && (h->CR1&USART_CR1_TXEIE) )			// Must check for TXEIE so that we do not accumulate TXE counts while no TX was intended.
	{
		//itmprintf("\tTXE\n");

		Serial1TXE++;

		// Data must be sent to bluetooth anyways as it could be $$$ messages to configure the bluetooth chip
		// A better implementation would distinguish between communication to bluetooth chip or remote.
		/*if(!system_isbtconnected())
		{
			// Not connected - do not push data and deactivate interrupt otherwise we reenter continuously the loop
			h->CR1 &= ~USART_CR1_TXEIE;
			return;
		}*/

		if(!buffer_isempty(&_serial_uart_param[p].txbuf))
		{
			char c = buffer_get(&_serial_uart_param[p].txbuf);
			//itmprintf("\tTX: %c\n",c);
			h->TDR = c;
		}
		// Transmit data or deactivate interrupt
		if(buffer_isempty(&_serial_uart_param[p].txbuf))
		{
			// No data to transmit: deactivate interrupt otherwise we reenter continuously the loop
			h->CR1 &= ~USART_CR1_TXEIE;
		}
	}

	// Check SR again - it should be zero unless another event occurred during this interrupt
	sr = h->ISR;
	// Keep the interesting bits: LBD, TXE, RXNE, ORE, NF, FE, PE
	sr = sr & 0b0110101111;
	if(sr)
	{
		//itmprintf("%04X\n",sr);
		Serial1EvtWithinInt++;
	}


}

/******************************************************************************
	uart1_fputchar_block_int
*******************************************************************************
	Interrupt driven, blocking write.

	c:				character to write
	p:				UART id (returned by serial_uart_init)

	Return value:	0
******************************************************************************/
/*int serial_uart_putchar_block_int(char c, int p)
{
	// Wait until send buffer is free.
	while(buffer_isfull(&_serial_uart_param[p].txbuf));
	// Store the character in the buffer
	buffer_put(&_serial_uart_param[p].txbuf,c);

	// Trigger an interrupt when UDR is empty
	((USART_TypeDef*)_serial_uart_param[p].device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

	return 0;
}*/

/******************************************************************************
	uart1_fgetchar_nonblock_int
*******************************************************************************
	Interrupt driven, non-blocking read.

	p:				UART id (returned by serial_uart_init)

	Return value:
		EOF (-1):	No data available
		other:		Character read
******************************************************************************/
/*int serial_uart_getchar_nonblock_int(int p)
{
	char c;
	if(buffer_isempty(&_serial_uart_param[p].rxbuf))
		return -1;
	c = buffer_get(&_serial_uart_param[p].rxbuf);
	return ((int)c)&0xff;
}*/

/******************************************************************************
	uart1_fgetchar_int
*******************************************************************************
	Interrupt driven, possibly blocking read.
	Whether the access is blocking or non-blocking is defined by the previous
	call to uart_setblocking.

	stream:	unused, can be 0.

	Return value:
		EOF (-1):	No data available (only happens if non-blocking is enabled)
		other:		Character read
******************************************************************************/
//int uart1_fgetchar_int(FILE *stream)
//{
	/*int c;
	do{c=uart1_fgetchar_nonblock_int(stream);}
	while(c==EOF && serial_isblocking(stream));

	return c;*/
	//return 0;
//}


/******************************************************************************
	uart1_fputbuf_int
*******************************************************************************
	Atomically writes a buffer to a stream, or fails if the buffer is full.

	Return value:
		0:				success
		nonzero:		error
******************************************************************************/
/*int serial_uart_putbuf_int(char *data,unsigned char n,int p)
{
	if(serial_uart_txbufferfree(p)>=n)
	{
		for(unsigned short i=0;i<n;i++)
			buffer_put(&_serial_uart_param[p].txbuf,data[i]);
		// Trigger an interrupt when UDR is empty
		((USART_TypeDef*)_serial_uart_param[p].device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

		return 0;
	}
	return 1;
}*/


/******************************************************************************
	serial_uart_cookiewrite_int
*******************************************************************************
	Used with fcookieopen to write a buffer to a stream.

	This function is interrupt driven, i.e. uses a circular buffer.

	Return value:
		number of bytes written (0 if none)
******************************************************************************/
//typedef ssize_t cookie_write_function_t(void *__cookie, const char *__buf, size_t __n);
ssize_t serial_uart_cookiewrite_int(void *__cookie, const char *__buf, size_t __nbytes)
{
	__ssize_t nw=0;
	unsigned c=0;

	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;


	//fprintf(file_usb,"wr %d\n",__nbytes);
	//itmprintf("wr %d\n",__nbytes);
	//itmprintf("btwr n: %d. cookie: %p buffer?: %d btcon: %d\n",__nbytes,__cookie,sp->bufferwhendisconnected,system_isbtconnected());




	// If no buffering when disconnected return
	if( (sp->bufferwhendisconnected==0) && (system_isbtconnected()==0) )
	{
		//itmprintf("\tbtnowr\n");
		return __nbytes;		// Simulate we wrote all data to empty the FILE buffer.
	}


	// Write byte by byte
	for(size_t i=0;i<__nbytes;i++)
	{
		// Blocking write
		c=0;
		while(buffer_isfull(&(sp->txbuf)))
		{
			c++;
			//HAL_Delay(1);
			if( (sp->bufferwhendisconnected==0) && (system_isbtconnected()==0) )
			{
				// We can loose connection while waiting for buffer to empty.
				// Make sure we return simulating having written all data successfully, to empty the FILE buffer.
				//itmprintf("\tbtnowr\n");
				return __nbytes;		// Simulate we wrote all data to empty the FILE buffer.
			}
		}
		//itmprintf("n: %d i: %d c: %d\n",__nbytes,i,c);
		buffer_put(&(sp->txbuf),__buf[i]);
		{
			nw++;
			// Immediately trigger an interrupt if TXE empty
			((USART_TypeDef*)sp->device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable
		}
		/*
		// Non-blocking write
		if(!buffer_isfull(&(sp->txbuf)))
		{
			// Write to buffer if space
			buffer_put(&(sp->txbuf),__buf[i]);
			nw++;
		}
		else
			// No space - quit loop
			break;*/
	}
	// Trigger an interrupt when UDR is empty and if wrote
	//if(nw)
		//((USART_TypeDef*)sp->device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable
	return nw;
}
/******************************************************************************
	serial_uart_cookieread_int
*******************************************************************************
	Used with fcookieopen to read a buffer from a stream.

	This function is interrupt driven, i.e. uses a circular buffer.

	Return value:
		number of bytes read (0 if none)
******************************************************************************/
//typedef ssize_t cookie_read_function_t(void *__cookie, char *__buf, size_t __n);
ssize_t serial_uart_cookieread_int(void *__cookie, char *__buf, size_t __n)
{
	__ssize_t nr=0;

	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;

	// Read up to __n bytes
	for(size_t i=0;i<__n;i++)
	{
		if(!buffer_isempty(&(sp->rxbuf)))
		{
			__buf[i] = buffer_get(&(sp->rxbuf));
			nr++;
		}
		else
			break;
	}

	// If no data, should return error (-1) instead of eof (0).
	// Returning eof has side effects on subsequent calls to fgetc: it returns eof forever, until some write is done.
	if(nr==0)
		return -1;

	return nr;
}


/******************************************************************************
*******************************************************************************
BUFFER MANIPULATION --- BUFFER MANIPULATION --- BUFFER MANIPULATION --- BUFFER
*******************************************************************************
******************************************************************************/

/******************************************************************************
	function: serial_uart_putbuf
*******************************************************************************
	Atomically writes a buffer to a stream, or fails if the buffer is full.

	Never buffers if disconnected.

	Return value:
		0			-	Success
		nonzero		-	Error
******************************************************************************/
unsigned char serial_uart_putbuf(SERIALPARAM *sp,char *data,unsigned short n)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(!system_isbtconnected())
			return 1;
		if(buffer_freespace(&sp->txbuf)<n)
			return 1;
		for(unsigned short i=0;i<n;i++)
			buffer_put(&sp->txbuf,data[i]);
	}
	// Trigger an interrupt when UDR is empty and if wrote
	if(n)
		((USART_TypeDef*)sp->device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

	return 0;
}
/******************************************************************************
	function: serial_uart_fischar
*******************************************************************************
	Checks if character in receive buffer.

	Return value:
		0			-	No data
		nonzero		-	Data
******************************************************************************/
unsigned char serial_uart_fischar(SERIALPARAM *sp)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(buffer_level(&sp->rxbuf))
			return 1;
	}
	return 0;
}


/******************************************************************************
	serial_uart_clearbuffers
*******************************************************************************
	Clear all the data in the RX and TX buffers.
******************************************************************************/
void serial_uart_clearbuffers(void)
{
	// TOFIX: should free only a specific UART buffer
	for(int i=0;i<SERIALUARTNUMBER;i++)
	{
		buffer_clear(&_serial_uart_param[i].txbuf);
		buffer_clear(&_serial_uart_param[i].rxbuf);
	}
}

int serial_uart_txbufferfree(int p)
{
	return buffer_freespace(&_serial_uart_param[p].txbuf);
}

/*
Test when issuing H to RN41: counter
460K:
	22 overrun with cts rts
	302 overrun with none
	23 with rts
	321 with cts
230K:
	5 with rts
RTS: output of STM32 indicating readiness to receive data. RTS prevents receive overruns.

*/
// Speed: 115200, etc.
void serial_uart_init_ll(int speed)
{
	LL_USART_InitTypeDef USART_InitStruct = {0};

	USART_InitStruct.BaudRate = speed;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS_CTS;
	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS;
	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_CTS;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	LL_USART_Disable(USART2);
	ErrorStatus status = LL_USART_Init(USART2, &USART_InitStruct);
	if(status == ERROR)
		fprintf(file_pri,"Error init USART\n");
	LL_USART_ConfigAsyncMode(USART2);
	LL_USART_Enable(USART2);


}

void serial_uart_printreg(FILE *f)
{


	unsigned brr = USART2->BRR;
	unsigned cr1 = USART2->CR1;
	unsigned m = (brr>>4)&0x0fff;
	unsigned frac = brr&0x0f;
	unsigned ov=(cr1&0x8000)?1:0;
	unsigned frq=20000000;
	unsigned long long b;

	if(ov==0)
	{
		b = frq/(16*m+frac);
	}
	else
	{
		b = frq/(8*m+(frac&0b111));
	}

	fprintf(f,"UART registers:\n");
	fprintf(f,"\tISR: %04X\n",USART2->ISR);
	fprintf(f,"\tBRR: %04X (mantissa: %u fraction: %u)\n",brr,m,frac);
	fprintf(f,"\tCR1: %04X (over8 %d: oversampling by %s)\n",cr1,ov,ov?"8":"16");
	fprintf(f,"\tCR2: %04X\n",USART2->CR2);
	fprintf(f,"\tCR3: %04X\n",USART2->CR3);
	fprintf(f,"\tBaud rate: %u\n",b);
}

void serial_uart_printevents(FILE *f)
{
	fprintf(f,"UART events:\n");
	fprintf(f,"\tTotal interrupts: %lu\n",Serial1Int);
	fprintf(f,"\tOverrun: %lu\n",Serial1DOR);
	fprintf(f,"\tNF: %lu\n",Serial1NR);
	fprintf(f,"\tPE: %lu\n",Serial1PE);
	fprintf(f,"\tFE: %lu\n",Serial1FE);
	fprintf(f,"\tRXNE: %lu\n",Serial1RXNE);
	fprintf(f,"\tTXE: %lu\n",Serial1TXE);
	fprintf(f,"\tCTS: %lu\n",Serial1CTS);
	fprintf(f,"\tEvents during interrupt: %lu\n",Serial1EvtWithinInt);
}
void serial_uart_clearevents()
{
	Serial1DOR = 0;
	Serial1NR = 0;
	Serial1PE = 0;
	Serial1FE = 0;
	Serial1TXE = 0;
	Serial1RXNE = 0;
	Serial1CTS=0;
	Serial1EvtWithinInt = 0;
	Serial1Int=0;
}

