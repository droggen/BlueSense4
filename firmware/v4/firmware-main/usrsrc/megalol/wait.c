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

/*
	file: wait
	
	This library provides functions related to timing, waiting, and issuing periodic callbacks.
	
	This library requires some of these functions to be called from a timer interrupt to update the internal logic:
	
	_timer_tick_1024hz	-	This function must be called from an interrupt running at 1024Hz. _timer_tick_1024hz or _timer_tick_1000hz are mutually exclusive: it is mandatory to call one or the other, but not both.
	_timer_tick_1000hz	-	This function must be called from an interrupt running at 1000Hz. _timer_tick_1024hz or _timer_tick_1000hz are mutually exclusive: it is mandatory to call one or the other, but not both.
	_timer_tick_hz		-	Optionally, this may be called once per second if a high-accuracy RTC with second resolution is available.
	_timer_tick_50Hz	-	Optionally, this may be called at 50Hz; this is solely used for the "50Hz" callbacks.
		
	The overall time is composed of the highly accurate second counter (if available), combined with the less 1024Hz timer for millisecond accuracy.
	
	This library allows to register user callbacks to be called at regular interval from the timer interrupts with a specified clock divider.
	The callbacks are divided in millisecond-accuracy callbacks (functions timer_register_callback and timer_unregister_callback) and second-accuracy
	callbacks (functions timer_register_slowcallback and timer_unregister_slowcallback).
	The slow callbacks are only available if the 1Hz tick is called (_timer_tick_hz). Slow callbacks are lighter on processor usage and should be preferred
	for task to be executed with a longer time interval which does not need millisecond accuracy.
	
	The functions timer_waitperiod_ms and timer_waitperiod_us allow to wait until a period of time or a multiple of the period has elapsed from the previous call.
	This can be used to schedule taks which must land on a periodic grid, such as sampling.
	
	
	The key functions are:
	
	* timer_init:						Initialise the timer subsystem with a given epoch.
	* timer_ms_get:						Return the time in millisecond since the epoch; time is composed of the 1Hz high accuracy clock (if available) and the internal 1000Hz or 1024Hz clock.
	* timer_ms_get_intclk:				Return the time in millisecond since the epoch; time is composed only of the internal 1000Hz or 1024Hz clock.
	* timer_us_get: 					Return the time in microsecond since the epoch.
	* timer_register_callback:			Register a callback that will be called at 1024Hz/(divider+1) Hz
	* timer_register_slowcallback:		Register a callback that will be called at 1Hz/(divider+1) Hz
	* timer_unregister_callback: 		Unregisters a callback
	* timer_unregister_slowcallback:	Unregisters a slow callback
	* timer_printcallbacks:				Prints the list of registered callbacks
	* timer_waitperiod_ms:				Wait until a a period of time - or a multiple of it - has elapsed from the previous call.
	* timer_waitperiod_us:				Wait untila a period of time - or a multiple of it - has elapsed from the previous call.
	
	*Callbacks*
	
	There are two types of callbacks: "normal" occuring at 1KHz divided by a user-specific factor and "slow" occurring at 1Hz divided by a user-specific factor.
	
	Slow callbacks are only available if _timer_tick_hz is called at 1Hz, otherwise use the normal callbacks. However, for infrequent tasks, slow callbacks
	are lighter on system resources.
	
	The callbacks must be of the form
		unsigned char callback(unsigned char)
	
	The return value of the callback is currently not used, but it is recommended to return 0 (future versions of the API could use the return value
	to deregister a callback).
	
	The parameter passed to "normal" callbacks is always 0. The parameter passed to the "slow" callbacks is the number of seconds since the epoch, i.e.
	the number of times _timer_tick_hz has been called. This can be used to synchronise tasks based on the number of seconds elapsed since the epoch.
	
	*Implementation rationale*
	
	The time in milliseconds is computed and updated upon each callback call. This slightly increases the time to execute the callback, but the benefits are:
	- a much faster timer_ms_get function which only needs to return the pre-computed time; this is especially beneficial if timestamps are required in interrupt routines or for delays.
	- a more consistent use of CPU resources in a real-time implementation.
	
	*TODO*
	
	The assembler version of time in microsecond uses a previous implementation of the millisecond time which was not precomputed. 
	This timer_us_get_asm_fast should be updated to rely on the pre-computed milliseconds; if this is done, the following variable and their updates can be eliminated: _timer_1hztimer_in_ms, _timer_1hztimer_in_s, _timer_time_1024.
	
	
*/

//#define ATOMIC_BLOCK(ATOMIC_RESTORESTATE)


#include "wait.h"
#include <stdio.h>
#include "dsystick.h"
#include "atomicop.h"


/******************************************************************************
	Global variables
*******************************************************************************
	_timer_1hztimer_in_ms: 	second counter, stored in millisecond to speedup time read
	_timer_time_1024: 1/1024s counter, to be reset at each second if an RTC is 
										available
										This counter should not be higher than 1hr, otherwise the
										conversion to time will fail. This is not an issue if a
										1Hz timer is available. 
	_timer_time_1000: time in 1/1000s; used for a faster version of the code
	_timer_time_ms:				Current time in milliseconds, guaranteed to be monotonic; this is updated every time a _timer_tick_1024hz or _timer_tick_hz callback is called. This is the time returned by timer_ms_get
	_timer_time_ms_lastsec:		Time in milliseconds the last time _timer_tick_hz was called. This is used internally to compute _timer_time_ms.
	
******************************************************************************/

volatile unsigned long _timer_1hztimer_in_s=0;					// Incremented by 1s at each 1Hz timer callback.
volatile unsigned long _timer_1hztimer_in_s_frommidnight=0;		// Incremented by 1s at each 1Hz timer callback (initialised using the _frommidnight time base).
volatile unsigned long _timer_1hztimer_in_ms=0;					// Incremented by 1000ms at each 1Hz timer callback
volatile unsigned long _timer_1hztimer_in_us=0;					// Incremented by 1000000us at each 1Hz timer callback
volatile unsigned long _timer_time_ms_intclk=0;				// Increments by 1 every millisecond and never reset by 

volatile unsigned long _timer_time_ms=0;					// Current time in milliseconds; initialised by the 1Hz callback to _timer_1hztimer_in_ms and incremented by the internal clock
volatile unsigned long _timer_time_us=0;					// Current time in microseconds; initialised by the 1Hz callback to _timer_1hztimer_in_us and incremented by the internal clock
volatile unsigned long _timer_time_ms_monotonic=0;			// Current time in milliseconds; initialised by the 1Hz callback to _timer_1hztimer_in_ms and incremented by the internal clock, guaranteed to be monotonic
volatile unsigned long _timer_time_us_monotonic=0;			// Current time in microseconds; initialised by the 1Hz callback to _timer_1hztimer_in_us and incremented by the internal clock, guaranteed to be monotonic
volatile unsigned long _timer_time_us_lastreturned=0;		// Last returned microseconds; combination of _timer_time_us_monotonic and timer counter; used to ensure monotonic time in the call to timer_us_get

// State
unsigned char _timer_time_1024to1000_divider=0;				// This variable is used by _timer_tick_1024hz to generate a 1000Hz update from a 1024Hz clock and to approximate the 976.5625uS increment of the uS counter
unsigned char _timer_time_1hzupdatectr=0;					// Used to indicate when to correct the internal timer 

// Timer callbacks: fixed-number of callbacks
unsigned char timer_numcallbacks=0;
TIMER_CALLBACK timer_callbacks[TIMER_NUMCALLBACKS];
unsigned char timer_numslowcallbacks=0;
TIMER_CALLBACK timer_slowcallbacks[TIMER_NUMCALLBACKS];
unsigned char timer_num50hzcallbacks=0;
TIMER_CALLBACK timer_50hzcallbacks[TIMER_NUMCALLBACKS];


/******************************************************************************
	function: timer_init
*******************************************************************************
	Initialise the timer subsystem with a given epoch.
	
	The epoch is optional: set it to zero if the system does not have a 
	battery-backed RTC.
	
	If the system has an absolute time reference (e.g. a 
	battery backed RTC) use the RTC time as the epoch, 
	otherwise set to zero.
	If an RTC is used, this function must be called exactly upon a second change.

	Parameters:
		epoch_s 	-	Time in seconds from an arbitrary "zero time". 
						Used by the milliseconds timers (timer_ms_get).
		epoch_s_frommidnight -	Time in seconds from an arbitrary "zero time" at midnight (e.g. current day). 
						(unused)
						
		epoch_us	-	Time in microseconds from an arbitrary "zero time".
						Used by the microseconds timers (timer_us_get).
						
******************************************************************************/
void timer_init(unsigned long epoch_s,unsigned long epoch_s_frommidnight,unsigned long epoch_us)
{
	// Ensures an atomic change

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Initialise the variables holding the time updated at the 1Hz tick
		_timer_1hztimer_in_s=epoch_s;
		_timer_1hztimer_in_s_frommidnight=epoch_s_frommidnight;
		_timer_1hztimer_in_ms=epoch_s*1000l;
		_timer_1hztimer_in_us=epoch_us;
		
		// Initialise the variable holding the time updated solely from the internal timer
		_timer_time_ms_intclk=_timer_1hztimer_in_ms;
		
		// Store current time and current monotonic time
		_timer_time_ms_monotonic=_timer_time_ms=_timer_1hztimer_in_ms;
		_timer_time_us_lastreturned=_timer_time_us_monotonic=_timer_time_us=_timer_1hztimer_in_us;
				
		// Reset the dividers 1024 to 1000 and uS dividers
		_timer_time_1024to1000_divider=0;
		
		// Reset the 1hz correction counter
		_timer_time_1hzupdatectr=0;
		
		
		// Clear counter and interrupt flags
		//ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			// Reset callback counters
			for(unsigned char i=0;i<timer_numslowcallbacks;i++)
			{
				timer_slowcallbacks[i].counter=0;
			}
			for(unsigned char i=0;i<timer_numcallbacks;i++)
			{
				timer_callbacks[i].counter=0;
			}
			// TOFIX ARM
			dsystick_clear();
			//WAIT_TCNT=0;				// Clear counter, and in case the timer generated an interrupt during this initialisation process clear the interrupt flag manually
			//WAIT_TIFR=0b00100111;		// Clear all interrupt flags
		}
	}
}

// Hack to set the uS epoch only.
// This is a serious hack: there is no guarantee that timer_us_get will return the epoch_us if immediately called afterwards, as 
// some internal variables shared with by the ms and us logic are not reset.
void timer_init_us(unsigned long epoch_us)
{
	// Ensures an atomic change
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Initialise the variables holding the time updated at the 1Hz tick
		_timer_1hztimer_in_us=epoch_us;
				
		// Store current time and current monotonic time
		_timer_time_us_lastreturned=_timer_time_us_monotonic=_timer_time_us=_timer_1hztimer_in_us;
	}
}


/******************************************************************************
	function: _tick_hz
*******************************************************************************
	This function may be called from a RTC interrupt at 1Hz (optional).
	
	The underlying assumption is that this RTC 1Hz timer is more accurate than the 
	1024Hz timer derived from the processor clock.
	The overall time is composed of the highly accurate second counter, combined
	with the less 1024Hz timer for millisecond accuracy.
******************************************************************************/
void _timer_tick_hz(void)
{
	// Increment the time updated on the 1Hz tick
	_timer_1hztimer_in_s++;
	_timer_1hztimer_in_s_frommidnight++;
	
	// Increment the 1hz update correction counter.
	_timer_time_1hzupdatectr++;

	//return;	// Debug: never do correction
#if 0
	// Updating the internal time every 5s leads to <estimate ppm error>
	const unsigned updateperiod = 5;
	if(_timer_time_1hzupdatectr>=updateperiod)
	//if(_timer_time_1hzupdatectr>=1)
	{
		_timer_time_1hzupdatectr=0;
		
		// Correct the systick
		dsystick_clear();
	
		// Must adjust accordingly
		_timer_1hztimer_in_ms+=updateperiod*1000;
		_timer_1hztimer_in_us+=updateperiod*1000000l;

		// Update the current time. Note that _timer_time_ms and _timer_time_us can jump back or forward in time if the internal clock is respectively too fast or too slow.
		_timer_time_ms=_timer_1hztimer_in_ms;	
		_timer_time_us=_timer_1hztimer_in_us;
		
		// Pre-compute the monotonic time
		// If the current time is higher than the monotonic, update the monotonic to current time. Otherwise do nothing, and eventually the current time will be higher than the monotonic.
		if(_timer_time_ms>_timer_time_ms_monotonic)			
		{
			_timer_time_ms_monotonic=_timer_time_ms;
		}
		if(_timer_time_us>_timer_time_us_monotonic)
		{
			_timer_time_us_monotonic=_timer_time_us;
		}



		// Reset the dividers
		_timer_time_1024to1000_divider=0;
	}
#endif

	// Process the callbacks
	for(unsigned char i=0;i<timer_numslowcallbacks;i++)
	{
		timer_slowcallbacks[i].counter++;
		if(timer_slowcallbacks[i].counter>timer_slowcallbacks[i].top)
		{
			timer_slowcallbacks[i].counter=0;
			timer_slowcallbacks[i].callback(_timer_1hztimer_in_s);
		}		
	}
}



/******************************************************************************
	function: _timer_tick_1024hz
*******************************************************************************
	This function must be called from an interrupt routine every 1/1024 hz.
	
	This may not be a very accurate clock (the 1 Hz clock aims to compensate for that).
	
	This routine also dispatches timer callbacks for downsampled versions of this clock.	
******************************************************************************/
void _timer_tick_1024hz(void)
{
	// _timer_time_us should increment by 976.5625uS
	// Use _timer_time_1024to1000_divider to pad up _timer_time_us to approximate increment by 976.5625uS
	// An unsuitable alternative is to increment by 976uS - this leads to 576uS under-estimation error after 1 second, which is too large
	// An suitable alternative is to increment by 976.5uS - this leads to 64uS under-estimation error after 1 second, which is within reason
	
	// The following does an update by 976.5625uS on average in two steps
	// 1. The following increments on average by 976.5uS. Total error after 1 second: 64uS underestimated
	if(_timer_time_1024to1000_divider&1)
		_timer_time_us+=977;
	else
		_timer_time_us+=976;
	// 2. Add 1uS every 16/1024Hz to achieve exactly 976.5625 on average
	if( (_timer_time_1024to1000_divider&0b1111)==15 )
		_timer_time_us++;
		
	// Pre-compute the monotonic time for uS
	// If the current time is higher than the monotonic, update the monotonic to current time. Otherwise do nothing, and eventually the current time will be higher than the monotonic.
	if(_timer_time_us>_timer_time_us_monotonic)
	{
		_timer_time_us_monotonic=_timer_time_us;
	}
		
	// Do a downsampling from 1024 to 1000 (or 128 to 125) by skipping 3 increments of _timer_time_1000 every 128.
	_timer_time_1024to1000_divider=(_timer_time_1024to1000_divider+1)&0x7f;		// Count from 0-127
	if( _timer_time_1024to1000_divider==42 || _timer_time_1024to1000_divider==84 || _timer_time_1024to1000_divider==126)
		return;	
	
	// This part is called at 1000Hz on average.	

	_timer_time_ms_intclk++;		// Current time using only 1024Hz tick in millisecond
	_timer_time_ms++;				// Current time combining 1Hz tick and 1024Hz tick in millisecond
	
	// Pre-compute the monotonic time for mS
	// If the current time is higher than the monotonic, update the monotonic to current time. Otherwise do nothing, and eventually the current time will be higher than the monotonic.
	if(_timer_time_ms>_timer_time_ms_monotonic)			
	{
		_timer_time_ms_monotonic=_timer_time_ms;
	}
		
	// Process the callbacks
	for(unsigned char i=0;i<timer_numcallbacks;i++)
	{
		timer_callbacks[i].counter++;
		if(timer_callbacks[i].counter>timer_callbacks[i].top)
		{
			timer_callbacks[i].counter=0;
			timer_callbacks[i].callback(0);
		}		
	}	
}
/******************************************************************************
	function: _timer_tick_1000hz
*******************************************************************************
	This function must be called from an interrupt routine every 1/1000 hz.

	This may not be a very accurate clock (the 1 Hz clock aims to compensate for that).

	This routine also dispatches timer callbacks for downsampled versions of this clock.
******************************************************************************/
void _timer_tick_1000hz(void)
{
	// _timer_time_us should increment by 1000uS
	_timer_time_us+=1000;

	// Pre-compute the monotonic time for uS
	// If the current time is higher than the monotonic, update the monotonic to current time. Otherwise do nothing, and eventually the current time will be higher than the monotonic.
	if(_timer_time_us>_timer_time_us_monotonic)
	{
		_timer_time_us_monotonic=_timer_time_us;
	}

	// This part is called at 1000Hz

	_timer_time_ms_intclk++;		// Current time using only 1024Hz tick in millisecond
	_timer_time_ms++;				// Current time combining 1Hz tick and 1024Hz tick in millisecond

	// Pre-compute the monotonic time for mS
	// If the current time is higher than the monotonic, update the monotonic to current time. Otherwise do nothing, and eventually the current time will be higher than the monotonic.
	if(_timer_time_ms>_timer_time_ms_monotonic)
	{
		_timer_time_ms_monotonic=_timer_time_ms;
	}

	// Process the callbacks
	for(unsigned char i=0;i<timer_numcallbacks;i++)
	{
		timer_callbacks[i].counter++;
		if(timer_callbacks[i].counter>timer_callbacks[i].top)
		{
			timer_callbacks[i].counter=0;
			timer_callbacks[i].callback(0);
		}
	}
}
/******************************************************************************
	function: _timer_tick_50hz
*******************************************************************************
	This function can be called from an interrupt routine at 50Hz.
	
	This is solely used to issue the 50Hz callbacks.
	
******************************************************************************/
void _timer_tick_50hz(void)
{
	// Process the callbacks
	for(unsigned char i=0;i<timer_num50hzcallbacks;i++)
	{
		timer_50hzcallbacks[i].counter++;
		if(timer_50hzcallbacks[i].counter>timer_50hzcallbacks[i].top)
		{
			timer_50hzcallbacks[i].counter=0;
			timer_50hzcallbacks[i].callback(0);
		}		
	}
}

/******************************************************************************
	function: timer_ms_get_intclk
*******************************************************************************
	Return the time in millisecond since the epoch using only the internal clock.
	
	Unlike timer_ms_get, which uses both the internal clock and an external 
	high-accuracy 1Hz clock, timer_ms_get_intclk only uses the internal clock.
	
	Use this function for debugging purposes if the 1Hz clock is provided but
	unreliable (e.g. if a RTC time is changed).
	
	The maximum time since the epoch is about 48.5 days.
	The return value is guaranteed to be monotonic until the time counter wraps
	around after 48 days.
	
	Returns:
		Time in milliseconds since the epoch	
******************************************************************************/
unsigned long timer_ms_get_intclk(void)
{	
	unsigned long t;
	
	// Copy current time atomically
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t = _timer_time_ms_intclk;
	}
	return t;
}
/******************************************************************************
	function: timer_ms_get_c
*******************************************************************************
	Return the time in millisecond since the epoch.
	
	This time combines the internal clock and an external high-accuracy 1Hz clock
	to give a high precision time.
	
	The maximum time since the epoch is about 48.5 days.
	The return value is guaranteed to be monotonic until the time counter wraps
	around after 48 days.
	
	Returns:
		Time in milliseconds since the epoch	
******************************************************************************/
unsigned long timer_ms_get_c(void)
{	
	unsigned long t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t=_timer_time_ms_monotonic;
	}
	return t;
}

/******************************************************************************
	timer_us_get_c
*******************************************************************************
	Return the time in microsecond since the epoch.
	
	TODO: this function should return an unsigned long long to allow for 
	epochs older than 1 hour.
	
	This function is guaranteed to be monotonic.
	
	Calls to this function were benchmarked at about 14uS per call.
	
	Returns:
		Time in microseconds since the epoch
	
******************************************************************************/
unsigned long int timer_us_get_c(void)
{
	unsigned long t=0;
	unsigned long tcnt=0;
	
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		tcnt = dsystick_getus();
		t=_timer_time_us_monotonic;		
	}
	
	t+=tcnt;
	
	
	if(t>_timer_time_us_lastreturned)
	{
		_timer_time_us_lastreturned=t;
	}
	return _timer_time_us_lastreturned;				// Guaranteed monotonic
	
}

/******************************************************************************
	function: timer_s_wait
*******************************************************************************
	Waits for a change in the seconds-since-epoch counter and returns the 
	time in seconds since the epoch. 
	
	This function requires the  _timer_tick_hz callback to be called at 1Hz.
	
		
	
	Returns:
		Time in seconds since the epoch	
******************************************************************************/
unsigned long timer_s_wait(void)
{	
	unsigned long t1,t2;
	t1=timer_s_get();
	do
	{
		t2=timer_s_get();
	}
	while(t1==t2);
	return t2;
}
/******************************************************************************
	function: timer_s_get
*******************************************************************************
	Returns the time in seconds since the epoch. 
	
	This function requires the  _timer_tick_hz callback to be called at 1Hz.
	
	Returns:
		Time in seconds since the epoch	
******************************************************************************/
unsigned long timer_s_get(void)
{	
	unsigned long t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t=_timer_1hztimer_in_s;
	}
	return t;
}
/******************************************************************************
	function: timer_s_get_frommidnight
*******************************************************************************
	Returns the time in seconds since midnight. 
	
	This function requires the  _timer_tick_hz callback to be called at 1Hz.
	
	Returns:
		Time in seconds since the epoch	
******************************************************************************/
unsigned long timer_s_get_frommidnight(void)
{	
	unsigned long t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t=_timer_1hztimer_in_s_frommidnight;
	}
	return t;
}


/******************************************************************************
	function: timer_register_callback
*******************************************************************************
	                                            1000Hz
	Register a callback that will be called at --------- Hz.
	                                           divider+1
	       
	Parameters:
		callback		-	User callback
		divider			-	How often the callback must be called, i.e. 1024Hz/(divider+1) Hz.
		   
	Returns:
		-1				-	Can't register callback
		otherwise		-	Callback ID
	
******************************************************************************/
char timer_register_callback(unsigned char (*callback)(unsigned char),unsigned short divider)
{
	//printf("Register\n");
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(timer_numcallbacks>=TIMER_NUMCALLBACKS)
			return -1;
		timer_callbacks[timer_numcallbacks].callback=callback;
		timer_callbacks[timer_numcallbacks].counter=0;
		timer_callbacks[timer_numcallbacks].top=divider;
		timer_numcallbacks++;
		//printf("Now %d callbacks\n",timer_numcallbacks);
	}
	return timer_numcallbacks-1;
}
/******************************************************************************
	function: timer_register_slowcallback
*******************************************************************************
	                                             1Hz
	Register a callback that will be called at --------- Hz.
	                                           divider+1

	Slow callbacks are only available if the 1Hz tick is available (_timer_tick_hz), otherwise
	use timer_register_callback.

	Parameters:
		callback		-	User callback
		divider			-	How often the callback must be called, i.e. 1Hz/(divider+1) Hz.


	Returns:
		-1				-	Can't register callback
		otherwise		-	Callback ID
	
******************************************************************************/
char timer_register_slowcallback(unsigned char (*callback)(unsigned char),unsigned short divider)
{
	//printf("Register\n");
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(timer_numslowcallbacks>=TIMER_NUMCALLBACKS)
			return -1;
		timer_slowcallbacks[timer_numslowcallbacks].callback=callback;
		timer_slowcallbacks[timer_numslowcallbacks].counter=0;
		timer_slowcallbacks[timer_numslowcallbacks].top=divider;
		timer_numslowcallbacks++;
		//printf("Now %d callbacks\n",timer_numcallbacks);
	}
	return timer_numslowcallbacks-1;
}
/******************************************************************************
	function: timer_register_50hzcallback
*******************************************************************************
	                                             50Hz
	Register a callback that will be called at --------- Hz.
	                                           divider+1

	Slow callbacks are only available if the 50Hz tick is available (_timer_tick_50hz), 
	otherwise use timer_register_callback.

	Parameters:
		callback		-	User callback
		divider			-	How often the callback must be called, i.e. 50Hz/(divider+1) Hz.


	Returns:
		-1				-	Can't register callback
		otherwise		-	Callback ID
	
******************************************************************************/
char timer_register_50hzcallback(unsigned char (*callback)(unsigned char),unsigned short divider)
{
	//printf("Register\n");
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(timer_num50hzcallbacks>=TIMER_NUMCALLBACKS)
			return -1;
		timer_50hzcallbacks[timer_num50hzcallbacks].callback=callback;
		timer_50hzcallbacks[timer_num50hzcallbacks].counter=0;
		timer_50hzcallbacks[timer_num50hzcallbacks].top=divider;
		timer_num50hzcallbacks++;
		//printf("Now %d callbacks\n",timer_numcallbacks);
	}
	return timer_num50hzcallbacks-1;
}
/******************************************************************************
	function: timer_isregistered_callback
*******************************************************************************
	Check if the specified callback is registered at least once.
	
	Parameters:
		callback		-		User callback to check for registration
	Returns:
		0				-		Callback is not registered
		n 				-		Callback is registered n times
		
******************************************************************************/
unsigned char timer_isregistered_callback(unsigned char (*callback)(unsigned char))
{
	unsigned char n=0;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Iterate all entries to find the callback
		for(unsigned char i=0;i<timer_numcallbacks;i++)
		{
			if(timer_callbacks[i].callback==callback)
			{
				n++;
			}
		}
	}
	return n;
}
/******************************************************************************
	function: timer_unregister_callback
*******************************************************************************
	Unregisters a callback.
	
	Parameters:
		callback		-		User callback to unregister
******************************************************************************/
void timer_unregister_callback(unsigned char (*callback)(unsigned char))
{
	//printf("Unregister cb %p\n",callback);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Iterate all entries to find the callback
		for(unsigned char i=0;i<timer_numcallbacks;i++)
		{
			//printf("check %d: %p == %p?\n",i,timer_callbacks[i],callback);
			if(timer_callbacks[i].callback==callback)
			{
				//printf("found callback %d\n",i);
				// Found the callback - shift up the other callbacks
				for(unsigned char j=i;j<timer_numcallbacks-1;j++)
				{
					//printf("Move %d <- %d\n",j,j+1);
					timer_callbacks[j]=timer_callbacks[j+1];
				}
				timer_numcallbacks--;
				break;
			}
		}
	}
	//printf("Num callbacks: %d\n",timer_numcallbacks);
}
/******************************************************************************
	function: timer_unregister_slowcallback
*******************************************************************************
	Unregisters a slow callback.
	
	Parameters:
		callback		-		User callback to unregister	
******************************************************************************/
void timer_unregister_slowcallback(unsigned char (*callback)(unsigned char))
{
	//printf("Unregister cb %p\n",callback);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Iterate all entries to find the callback
		for(unsigned char i=0;i<timer_numslowcallbacks;i++)
		{
			//printf("check %d: %p == %p?\n",i,timer_callbacks[i],callback);
			if(timer_slowcallbacks[i].callback==callback)
			{
				//printf("found callback %d\n",i);
				// Found the callback - shift up the other callbacks
				for(unsigned char j=i;j<timer_numslowcallbacks-1;j++)
				{
					//printf("Move %d <- %d\n",j,j+1);
					timer_slowcallbacks[j]=timer_slowcallbacks[j+1];
				}
				timer_numslowcallbacks--;
				break;
			}
		}
	}
	//printf("Num callbacks: %d\n",timer_numcallbacks);
}
/******************************************************************************
	function: timer_unregister_50hzcallback
*******************************************************************************
	Unregisters a 50Hz callback.
	
	Parameters:
		callback		-		User callback to unregister	
******************************************************************************/
void timer_unregister_50hzcallback(unsigned char (*callback)(unsigned char))
{
	//printf("Unregister cb %p\n",callback);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Iterate all entries to find the callback
		for(unsigned char i=0;i<timer_num50hzcallbacks;i++)
		{
			//printf("check %d: %p == %p?\n",i,timer_callbacks[i],callback);
			if(timer_50hzcallbacks[i].callback==callback)
			{
				//printf("found callback %d\n",i);
				// Found the callback - shift up the other callbacks
				for(unsigned char j=i;j<timer_num50hzcallbacks-1;j++)
				{
					//printf("Move %d <- %d\n",j,j+1);
					timer_50hzcallbacks[j]=timer_50hzcallbacks[j+1];
				}
				timer_num50hzcallbacks--;
				break;
			}
		}
	}
	//printf("Num callbacks: %d\n",timer_numcallbacks);
}
/******************************************************************************
	function: timer_printcallbacks
*******************************************************************************
	Prints the list of registered callbacks.
	
	Parameters:
		f		-		FILE on which to print the information
******************************************************************************/
void timer_printcallbacks(FILE *f)
{
	fprintf(f,"Number of KHz callbacks %d/%d\n",timer_numcallbacks,TIMER_NUMCALLBACKS);
	for(unsigned char i=0;i<timer_numcallbacks;i++)
	{
		fprintf(f,"CB %d @%p %d/%d\n",i,timer_callbacks[i].callback,timer_callbacks[i].counter,timer_callbacks[i].top);
	}
	fprintf(f,"Number of 50Hz callbacks %d/%d\n",timer_num50hzcallbacks,TIMER_NUMCALLBACKS);
	for(unsigned char i=0;i<timer_num50hzcallbacks;i++)
	{
		fprintf(f,"CB %d @%p %d/%d\n",i,timer_50hzcallbacks[i].callback,timer_50hzcallbacks[i].counter,timer_50hzcallbacks[i].top);
	}
	fprintf(f,"Number of slow callbacks %d/%d\n",timer_numslowcallbacks,TIMER_NUMCALLBACKS);
	for(unsigned char i=0;i<timer_numslowcallbacks;i++)
	{
		fprintf(f,"CB %d @%p %d/%d\n",i,timer_slowcallbacks[i].callback,timer_slowcallbacks[i].counter,timer_slowcallbacks[i].top);
	}
}




/******************************************************************************
	function: timer_waitperiod_ms
*******************************************************************************
	Wait until a a period of time, or a multiple of it, has elapsed from the previous call.
		
	Prior to the first call to this function, wp must be set to 0.
	
	This function is time-wraparound-safe if the desired period and maximum
	time elapsed between each function call is lower than 0x80000000 ms (i.e. 24 days). 
		
	Parameters:
		p			-		Period in milliseconds
		wp			-		Pointer to WAITPERIOD, internally used to regularly
							schedule the waits.
							
	Returns:
		Current time in ms.		
******************************************************************************/
unsigned long timer_waitperiod_ms(unsigned short p,WAITPERIOD *wp)
{
	unsigned long t,wp2;
	
	// Current time
	t=timer_ms_get();
	
	// If wait delay is zero return immediately
	if(p==0)
		return t;
	
	// First call: initialise when is the next period
	if(*wp==0)
		*wp = t;
	
	
	// Set the wait deadline. This addition may wrap around
	wp2=*wp+p;
	
	//printf("wp: %lu wp2: %lu\n",*wp,wp2);
	
	// Identify closest next period. Logic is wrap-around safe assuming wait period or time between calls is lower than maxrange/2.
	// If wp2 is later than time then wp2-time is zero or a small number.
	// If wp2 is earlier than time, then wp2-time is larger some large value maxrange/2=0x80000000
	// If wp2 is equal to time, consider ok and go to wait.
	while(wp2-timer_ms_get()>=0x80000000)
		wp2+=p;
	
	// Wait until time passes the next period
	// If t is earlier than wp2 then the difference is larger than some large value maxrange/2=0x80000000
	// if t is equal or later than wp2, then the difference is zero or a small number.
	while( (t=timer_ms_get())-wp2>0x80000000 )
		;
		//_delay_us(10);		// Adding a us delay prevents triggering continuously interrupts
	
	// Store the next time into the user-provided pointer
	*wp=wp2;
	return t;	
}

/*unsigned long timer_waitperiod_us_old(unsigned long p,WAITPERIOD *wp)
{
	unsigned long t;
	
	return 0;
	return timer_us_get();
	
	if(p<50)											// Benchmarks show ~30uS minimum delay.
		return timer_us_get();
	
	// First call: initialise when is the next period
	if(*wp==0)
	{
		*wp = timer_us_get();
	}
	
	// Set wait deadline
	*wp+=p;	
	
	// Identify closest next period
	while(timer_us_get()>*wp)
		*wp+=p;
	
	// Wait until passed next period
	//while( (t=timer_us_get())<=*wp);	
	while( (t=timer_us_get())<=*wp)
		_delay_us(50);
	
	return t;	
}*/
/******************************************************************************
	function: timer_waitperiod_us
*******************************************************************************
	Wait until a a period of time, or a multiple of it, has elapsed from the previous call.
		
	Prior to the first call to this function, the content pointed by wp must be set to 0.
	
	This function is time-wraparound-safe if the desired period and maximum
	time elapsed between each function call is lower than 0x80000000 us (i.e. 35 minutes). 
	
	This function returns immediately for delays lower than 50uS.
	This function may not be sufficiently accurate for delays below about 150us.
	
	Parameters:
		p			-		Period in microseconds
		wp			-		Pointer to WAITPERIOD, internally used to regularly
							schedule the waits.
							
	Returns:
		Current time in us.
******************************************************************************/
/*unsigned long timer_waitperiod_us(unsigned long p,WAITPERIOD *wp)
{
	unsigned long t,wp2;
	
	// Current time
	t = timer_us_get();
	
	// Benchmarks show ~30uS minimum delay at 11MHz.
	// Return immediately for delays lower than 50uS, as the while loops to define the next period would be too slow
	if(p<50)											
		return t;
	
	// First call: initialise when is the next period
	if(*wp==0)
		*wp = t;
	
	// Set the wait deadline. This addition may wrap around
	wp2=*wp+p;
	
	// Identify closest next period. Logic is wrap-around safe assuming wait period or time between calls is lower than maxrange/2.
	// If wp2 is later than time then wp2-time is zero or a small number.
	// If wp2 is earlier than time, then wp2-time is larger some large value maxrange/2=0x80000000
	// If wp2 is equal to time, consider ok and go to wait.
	while(wp2-timer_us_get()>=0x80000000)
		wp2+=p;
	
	// Wait until time passes the next period
	// If t is earlier than wp2 then the difference is larger than some large value maxrange/2=0x80000000
	// if t is equal or later than wp2, then the difference is zero or a small number.
	while( (t=timer_us_get())-wp2>0x80000000 )
		_delay_us(10);
	
	// Store the next time into the user-provided pointer
	*wp=wp2;
	return t;	
}*/

/******************************************************************************
	function: timer_waitperiod_us_test
*******************************************************************************
	Tests of timer_waitperiod_us
******************************************************************************/
/*void timer_waitperiod_test(void)
{
	unsigned char n=64;
	unsigned long times[n];
	WAITPERIOD p;
	
	for(unsigned long period=1;period<=32000;period*=2)
	{
		printf("Test period %lu\n",period);
		
		p=0;
		for(unsigned char i=0;i<n;i++)
		{
			times[i] = timer_waitperiod_us(period,&p);			
		}
		//for(unsigned char i=0;i<n;i++)
			//printf("%lu ",times[i]);
		//printf("\n");
		for(unsigned char i=0;i<n;i++)
			printf("%lu ",times[i]-times[0]);
		printf("\n");
		for(unsigned char i=0;i<n-1;i++)
			printf("%lu ",times[i+1]-times[i]);
		printf("\n");
	}
	
}*/


/******************************************************************************
	function: timer_waitperiod_us
*******************************************************************************
	Wait until a a period of time, or a multiple of it, has elapsed from the previous call.
		
	Prior to the first call to this function, wp must be set to 0.
	
	This function is time-wraparound-safe if the desired period and maximum
	time elapsed between each function call is lower than 0x80000000 us (i.e. 35 minutes). 
	
	This function returns immediately for delays lower than 50uS.
	This function may not be sufficiently accurate for delays below about 150us.
	
	Parameters:
		p			-		Period in microseconds
		wp			-		Pointer to WAITPERIOD, internally used to regularly
							schedule the waits.
							
	Returns:
		Current time in us.
******************************************************************************/

/*
void wait_ms(unsigned int ms)
{

	unsigned int ocr, counter=0;
	unsigned char countL, countH;

	TCCR1A = 0x00;
	//if(ms > 65500/8)
	//	ms = 65500/8;
	//ocr = 8*ms; 		// Approx delay in ms with 1024 prescaler
	if(ms>65500/12)
		ms=65500/12;
	ocr = 12*ms;

	WAIT_TCNTH = 0;
	WAIT_TCNTL = 0;

	TIMSK1 = 0x00;		// no interrupt
	TCCR1B = 0x05;		// normal, prescaler=1024, start timer

	while(counter<ocr)
	{
		countL = WAIT_TCNTL;
		countH = WAIT_TCNTH;
		counter = (((unsigned int)countH) << 8) | (unsigned int)countL;
	}
	TCCR1B = 0x00;  	// disable timer
}
*/

/*
*/
/*
void timer_start(void)
{
	TCCR1A = 0x00;

	WAIT_TCNTH = 0;
	WAIT_TCNTL = 0;

	TIMSK1 = 0x00;		// no interrupt
	TCCR1B = 0x05;		// normal, prescaler=1024, start timer	
}
*/
/*
	Return the elapsed time since timer_start was called, in ~milliseconds (exactly 1.024ms/unit)
*/
/*
unsigned int timer_elapsed(void)
{
	unsigned char countL, countH;
	unsigned int counter;
	countL = WAIT_TCNTL;
	countH = WAIT_TCNTH;
	counter = (((unsigned int)countH) << 8) | (unsigned int)countL;
	return counter/12;	
}
*/

/*
	No prescaler, counts the CPU cycles.
*/
/*
void timer_start_fast(void)
{
	TCCR1A = 0x00;

	WAIT_TCNTH = 0;
	WAIT_TCNTL = 0;

	TIMSK1 = 0x00;		// no interrupt
	TCCR1B = 0x01;		// normal, prescaler=1024, start timer	
}
*/
/*
	Returns the elapsed time in CPU cycles
*/
/*
unsigned int timer_elapsed_fast(void)
{
	unsigned char countL, countH;
	unsigned int counter;
	countL = WAIT_TCNTL;
	countH = WAIT_TCNTH;
	counter = (((unsigned int)countH) << 8) | (unsigned int)countL;
	return counter;	
}
*/
/*void timer_rtc_start(void)
{
	TCCR2A = 0x00;

	ASSR=0x20;

	TCNT2H = 0;
	TCNT2L = 0;

	TIMSK1 = 0x00;		// no interrupt
	TCCR2B = 0x05;		// normal, prescaler=1024, start timer	
}*/
/*
	Return the elapsed time since timer_start was called, in ~milliseconds (exactly 1.024ms/unit)
*/
/*unsigned int timer_rtc_elapsed(void)
{
	unsigned char countL, countH;
	unsigned int counter;
	countL = WAIT_TCNTL;
	countH = WAIT_TCNTH;
	counter = (((unsigned int)countH) << 8) | (unsigned int)countL;
	return counter/8;	
}
*/



/*
	Interrupt-driven timer functions 
*/
//volatile unsigned long int timer_millisecond;



/*
	Initializes timer 0 with approx a millisecond timer count.

	Override this function according to system clock
*/
/*void timer_ms_init(void)
{
	// Stop the timer
	TCCR0B = 0x00;		// Stop

	// Init the tick counter
	timer_millisecond = 0;

	// Set-up the timer and activate the timer interrupt		
	TCCR0A = 0x02;			// Clear timer on compare
	TCNT0 = 0;				// Clear timer
	OCR0A	=	46;			// CTC value, assuming 12MHz: 12MHz/(46+1)/256 -> 1.002ms
	TIMSK0 	= 0x02;		// Output compare A match interrupt
	TCCR0B 	= 0x04;		// Normal, prescaler = 256, start timer	
	

}*/
/*extern unsigned char sharedbuffer[];
void timer_get_speedtest(void)
{
	// Test timer
	unsigned long *sb=(unsigned long*)sharedbuffer;
	
	timer_init(0,0,0);
	
	
	unsigned long t=0;
	unsigned long t1,t2;
	t1 = timer_ms_get();
	for(unsigned i=0;i<50000;i++)
			t+=timer_ms_get();
	t2 = timer_ms_get();
	printf("%lu (%lu)\n",t2-t1,t);
	t1 = timer_ms_get();
	for(unsigned i=0;i<50000;i++)
			t+=timer_us_get_c();
	t2 = timer_ms_get();
	printf("%lu (%lu)\n",t2-t1,t);
	
	while(1)
	{
		for(int i=0;i<128;i++)
			sb[i]=timer_ms_get();
		//printf("ms: "); for(int i=0;i<128;i++) printf("%lu ",sb[i]); printf("\n");
		printf("msdt: "); for(int i=0;i<127;i++) printf("%lu ",sb[i+1]-sb[i]); printf("\n");
		

		for(int i=0;i<128;i++)
			sb[i]=t2=timer_us_get();
		//printf("us: "); for(int i=0;i<128;i++) printf("%lu ",sb[i]); printf("\n");		
		printf("usdt: "); for(int i=0;i<127;i++) printf("%lu ",sb[i+1]-sb[i]); printf("\n");
		
		
			
		printf("\n");
		_delay_ms(1000);
	}
}
*/

void timer_us_wait(unsigned long us)
{
	unsigned long t1 = timer_us_get();
	while((timer_us_get()-t1)<us);
}
