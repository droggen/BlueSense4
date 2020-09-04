/*
 * global.c
 *
 *  Created on: 16 Sep 2019
 *      Author: droggen
 */

#include <stdio.h>
#include "stm32l4xx.h"




FILE *file_usb,*file_pri,*file_bt,*file_itm,*file_dbg;


unsigned char sharedbuffer[520];

unsigned char __mode_sleep=1;

// Device name
unsigned char system_devname[16];

unsigned char system_mode;

unsigned char is_in_interrupt()
{
	if (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
		return 1;
	return 0;
}
