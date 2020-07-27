/*
   MEGALOL - ATmega LOw level Library
	ADC Module
   Copyright (C) 2009-2016:
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
#ifndef __WAIT_H
#define __WAIT_H

#include "stdio.h"

// Maximum number of user callbacks
#define TIMER_NUMCALLBACKS 6

// Define which timer to use - only TCNT and TIFR1 are used which must be defined to TCNT1 or TCNT3
//#define WAIT_TCNT TCNT1
//#define WAIT_TIFR TIFR1
#define WAIT_TCNT TCNT3
#define WAIT_TIFR TIFR3



extern unsigned char timer_numcallbacks;
// Callback data structure
typedef struct 
{
	unsigned char (*callback)(unsigned char);		// User callback to call
	volatile unsigned short counter;				// Current counter to define call periodicity
	unsigned short top;								// Top value defining the call periodicity. top=0
} TIMER_CALLBACK;



// Select which implementation of timer_ms_get to use timer_ms_get_c, timer_ms_get_asm, timer_ms_get_asm_fast. Note: assembler implementations are obsolete; C implementation faster due to pre-computing in uS and mS
//#define timer_ms_get timer_ms_get_asm_fast
#define timer_ms_get timer_ms_get_c
//#define timer_us_get timer_us_get_asm_fast
#define timer_us_get timer_us_get_c

/*
	Interrupt-driven timer functions 
*/
extern volatile unsigned long _timer_1hztimer_in_ms;							// We count time from the 1Hz clock in milliseconds to speedup timer_ms_get. 32-bit: max 49 days
//extern volatile unsigned long _timer_time_1024;
//extern volatile unsigned long _timer_time_1000;
extern volatile unsigned long _timer_time_ms_intclk;
//extern volatile unsigned long _timer_lastmillisec;							// 32-bit: max 49 days
extern volatile unsigned long long _timer_lastreturned_microsec;

void timer_init(unsigned long epoch_s,unsigned long epoch_s_frommidnight,unsigned long epoch_us);
void timer_init_us(unsigned long epoch_us);
//unsigned long timer_ms_get_c(void);
unsigned long timer_ms_get_c(void);
unsigned long timer_ms_get_asm(void);
#ifdef __cplusplus
extern "C" unsigned long timer_ms_get_asm_fast(void);
extern "C" unsigned long int timer_us_get_asm_fast(void);
#else
unsigned long timer_ms_get_asm_fast(void);
unsigned long int timer_us_get_asm_fast(void);
#endif
unsigned long int timer_us_get_c(void);
unsigned long timer_ms_get_intclk(void);

unsigned long timer_s_wait(void);
unsigned long timer_s_get(void);
unsigned long timer_s_get_frommidnight(void);

// Call this function from an interrupt routine every herz, if available, e.g. from a RTC
void _timer_tick_hz(void);
// Call this function from an interrupt routine every 1/1024 hz (_timer_tick_1024hz or _timer_tick_1000hz are mutually exclusive: call one or the other)
void _timer_tick_1024hz(void);
// Call this function from an interrupt routine every 1/1000 hz (_timer_tick_1024hz or _timer_tick_1000hz are mutually exclusive: call one or the other)
void _timer_tick_1000hz(void);
void _timer_tick_50hz(void);

//typedef unsigned long int WAITPERIOD;					// This must be matched to the size of the return value of timer_?s_get
typedef unsigned WAITPERIOD;					// This must be matched to the size of the return value of timer_?s_get

char timer_register_callback(unsigned char (*callback)(unsigned char),unsigned short divider);
char timer_register_slowcallback(unsigned char (*callback)(unsigned char),unsigned short divider);
char timer_register_50hzcallback(unsigned char (*callback)(unsigned char),unsigned short divider);
unsigned char timer_isregistered_callback(unsigned char (*callback)(unsigned char));
void timer_unregister_callback(unsigned char (*callback)(unsigned char));
void timer_unregister_slowcallback(unsigned char (*callback)(unsigned char));
void timer_unregister_50hzcallback(unsigned char (*callback)(unsigned char));
void timer_printcallbacks(FILE *f);


unsigned long timer_waitperiod_ms(unsigned short p,WAITPERIOD *wp);
//unsigned long timer_waitperiod_us(unsigned long p,WAITPERIOD *wp);

//void timer_waitperiod_test(void);
//void timer_get_speedtest(void);

#endif


