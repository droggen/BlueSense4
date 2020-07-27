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

void BS_GPIO_Init();
void usrmain();



#ifdef __cplusplus
}
#endif

#endif /* USRMAIN_H_ */
