/*
 * dsystick.h
 *
 *  Created on: 17 Sep 2019
 *      Author: droggen
 */

#ifndef DSYSTICK_H_
#define DSYSTICK_H_

void SysTick_Handler();

void dsystick_init();
void dsystick_clear();
unsigned long dsystick_getus();

unsigned char systick_cb_50hz(unsigned char);

#endif /* DSYSTICK_H_ */
