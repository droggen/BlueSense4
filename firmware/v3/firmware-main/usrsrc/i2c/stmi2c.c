/*
   MEGALOL - ATmega LOw level Library
	I2C Module
   Copyright (C) 2010-2015:
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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "i2c.h"
//#include "i2c_internal.h"
#ifdef ENABLE_I2CINTERRUPT
//#include "i2c_int.h"
#endif
#ifdef ENABLE_I2CPOLL
#include "i2c_poll.h"
#endif
#include "main.h"
#include "usrmain.h"
#include "global.h"
#include "stm32l4xx_hal_rcc.h"
//#include "rcc.h"
/*
	File: i2c
	
	I2C functions
	
*/


// An I2C bus can also be reset with the magical sequence explained in multiple places : start, clock 9 bits, start, stop.


/******************************************************************************
*******************************************************************************
GENERAL & WRAPPER
*******************************************************************************
******************************************************************************/

/*

CubeMX initialises as follows:
- 400KHz: fm duty=16/9
- 100KHz: sm duty=2


PCLK=20MHz FAST 400KHz (default from CubeMX)
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 0000C002 (49154d)
	TRISE: 00000007
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000

PCLK=20MHz Normal 1KHz (default from CubeMX)
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 00000710 (1808d)
	TRISE: 00000015
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000

PCLK=20MHz SM 100KHz (default from CubeMX)
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 00000064 (100d)
	TRISE: 00000015
	FLTR: 00000000

PCLK=20MHz FM 200KHz (default from CubeMX
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 00008022 (32802d)
	TRISE: 00000007
	FLTR: 00000000


Init 400000Hz dummy: 0... OK
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 00008011
	CCR: 32785
	TRISE: 00000007
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000

Init 400000Hz dummy: 1... OK
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 0000C002
	CCR: 49154
	TRISE: 00000007
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000



Init 100000Hz dummy: 0... OK
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 00000064
	CCR: 100
	TRISE: 00000015
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000

Init 100000Hz dummy: 1... OK
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 00000064
	CCR: 100
	TRISE: 00000015
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000

Init 10000Hz dummy: 0... OK
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 000003E8
	CCR: 1000
	TRISE: 00000015
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000
CMDOK

Init 10000Hz dummy: 1... OK
I2C registers:
	CR1: 00000001
	CR2: 00000014
	OAR1: 00004000
	OAR2: 00000000
	CCR: 000003E8
	CCR: 1000
	TRISE: 00000015
	FLTR: 00000000
HCLK: 20000000
PCLK1: 20000000
PCLK2: 20000000





SM: Tclk=20ns @ 20MHz
	thigh = CCR*Tclk = 100*20 = 2uS
	tlow = CCR*Tclk = 100*20 = 2uS -> 250KHz

FM Duty2: Tclk=20ns @ 20MHz
	thigh = CCR*Tclk = 2uS
	tlow = 2*CCR*Tclk = 4uS -> 6uS = 166KHz


 */
void i2c_printconfig(void)
{
#if 0
	// STM32F401 codepath
	fprintf(file_pri,"I2C registers:\n");
	fprintf(file_pri,"\tCR1: %08X\n",I2C1->CR1);
	fprintf(file_pri,"\tCR2: %08X\n",I2C1->CR2);
	fprintf(file_pri,"\tSR1: %08X\n",I2C1->SR1);
	fprintf(file_pri,"\tSR2: %08X\n",I2C1->SR2);
	fprintf(file_pri,"\tOAR1: %08X\n",I2C1->OAR1);
	fprintf(file_pri,"\tOAR2: %08X\n",I2C1->OAR2);
	fprintf(file_pri,"\tCCR: %08X (%ud)\n",I2C1->CCR,I2C1->CCR);
	fprintf(file_pri,"\tTRISE: %08X\n",I2C1->TRISE);
	fprintf(file_pri,"\tFLTR: %08X\n",I2C1->FLTR);
	fprintf(file_pri,"HCLK: %d\n",HAL_RCC_GetHCLKFreq());
	fprintf(file_pri,"PCLK1: %d\n",HAL_RCC_GetPCLK1Freq());
	fprintf(file_pri,"PCLK2: %d\n",HAL_RCC_GetPCLK2Freq());
#endif
#if 1
	// STM32L4 codepath
	fprintf(file_pri,"I2C registers:\n");
	fprintf(file_pri,"\tCR1: %08X\n",(unsigned int)I2C1->CR1);
	fprintf(file_pri,"\tCR2: %08X\n",(unsigned int)I2C1->CR2);
	fprintf(file_pri,"\tOAR1: %08X\n",(unsigned int)I2C1->OAR1);
	fprintf(file_pri,"\tOAR2: %08X\n",(unsigned int)I2C1->OAR2);
	fprintf(file_pri,"\tTIMINGR: %08X\n",(unsigned int)I2C1->TIMINGR);
	fprintf(file_pri,"\tTIMEOUTR: %08X\n",(unsigned int)I2C1->TIMEOUTR);
	fprintf(file_pri,"\tISR: %08X\n",(unsigned int)I2C1->ISR);
	fprintf(file_pri,"\tPECR: %08X\n",(unsigned int)I2C1->PECR);
	fprintf(file_pri,"\tRXDR: %08X\n",(unsigned int)I2C1->RXDR);
	fprintf(file_pri,"\tTXDR: %08X\n",(unsigned int)I2C1->TXDR);
	fprintf(file_pri,"Bus speed:\n");
	fprintf(file_pri,"\tHCLK: %u\n",HAL_RCC_GetHCLKFreq());
	fprintf(file_pri,"\tPCLK1: %u\n",HAL_RCC_GetPCLK1Freq());
	fprintf(file_pri,"\tPCLK2: %u\n",HAL_RCC_GetPCLK2Freq());
#endif
}

void i2c_init(void)
{
	i2c_transaction_init();

	// Initialisation on ARM
	/*I2C1->CR1=0x00000000;	// Default reset value - control register - peripheral deactivated
	I2C1->CR2=0x00000000;	// Default reset value - control register
	I2C1->OAR1=0x00000000;	// Default reset value - own address
	I2C1->OAR2=0x00000000;	// Default reset value - own address
	I2C1->CCR=0x00000000;	// Default reset value - clock control register
	I2C1->TRISE=0x00000000;
	I2C1->FLTR=0x00000000;*/

	// 20 MHz -> 10KHz
	/*I2C1->CR2=0x14;
	I2C1->OAR1=0x4000;
	I2C1->OAR2=0x0;
	I2C1->CCR=0x3e8;		// sm, duty=2, ccr=1000d: t=th+tl, th=ccr*tpclk, tl=ccr*tpclk, t=2*ccr*plk = 2*50ns*1000 = 10khz
	I2C1->TRISE=0x15;
	I2C1->FLTR=0x00;*/

	// 20 MHz -> 1KHz
	/*I2C1->CR2=0x00000014;			// dma dis, itbufen dis, iteven dis, iterren dis, freq=20d: APB=20MHz
	I2C1->OAR1=0x00004000;			// 0100 0000 0000 0000: bit 14 always 1, 7-bit, address 0
	I2C1->OAR2=0x00000000;			// dual addressing disabled
	I2C1->CCR=0x00000710;			// sm, duty=2, ccr=1808d: t=th+tl, th=ccr*tpclk, tl=ccr*tpclk, t=2*ccr*plk = 2*50ns*1808 = 5531Hz
	I2C1->TRISE=0x00000015;
	I2C1->FLTR=0x00000000;*/

	// 20MHz -> 400KHz
	/*I2C1->CR2=0x00000014;
	I2C1->OAR1=0x00004000;
	I2C1->OAR2=0x00000000;
	I2C1->CCR=0x0000C002;// (49154d)
	I2C1->TRISE=0x00000007;
	I2C1->FLTR=0x00000000;*/

	// 20MHz -> 100KHz
	/*I2C1->CR2=0x00000014;
	I2C1->OAR1=0x00004000;
	I2C1->OAR2=0x00000000;
	I2C1->CCR=0x00000064; // (100d)
	I2C1->TRISE=0x00000015;
	I2C1->FLTR=0x00000000;*/

	// 20MHz -> 100KHz
	/*I2C1->CR2=0x00000014;
	I2C1->OAR1=0x00004000;
	I2C1->OAR2=0x00000000;
	I2C1->CCR=0x00008022;
	I2C1->TRISE=0x0000007;
	I2C1->FLTR=0x00000000;

	I2C1->CR1=0x1;			// I2C active*/

	// I2C1 interrupt Init
	HAL_NVIC_SetPriority(I2C1_EV_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
	HAL_NVIC_SetPriority(I2C1_ER_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);


}

void i2c_deactivateinterrupts()
{
	I2C1->CR2=I2C1->CR2&0b1111100011111111;
}


void i2c_deinit(void)
{
#warning "FIX: IMPLEMENT disable"

}

