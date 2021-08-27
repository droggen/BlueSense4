/*
 * serial_usart3.c
 *
 *  Created on: 28 juin 2021
 *      Author: droggen
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "serial_uart.h"
#include "usrmain.h"
#include "atomicop.h"
#include "system.h"
#include "serial.h"
#include "serial_uart.h"
#include "serial_usart3.h"
#include "global.h"
#include "wait.h"
#include "stmrcc.h"


// Minimalistic USART3, only TX, no interrupt
SERIALPARAM _serial_usart3_param;
// Memory for the circular buffers
unsigned char _serial_uasrt3_tx_buffer[SERIAL_UART_TX_BUFFERSIZE];
unsigned _serial_usart3_cksumen=0;

FILE *_serial_uasrt3_file;

/*
	USART 3: APB1.
 	Initialise the Tx in alternate mode

	baudrate:		Baudrate in bps
	stopbit: 		One of LL_USART_STOPBITS_0_5, LL_USART_STOPBITS_1, LL_USART_STOPBITS_1_5, LL_USART_STOPBITS_2
*/
FILE *serial_usart3_init(unsigned baudrate,unsigned stopbit)
{
	LL_USART_InitTypeDef USART_InitStruct = {0};
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	LL_USART_Disable(USART3);

	// Initialise the high-level data structure
	_serial_usart3_param.device = 0;
	_serial_usart3_param.blocking = 0;
	_serial_usart3_param.blockingwrite = 0;
	_serial_usart3_param.bufferwhendisconnected = 0;
	_serial_usart3_param.putbuf = &serial_usart3_putbuf;
	_serial_usart3_param.fischar = 0;		// Not available as read only
	_serial_usart3_param.dma1_or_int0 = 0;
	_serial_usart3_param.dma1_or_int0_tx = 0;

	if(_serial_uasrt3_file==0)
	{
		// First time called the file is created.
		// Open file
		cookie_io_functions_t iof;

		iof.read = 0;
		iof.write = &serial_usart3_cookiewrite_int;
		iof.close = 0;
		iof.seek = 0;
		_serial_uasrt3_file = fopencookie((void*)&_serial_usart3_param,"w",iof);

		//itmprintf("fopencookie: %p\n",f);

		// Buffering
		setvbuf (_serial_uasrt3_file, 0, _IONBF, 0 );	// No buffering
		//setvbuf (_serial_uasrt3_file, 0, _IOLBF, 64);	// Line buffer buffering

		// Or: big hack - _cookie seems unused in this libc - this is not portable.
		_serial_uasrt3_file->_cookie = &_serial_usart3_param;
	}
	else
	{
		// Already called previously

		// In order to avoid memory allocation/deallocation, keep the previously allocated FILE.
		// The internal CIRBUF is cleared. However, stdio FILE internally buffers data, and this
		// input data buffer must be flushed.


		fflush(_serial_uasrt3_file);			//	Flush the file to clear output buffer


		fprintf(file_pri,"Warning: softuart already open. Reusing FILE.\n");
	}

	// Initialise the TX buffer and lcear.
	buffer_init(&_serial_usart3_param.txbuf,_serial_uasrt3_tx_buffer,SERIAL_UART_TX_BUFFERSIZE);


	// Peripheral clock enable
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_USART3);
	LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOC);

	/**USART3 GPIO Configuration
	PC4   ------> USART3_TX
	*/
	// Set TX to alternate mode
	GPIO_InitStruct.Pin = LL_GPIO_PIN_4;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_ALTERNATE;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	GPIO_InitStruct.Alternate = LL_GPIO_AF_7;
	LL_GPIO_Init(GPIOC, &GPIO_InitStruct);


	// USART configuration
	USART_InitStruct.BaudRate = baudrate;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = stopbit;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX;
	// No flow control
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

	ErrorStatus status = LL_USART_Init(USART3, &USART_InitStruct);
	if(status == ERROR)
		fprintf(file_usb,"Error init USART 3\n");

	LL_USART_ConfigAsyncMode(USART3);


	//LL_USART_EnableIT_TXE(USART3);			// No TX DMA -> enable TXE interrupt

	// USART interrupt activated anyways
	NVIC_SetPriority(USART3_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
	NVIC_EnableIRQ(USART3_IRQn);



	// Enable USART
	LL_USART_Enable(USART3);

	// Enable interrupt
	// RX, TX interrupt enable
	USART3->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable




	unsigned brr = USART3->BRR;
	unsigned cr1 = USART3->CR1;
	unsigned over8 = (cr1>>15)&1;

	// USART 3 on APB1
	// Get APB1 frequency
	unsigned apb1frq = stm_rcc_get_apb1_frq();
	fprintf(file_pri,"BRR: %u\n",brr);
	fprintf(file_pri,"OVER8: %u\n",over8);
	fprintf(file_pri,"Effective baudrate: %u. Desired baudrate: %d\n",apb1frq/brr,baudrate);

	return _serial_uasrt3_file;
}
void serial_usart3_deinit()
{
	LL_USART_Disable(USART3);
}
void serial_usart3_send(unsigned char ch)
{



	while( !((USART3->ISR)& USART_FLAG_TXE) );		// Wait until TXE is set

	// Write data
	USART3->TDR = ch;

}




/******************************************************************************
*******************************************************************************
INTERRUPT DRIVEN --- INTERRUPT DRIVEN --- INTERRUPT DRIVEN --- INTERRUPT DRIVEN
*******************************************************************************
******************************************************************************/

/******************************************************************************
	function: serial_usart3_irq
*******************************************************************************
	IRQ handler for UART.

	Note: called from USART2_IRQHandler in Src/stm32l4xx_it.c

	h:				base address of uart

	Return value:	-
******************************************************************************/
void serial_usart3_irq(USART_TypeDef *h)
{
#if 0
	Serial1Int++;

	unsigned sr = h->ISR;			// Get SR once

	//if(_serial_usart_debug_irq_enabled)
		//itmprintf("I %X\n",sr);


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
		//itmprintf("CTS\n");
		//fprintf(file_pri,"CTS\n");
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
	sr = sr & 0b1100101111;
	if(sr)
	{
		//itmprintf("I %08X\n",sr);
		Serial1EvtWithinInt++;
	}
#endif
}

unsigned char _byte_to_byte7e(unsigned char b)
{
	// Converts the byte to even parity, modifying the bit 0.
	b&=0xfe;	// Mask bit 0
	unsigned char p=0;
	for(unsigned i=1;i<8;i++)	// Count number of 1
	{
		if(b&(1<<i))
			p++;
	}
	b=b|(p&1);	// Set parity bit
	return b;
}

char str[]="THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG\n";
int strl=44;
void USART3_IRQHandler(void)
{
	static char c=0;

	unsigned sr = USART3->ISR;			// Get SR once

	//fprintf(file_pri,"it %02X %d\n",sr,(unsigned)c);

#if 0
	if( sr & USART_FLAG_TXE)
	{

		if(1)
		{
			USART3->TDR = c;				// Send incrementint 0, 1, 2, ..., fe, ff, 0
			c++;
		}
		if(0)
		{
			USART3->TDR = str[c];
			c++;
			if(c>=strl)
				c=0;
		}


		//USART3->TDR = 0x00;			// Always send 0.
		//USART3->TDR = 0xff;			// Always send ff.
	}
#endif

	if( (sr & USART_FLAG_TXE) && (USART3->CR1&USART_CR1_TXEIE) )			// Must check for TXEIE so that we do not accumulate TXE counts while no TX was intended.
	{
		//itmprintf("\tTXE\n");

		//Serial1TXE++;

		// Data must be sent to bluetooth anyways as it could be $$$ messages to configure the bluetooth chip
		// A better implementation would distinguish between communication to bluetooth chip or remote.
		/*if(!system_isbtconnected())
		{
			// Not connected - do not push data and deactivate interrupt otherwise we reenter continuously the loop
			h->CR1 &= ~USART_CR1_TXEIE;
			return;
		}*/

		if(!buffer_isempty(&_serial_usart3_param.txbuf))
		{
			unsigned char c = buffer_get(&_serial_usart3_param.txbuf);
			//itmprintf("\tTX: %c\n",c);

			if(_serial_usart3_cksumen)
			{

				// Hack to modify the data to include even parity on bit 0.
				c = _byte_to_byte7e(c);
			}

			USART3->TDR = c;

			//fprintf(file_pri,"U3: %d\n",c);
		}
		// Transmit data or deactivate interrupt
		if(buffer_isempty(&_serial_usart3_param.txbuf))
		{
			// No data to transmit: deactivate interrupt otherwise we reenter continuously the loop
			USART3->CR1 &= ~USART_CR1_TXEIE;
		}
	}
	// Check SR again - it should be zero unless another event occurred during this interrupt
	sr = USART3->ISR;
	// Keep the interesting bits: LBD, TXE, RXNE, ORE, NF, FE, PE
	sr = sr & 0b1100101111;
	if(sr)
	{
		fprintf(file_pri,"usart 3 irq sr: %08x\n",sr);
	}

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
unsigned char serial_usart3_putbuf(SERIALPARAM *sp,char *data,unsigned short n)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(buffer_freespace(&sp->txbuf)<n)
			return 1;
		for(unsigned short i=0;i<n;i++)
			buffer_put(&sp->txbuf,data[i]);
	}

	// Interrupt mode: trigger an interrupt when UDR is empty and if wrote data
	if(n)
		//((USART_TypeDef*)sp->device)->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable
		USART3->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable


	return 0;
}




/******************************************************************************
	serial_uart_cookiewrite_int
*******************************************************************************
	Used with fcookieopen to write a buffer to a stream.

	This function is interrupt driven, i.e. uses a circular buffer.

	Return value:
		number of bytes written (0 if none)
******************************************************************************/
ssize_t serial_usart3_cookiewrite_int(void *__cookie, const char *__buf, size_t __nbytes)
{
	__ssize_t nw=0;
	unsigned c=0;


	//fprintf(file_pri,"cookiewrite: %d\n",__nbytes);

	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;

	// Check if must block until space available
	if(sp->blockingwrite)
	{
		// Block only if not in interrupt (e.g. if fprintf called from interrupt), otherwise buffers will never empty
		if(!is_in_interrupt())
		{
			// Block only if the buffer is large enough to have a chance to hold the data
			if(sp->txbuf.size>__nbytes)
			{
				while(buffer_freespace(&sp->txbuf)<__nbytes)
					__NOP();
			}
		}
	}

	// Write byte by byte
	for(size_t i=0;i<__nbytes;i++)
	{
		// Blocking write
		c=0;
		while(buffer_isfull(&(sp->txbuf)))
		{
			c++;
		}
		//itmprintf("n: %d i: %d c: %d\n",__nbytes,i,c);
		buffer_put(&(sp->txbuf),__buf[i]);
		{
			nw++;

			// Interrupt mode: immediately trigger an interrupt if TXE empty, so that the first byte is pushed out in the next interrupt
			USART3->CR1 |= USART_CR1_TXEIE;		// TX register empty interrupt enable

		}
	}
	return nw;
}
/******************************************************************************
	function: serial_usart3_enablecksum
*******************************************************************************
	Hack to use bit 0 as a parity bit. Even parity. Bit zero is modified
	so that the sum of all ones is even.

	Damages data, but ok for e.g. audio transfer.

	Return value:
		0			-	Success
		nonzero		-	Error
******************************************************************************/
void serial_usart3_enablecksum(unsigned en)
{
	_serial_usart3_cksumen=en;
}
