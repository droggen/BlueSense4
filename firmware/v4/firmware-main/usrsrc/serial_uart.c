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
#include "wait.h"

/******************************************************************************
	file: serial_uart
*******************************************************************************
	Abstraction to use stdio functions on a uart.



	*Implementation details*

	RX without DMA: Interrupt driven. RX HW flow control enabled (RTS). Data received on RXNE interrupt
	RX with DMA: RX flow control (RTS) done by software. DMA HT and TC interrupt and serial IDLE interrupt call serial_uart_dma_rx.
	serial_uart_dma_rx moves the data from the DMA buffer to the circular buffer. If the circular buffer is too low, it deasserts RTS.
	Whenever cookie_read reads and the circular buffer is high enough again, reassert RTS.

	TX DMA:
	- cookie_write or putbut: try_dma_tx
	- cts deasserted: pause transfer by LL_USART_DisableDMAReq_TX
	- end dma: try_dma_tx
	try_dma_tx:
	- ongoing transfer (dma active): do nothing
	- no data in tx buf: do nothing
	- cts not asserted: do nothing

	TOCHECK: DMA moves data after TXE events; UART transmits a frame if CTS asserted. Therefore if CTS is not asserted,
	further DMA transfers are automatically paused.


	* Main functions*







TODO:

******************************************************************************/


#warning significant overrun errors when receiving data at 460K.

SERIALPARAM _serial_uart_param[SERIALUARTNUMBER];
char _serial_uart_param_used[SERIALUARTNUMBER];

// Memory for the circular buffers
unsigned char _serial_uart_rx_buffer[SERIALUARTNUMBER][SERIAL_UART_RX_BUFFERSIZE];
unsigned char _serial_uart_tx_buffer[SERIALUARTNUMBER][SERIAL_UART_TX_BUFFERSIZE];
// DMA buffers
volatile unsigned char _serial_uart_rx_dma_buffer[SERIAL_UART_DMA_RX_BUFFERSIZE];
volatile unsigned char _serial_uart_tx_dma_buffer[DMATXLEN];


volatile unsigned long Serial1DOR=0;				// Data overrun
volatile unsigned long Serial1NR=0;					//
volatile unsigned long Serial1FE=0;					//
volatile unsigned long Serial1PE=0;					//
volatile unsigned long Serial1TXE=0;					//
volatile unsigned long Serial1RXNE=0;					//
volatile unsigned long Serial1CTS=0;					//
volatile unsigned long Serial1Idle=0;					//
volatile unsigned long Serial1DMARXOverrun=0;			//
volatile unsigned long Serial1DMARXInt=0;
volatile unsigned long Serial1DMATXInt=0;
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
	serial_open_uart
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

	int p = serial_uart_init(periph,0,0);			// 1 for DMA; 0 for interrupt
	//int p = serial_uart_init(periph,1,1);			// 1 for DMA; 0 for interrupt

	//itmprintf("aft serial_uart_init\n");

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

	//itmprintf("fopencookie: %p\n",f);

	// Buffering can lead to issues when entering command modes ($$$)
	setvbuf (f, 0, _IONBF, 0 );	// No buffering
	//setvbuf (f, 0, _IOLBF, 1024);	// Line buffer buffering
	//setvbuf (f, 0, _IOLBF, 16);	// Line buffer buffering
	//setvbuf (f, 0, _IOLBF, 64);	// Line buffer buffering


	// Or: big hack - _cookie seems unused in this libc - this is not portable.
	f->_cookie = sp;






	return f;
}

// Init uart
// -1: error
// >=0 ok
int serial_uart_init(USART_TypeDef *h,int dma1_or_int0,int dma1_or_int0_tx)
{
	//fprintf(file_pri,"serial_uart_init\n");
	// Static allocate a data structure
	int a = staticalloc_alloc(_serial_uart_param_used,SERIALUARTNUMBER);
	if(a==-1)	// Allocation failed
		return -1;


	// Initialise the circular buffers with the data buffers
	buffer_init(&_serial_uart_param[a].rxbuf,_serial_uart_rx_buffer[a],SERIAL_UART_RX_BUFFERSIZE);
	buffer_init(&_serial_uart_param[a].txbuf,_serial_uart_tx_buffer[a],SERIAL_UART_TX_BUFFERSIZE);

	// Initialise the high-level data structure
	_serial_uart_param[a].device = h;
	_serial_uart_param[a].blocking = 0;
	_serial_uart_param[a].blockingwrite = 0;
	_serial_uart_param[a].bufferwhendisconnected = 0;
	_serial_uart_param[a].putbuf = &serial_uart_putbuf;
	_serial_uart_param[a].fischar = &serial_uart_fischar;
	_serial_uart_param[a].dma1_or_int0 = dma1_or_int0;
	_serial_uart_param[a].dma1_or_int0_tx = dma1_or_int0_tx;

	// Initialise the hardware
	serial_uart_init_ll(dma1_or_int0,dma1_or_int0_tx);
	//itmprintf("before ie\n");
	//fprintf(file_pri,"about to enable interrupt\n");
	//serial_usart_interruptenable(h);

	//itmprintf("after ie\n");
	//fprintf(file_pri,"after enable interrupt\n");

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


/*void serial_usart_interruptenable(USART_TypeDef *h)
{
	fprintf(file_pri,"In interrupt enable %p\n",h);
	HAL_Delay(100);


	// RX, TX interrupt enable
	h->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

	fprintf(file_pri,"RN\n",h);
	HAL_Delay(100);

	//h->CR1 |= USART_CR1_RXNEIE;			// RX not empty interrupt enable

	fprintf(file_pri,"CTS\n",h);
		HAL_Delay(100);

	h->CR3 |= USART_CR3_CTSIE;		// Enable CTS interrupt


}*/
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

	//fprintf(file_pri,"IRQ %08X\n",sr);
	//fprintf(file_usb,"IRQ %08X\n",sr);
	//fprintf(file_usb,"I\n");

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

	// IDLE interrupt occur when no data is received. In DMA mode trigger a check of the RX DMA buffer
	if(sr & USART_ISR_IDLE)
	{
		Serial1Idle++;
		h->ICR|=USART_ICR_IDLECF;
		if(_serial_uart_param[p].dma1_or_int0==1)
			serial_uart_dma_rx(p);
	}

	// CTS interrupt
	if(sr & USART_ISR_CTSIF)
	{
		Serial1CTS++;
		h->ICR|=USART_ICR_CTSCF;
		itmprintf("CTS\n");
	}

	// RX interrupt - used only in non-DMA mode
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
		// STM32L4: clear by writing 1 to ORECF in ICR
		h->ICR=USART_FLAG_ORE;

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
	function: serial_usart_dma_rx_irq
*******************************************************************************
	IRQ handler for UART DMA.

	Note: called from DMA1_Channel6_IRQHandler in Src/stm32l4xx_it.c



	Return value:	-
******************************************************************************/
void serial_usart_dma_rx_irq()
{
	Serial1DMARXInt++;		// Increment the DMA RX interrupt counter

	//fprintf(file_usb,"D\n");
	/*fprintf(file_pri,"DMA IRQ:\n");
	for(int i=0;i<10;i++)
	{
		uint32_t datlen = LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);

		fprintf(file_pri,"\t %d\n",datlen);
	}*/

	// Check half-transfer complete interrupt
	if (LL_DMA_IsEnabledIT_HT(DMA1, LL_DMA_CHANNEL_6) && LL_DMA_IsActiveFlag_HT6(DMA1))
	{
		//fprintf(file_pri,"HT\n");
		LL_DMA_ClearFlag_HT6(DMA1);             	// Clear half-transfer complete flag
		serial_uart_dma_rx(0);
	}

	// Check transfer-complete interrupt
	if (LL_DMA_IsEnabledIT_TC(DMA1, LL_DMA_CHANNEL_6) && LL_DMA_IsActiveFlag_TC6(DMA1))
	{
		//fprintf(file_pri,"TC\n");
		LL_DMA_ClearFlag_TC6(DMA1);             // Clear transfer complete flag
		serial_uart_dma_rx(0);
	}

}

void serial_uart_dma_rx(unsigned char serialid)
{
	static unsigned old_pos=0;			// Index of the first byte of the DMA buffer to transmit to the larger ring buffer.

	//fprintf(file_pri,"R\n");

	// Check if DMA is activated - this function could be called when "idle" if DMA isn't activated yet
	if(!LL_USART_IsEnabledDMAReq_RX(USART2))
	{
		//fprintf(file_pri,"rx_check: dma not active\n");
		return;
	}


	// LL_DMA_GetDataLength: returns how many bytes left before wrapping around. Successive calls show decreasing values until wrapping around
	unsigned datleft=(unsigned)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_6);
	unsigned pos = SERIAL_UART_DMA_RX_BUFFERSIZE-datleft;

	//fprintf(file_pri,"datleft: %d. copy from %d incl to %d excl\n",datleft,old_pos,pos);

	// Check how much space is left in the buffer
	unsigned freespace = buffer_freespace(&_serial_uart_param[serialid].rxbuf);
	// If the space is below the DMA buffer size -> clear RTS
	if(freespace<SERIAL_UART_DMA_RX_BUFFERSIZE)
		_serial_usart_rts_clear();
	while(old_pos!=pos)
	{
		// Get data at position pos
		unsigned char c = _serial_uart_rx_dma_buffer[old_pos];
		//fprintf(file_pri,"%c",c);
		// Add the character to the ring buffer if space available
		// If rx buffer full: choose to lose rx data

		/*
		// Implementation calling isfull everytime - more expensive than relying on freespace
		if(!buffer_isfull(&_serial_uart_param[serialid].rxbuf))
			buffer_put(&_serial_uart_param[serialid].rxbuf,c);
		else
			Serial1DMARXOverrun++;*/
		// Implementation relying on freespace
		if(freespace)
		{
			buffer_put(&_serial_uart_param[serialid].rxbuf,c);
			freespace--;
		}
		else
			Serial1DMARXOverrun++;

		// If the amount of free space in the ring buffer decreases below the DMA buffer size -> must deactivate RTS

		// Increment wrapping around
		old_pos=(old_pos+1)&(SERIAL_UART_DMA_RX_BUFFERSIZE-1);


	}
}
/******************************************************************************
	function: serial_usart_dma_tx_irq
*******************************************************************************
	IRQ handler for UART DMA TX.

	Note: called from DMA1_Channel7_IRQHandler in Src/stm32l4xx_it.c

	Only action: deactivate the channel at the end of the transfer to indicate
	a transfer is completed.

	Assume only the TC interrupt is enabled



	Return value:	-
******************************************************************************/
void serial_usart_dma_tx_irq()
{
	Serial1DMATXInt++;

	//fprintf(file_usb,"d\n");
	//unsigned datleft=(unsigned)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_7);
	//unsigned long t = timer_us_get();

	//fprintf(file_pri,"DMA TX IRQ: %d at %ld\n",datleft,t);
	// Check half-transfer complete interrupt. HT is not enabled -> deactivate this.
	/*if (LL_DMA_IsEnabledIT_HT(DMA1, LL_DMA_CHANNEL_7) && LL_DMA_IsActiveFlag_HT7(DMA1))
	{
		//fprintf(file_pri,"HT\n");
		LL_DMA_ClearFlag_HT7(DMA1);             	// Clear half-transfer complete flag
	}*/
	// Check transmit error. TE is not enabled -> deactivate this.
	/*if (LL_DMA_IsEnabledIT_TE(DMA1, LL_DMA_CHANNEL_7) && LL_DMA_IsActiveFlag_TE7(DMA1))
	{
		//fprintf(file_pri,"TE\n");
		LL_DMA_ClearFlag_TE7(DMA1);
	}*/

	// Check transfer-complete interrupt
	if (LL_DMA_IsEnabledIT_TC(DMA1, LL_DMA_CHANNEL_7) && LL_DMA_IsActiveFlag_TC7(DMA1))
	{
		//fprintf(file_usb,"TC. \n");
		LL_DMA_ClearFlag_TC7(DMA1);

		// Deactivate channel
		LL_DMA_DisableChannel(DMA1,LL_DMA_CHANNEL_7);

		// Send new data if any
		serial_uart_dma_tx(0);
	}

}

/******************************************************************************
	function: serial_uart_dma_tx
*******************************************************************************
	Attempts to start a DMA TX transfer.

	This function is called by:
	- the TC DMA interrupt
	- cookie_write
	- fputbuf
	- any other function writing to the tx circular buffer

	This function:
	- If any data to transfer: if not, do nothing
	- Checks if ongoing DMA transfer: if yes, do nothing
	- If connected or if not connected whether transmit while not connected is enabled



	Return value:	-
******************************************************************************/
void serial_uart_dma_tx(unsigned char serialid)
{
	//fprintf(file_usb,"serial_uart_dma_tx\n");

	// Check if any data to transfer in the tx circular buffer
	unsigned buflevel = buffer_level(&_serial_uart_param[serialid].txbuf);
	if(buflevel==0)
		return;
	// Check if ongoing DMA transfer. If yes, return.
	if(LL_DMA_IsEnabledChannel(DMA1,LL_DMA_CHANNEL_7))
		return;
	// Channel is free and data is available - fill in data
	unsigned n=buflevel;
	if(DMATXLEN<n)
		n=DMATXLEN;
	for(unsigned i=0;i<n;i++)
		_serial_uart_tx_dma_buffer[i] = buffer_get(&_serial_uart_param[serialid].txbuf);

	// Configure DMA
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, n);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)_serial_uart_tx_dma_buffer);
	// Clear all flags
	LL_DMA_ClearFlag_TC7(DMA1);
	LL_DMA_ClearFlag_HT7(DMA1);
	LL_DMA_ClearFlag_TE7(DMA1);
	// Start transfer
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
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
ssize_t serial_uart_cookiewrite_int(void *__cookie, const char *__buf, size_t __nbytes)
{
	__ssize_t nw=0;
	unsigned c=0;

	//fprintf(file_usb,"cookiewrite: %d\n",__nbytes);

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
			if(sp->dma1_or_int0_tx==0)
			{
				// Interrupt mode: immediately trigger an interrupt if TXE empty
				((USART_TypeDef*)sp->device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable
			}
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
	if(sp->dma1_or_int0_tx==1)
	{
		// DMA mode: only start DMA once the data has been copied to the circular buffer.
		serial_uart_dma_tx(0);		// Hacked to first usart
	}
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

	// If DMA mode, set RTS if enough space
	if(sp->dma1_or_int0)
	{
		if(buffer_freespace(&sp->rxbuf)>=SERIAL_UART_DMA_RX_BUFFERSIZE)
			_serial_usart_rts_set();
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
		// If no buffering when disconnected return
		if( (sp->bufferwhendisconnected==0) && (system_isbtconnected()==0) )
		{
			return 0;		// Simulate we wrote data successfully; but discard it.
		}

		if(!system_isbtconnected())
			return 1;
		if(buffer_freespace(&sp->txbuf)<n)
			return 1;
		for(unsigned short i=0;i<n;i++)
			buffer_put(&sp->txbuf,data[i]);
	}
	if(sp->dma1_or_int0_tx==0)
	{
		// Interrupt mode: trigger an interrupt when UDR is empty and if wrote data
		if(n)
			((USART_TypeDef*)sp->device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable
	}
	else
	{
		// DMA mode: try to send data
		serial_uart_dma_tx(0);			// Hacked to first usart
	}

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
/******************************************************************************
	function: serial_uart_init_ll
*******************************************************************************
	Low-level UART initialisation - replaces CubeMX initialisation code.

	Initialisation with RX via DMA and TX via interrupts.

	Info on DMA: https://github.com/MaJerle/stm32-usart-uart-dma-rx-tx/blob/master/projects/usart_rx_idle_line_irq_L4/Src/main.c

	Parameters:
		dma1_or_int0		-		1 for RX by DMA; 0 for RX by interrupt


******************************************************************************/
void serial_uart_init_ll(int dma1_or_int0,int dma1_or_int0_tx)
{
	LL_USART_InitTypeDef USART_InitStruct = {0};
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* Peripheral clock enable */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART2);
	LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

	/**USART2 GPIO Configuration
	PA0   ------> USART2_CTS
	PA1   ------> USART2_RTS
	PA2   ------> USART2_TX
	PA3   ------> USART2_RX
	*/
	// Set TX, RX and CTS to alternate mode
	GPIO_InitStruct.Pin = LL_GPIO_PIN_0|LL_GPIO_PIN_2|LL_GPIO_PIN_3;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
	LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	if(dma1_or_int0==0)
	{
		// If interrupt mode, set RTS to alternate mode
		GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
		GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
		GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
		GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
		LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	}
	else
	{
		// If DMA mode, set RTS to manual
		GPIO_InitStruct.Pin = LL_GPIO_PIN_1;
		GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
		GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
		GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
		GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
		LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
		// Set RTS to 1
		_serial_usart_rts_set();
	}



	// USART2 DMA RX init
	if(dma1_or_int0 == 1)
	{
		LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_6, LL_DMA_REQUEST_2);
		LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_6, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
		LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PRIORITY_LOW);
		LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MODE_CIRCULAR);
		LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PERIPH_NOINCREMENT);
		LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MEMORY_INCREMENT);
		LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PDATAALIGN_BYTE);
		LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MDATAALIGN_BYTE);
		LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)&USART2->RDR);
		LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)_serial_uart_rx_dma_buffer);		// Buffer to receive data
		LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, SERIAL_UART_DMA_RX_BUFFERSIZE);								// Buffer size

		// Enable HT & TC interrupts
		LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_6);
		LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_6);
	}
	// USART2 DMA TX init
	if(dma1_or_int0_tx == 1)
	{
		LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_7, LL_DMA_REQUEST_2);
		LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_7, LL_DMA_DIRECTION_MEMORY_TO_PERIPH);
		LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_7, LL_DMA_PRIORITY_LOW);
		LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_7, LL_DMA_MODE_NORMAL);
		LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_7, LL_DMA_PERIPH_NOINCREMENT);
		LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_7, LL_DMA_MEMORY_INCREMENT);
		LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_7, LL_DMA_PDATAALIGN_BYTE);
		LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_7, LL_DMA_MDATAALIGN_BYTE);

		LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)&USART2->TDR);
		//LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, DMATXLEN);			// No need to specify len as no transfer will be initiated now
		LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)_serial_uart_tx_dma_buffer);

		// Enable HT & TC interrupts
		//LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_7);
		LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_7);		// Only transmit complete is needed in normal mode
		//LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_7);
	}


	// USART configuration
	USART_InitStruct.BaudRate = 115200;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	// In RX DMA RTS is done in software.
	// CTS is done in hardware in TX by DMA and by interrupt
	if(dma1_or_int0 == 0)
	{
		USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS_CTS;
	}
	else
	{	// DMA mode -> only CTS (flow control for transfer) for now
		USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_CTS;
	}


	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;		// Override

	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	ErrorStatus status = LL_USART_Init(USART2, &USART_InitStruct);
	if(status == ERROR)
		fprintf(file_usb,"Error init USART\n");
	LL_USART_ConfigAsyncMode(USART2);

	if(dma1_or_int0 == 1)
		LL_USART_EnableDMAReq_RX(USART2);
	if(dma1_or_int0_tx == 1)
		LL_USART_EnableDMAReq_TX(USART2);

	if(dma1_or_int0_tx == 0)
		LL_USART_EnableIT_TXE(USART2);			// No TX DMA -> enable TXE interrupt
	LL_USART_EnableIT_CTS(USART2);
	if(dma1_or_int0 == 0)
		LL_USART_EnableIT_RXNE(USART2);			// No RX DMA -> enable RX interrupt
	if(dma1_or_int0 == 1)
		LL_USART_EnableIT_IDLE(USART2);			// RX DMA -> Enable idle interrupt

#if 0	// Legacy
	// RX, TX interrupt enable
	h->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

	fprintf(file_pri,"RN\n",h);
	HAL_Delay(100);

	//h->CR1 |= USART_CR1_RXNEIE;			// RX not empty interrupt enable

	fprintf(file_pri,"CTS\n",h);
		HAL_Delay(100);

	h->CR3 |= USART_CR3_CTSIE;		// Enable CTS interrupt
#endif



	// USART interrupt activated anyways
	NVIC_SetPriority(USART2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
	NVIC_EnableIRQ(USART2_IRQn);


	// Enable DMA
	if(dma1_or_int0 == 1)
		LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);
	// DMA1 channel 7 only activated when actual data needs transferring
	//if(dma1_or_int0_tx == 1)
		//LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);			// Don't enable channel yet

	// Enable USART
	LL_USART_Enable(USART2);
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
void serial_uart_changespeed(int speed)
{
	LL_USART_InitTypeDef USART_InitStruct = {0};

	USART_InitStruct.BaudRate = speed;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS_CTS;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_RTS;
	//USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_CTS;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	LL_USART_Disable(USART2);
	ErrorStatus status = LL_USART_Init(USART2, &USART_InitStruct);
	if(status == ERROR)
		fprintf(file_usb,"Error init USART\n");
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
	fprintf(f,"\tIdle: %lu\n",Serial1Idle);
	fprintf(f,"\tDMARXOverrun: %lu\n",Serial1DMARXOverrun);
	fprintf(f,"\tDMARXInt: %lu\n",Serial1DMARXInt);
	fprintf(f,"\tDMATXInt: %lu\n",Serial1DMATXInt);
	fprintf(f,"\tEvents during interrupt: %lu\n",Serial1EvtWithinInt);

	// Print if
	fprintf(f,"CR1: %08X\n",USART2->CR1);
	fprintf(f,"CR1: idle %d\n",(USART2->CR1&0x10)?1:0);
	fprintf(f,"CR1: rxne %d\n",(USART2->CR1&0x20)?1:0);
	fprintf(f,"CR1: tc %d\n",(USART2->CR1&0x40)?1:0);
	fprintf(f,"CR1: txe %d\n",(USART2->CR1&0x80)?1:0);
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
	Serial1Idle=0;
	Serial1DMARXOverrun=0;
	Serial1DMARXInt=0;
	Serial1DMATXInt=0;
	Serial1EvtWithinInt = 0;
	Serial1Int=0;
}

void _serial_usart_rts_set()
{
	// STM: RTS is an nRtS -> setting RTS resets the pin
	if(LL_GPIO_IsOutputPinSet(GPIOA,LL_GPIO_PIN_1))
	{
		LL_GPIO_ResetOutputPin(GPIOA,LL_GPIO_PIN_1);
		//fprintf(file_pri,"*R\n");
	}
}
void _serial_usart_rts_clear()
{
	//return;
	// STM: RTS is an nRtS -> clearing RTS sets the pin
	if(!LL_GPIO_IsOutputPinSet(GPIOA,LL_GPIO_PIN_1))
	{
		LL_GPIO_SetOutputPin(GPIOA,LL_GPIO_PIN_1);
		//fprintf(file_pri,"*r\n");
	}

}

