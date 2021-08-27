/*
 * usrmain.h
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */

#ifndef USRMAIN_H_
#define USRMAIN_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "stdio.h"

extern FILE *file_itm;

#define _delay_ms HAL_Delay

#define SYSTEM_CLOCK_APB1_MHZ 20
#define SYSTEM_CLOCK_APB1_HZ (SYSTEM_CLOCK_APB1_MHZ*1000000)


#ifndef SYSTEM_CLOCK_FREQUENCY
#error SYSTEM_CLOCK_FREQUENCY must be defined
#endif
#if !((SYSTEM_CLOCK_FREQUENCY==32) || (SYSTEM_CLOCK_FREQUENCY==80))
#error Invalid SYSTEM_CLOCK_FREQUENCY: must be defined to 32 or 80
#endif


void BS_GPIO_Init();
void usrmain();




#ifdef __cplusplus
}
#endif

#endif /* USRMAIN_H_ */
