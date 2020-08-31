/*
 * dsystick.c
 *
 *  Created on: 17 Sep 2019
 *      Author: droggen
 */
#include "main.h"
#include "wait.h"
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.ddi0413c/Bhccjgga.html
// core_cm4.h: SysTick_Config

/*
 Emulate the ST HAL systick handler
 */
void SysTick_Handler()
{
	HAL_IncTick();			// To emulate the ST HAL systick handler

	_timer_tick_1000hz();	// megalol systick handler
}

#if 0
void systick_cb_1hz()
{
	_timer_tick_hz();
}
#endif

unsigned char systick_cb_50hz(unsigned char t)
{
	(void)t;
	_timer_tick_50hz();
	return 0;
}
#if 0
void dsystick_init()
{
	// Reset epoch
	//timer_init(0,0,0);

	// Init all the subsequent callbacks based on the KHz one
	//timer_register_callback(systick_cb_1hz,999);
	timer_register_callback(systick_cb_50hz,19);
}
#endif
void dsystick_clear()
{
	// Writing any value to VAL clears it to zero
	SysTick->VAL=SysTick->LOAD;			// In effect: all writes clear to zero
}
unsigned long dsystick_getus()
{
	// Assuming VAL is 1ms

	return (SysTick->LOAD-SysTick->VAL)*1000l/SysTick->LOAD;
}



