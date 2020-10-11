/*
 * stmrcc.c
 *
 *  Created on: 11 oct. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include "global.h"
#include "stm32l4xx_hal.h"


/******************************************************************************
	function: stm_rcc_get_apb2_timfrq
*******************************************************************************
	Returns the clock frequency of timers connected to APB2 bus.

	The STM32L4 timers are as follows:

	Advanced control timers:
		TIM1		APB2
		TIM8		APB2
	General purpose timers (16 or 32 bit auto-reload):
		TIM2		APB1
		TIM3		APB1
		TIM4		APB1
		TIM5		APB1
	General purpose timers (16 bit auto-reload):
		TIM15		APB2
		TIM16		APB2
		TIM17		APB2
	Basic timers:
		TIM6		APB1
		TIM7		APB1
	Low-power timers:
		LPTIM1		APB1
		LPTIM2		APB1

	Parameters:
		-

	Returns:
		Timer clock frequency
******************************************************************************/
unsigned stm_rcc_get_apb2_timfrq()
{
	unsigned timclk;
	unsigned pclk = HAL_RCC_GetPCLK2Freq();
	// Get the APB2 timer clock frequency: it is equal to APB2 only if the APB2 prescaler is 1, otherwise it is double.
	// Get PCLK2 prescaler
	if((RCC->CFGR & RCC_CFGR_PPRE2) == 0)
	{
		// PCLK2 prescaler equal to 1 => TIMCLK = PCLK2
		timclk = pclk;
	}
	else
	{
		// PCLK prescaler different from 1 => TIMCLK = 2 * PCLK
		timclk = 2*pclk;
	}
	fprintf(file_pri,"APB2 peripheral clock: %u APB2 timer clock: %u\n",pclk,timclk);
	return timclk;
}

/******************************************************************************
	function: stm_rcc_get_apb1_timfrq
*******************************************************************************
	Returns the clock frequency of timers connected to APB1 bus.

	The STM32L4 timers are as follows:

	Advanced control timers:
		TIM1		APB2
		TIM8		APB2
	General purpose timers (16 or 32 bit auto-reload):
		TIM2		APB1
		TIM3		APB1
		TIM4		APB1
		TIM5		APB1
	General purpose timers (16 bit auto-reload):
		TIM15		APB2
		TIM16		APB2
		TIM17		APB2
	Basic timers:
		TIM6		APB1
		TIM7		APB1
	Low-power timers:
		LPTIM1		APB1
		LPTIM2		APB1

	Parameters:
		-

	Returns:
		Timer clock frequency
******************************************************************************/
unsigned stm_rcc_get_apb1_timfrq()
{
	unsigned timclk;
	unsigned pclk = HAL_RCC_GetPCLK1Freq();
	// Get the APB1 timer clock frequency: it is equal to APB1 only if the APB1 prescaler is 1, otherwise it is double.
	// Get PCLK1 prescaler
	if((RCC->CFGR & RCC_CFGR_PPRE1) == 0)
	{
		// PCLK2 prescaler equal to 1 => TIMCLK = PCLK1
		timclk = pclk;
	}
	else
	{
		// PCLK prescaler different from 1 => TIMCLK = 2 * PCLK
		timclk = 2*pclk;
	}
	fprintf(file_pri,"APB1 peripheral clock: %u APB1 timer clock: %u\n",pclk,timclk);
	return timclk;
}

