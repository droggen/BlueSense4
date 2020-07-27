#include <stdio.h>
#include <string.h>

#include "stmrtc.h"
//#include "i2c.h"
//#include "i2c_int.h"
//#include "main.h"
//#include "helper.h"
//#include "system.h"
//#include "system-extra.h"
#include "global.h"
#include "main.h"
#include "wait.h"

/*
	file: stmrtc
	
	STM32 RTC functions
	
	The key functions are:
	
		* stm32_init:		Initialisation of the STM32 RTC
		* ds3232_readtimeregisters:	Read the DS3232 date/time registers immediately (registers 0-6).
		
	*STMR32 specifics*
	
	Backup register 0 contains 0x32F2, if not it indicates power loss.

	*Usage in interrupts*
	
	??
	


*/

// Indicates if power is lost
unsigned __stmrtc_powerlost=0;

// Custom function to call in the RTC interrupt handler every second
void (*stmrtc_irqhandler)()=0;

// Default
void stmrtc_defaultirqhandler()
{
	//HAL_GPIO_TogglePin(LED_2_GPIO_Port, LED_2_Pin);
	_timer_tick_hz();
}

void RTC_Alarm_IRQHandler()
{
	// Must deactivate the interrupt
	EXTI->PR1 = (1<<18);		// STM32L4
	// Clear the alarm A interrupt
	RTC->ISR=0xFFFFFEFF;

	// Tryouts
	//unsigned r = __get_PRIMASK();
	//itmprintf("PRIMASK: %08X\n",__get_PRIMASK());


	if(stmrtc_irqhandler)
		stmrtc_irqhandler();

}

/******************************************************************************
	function: stmrtc_init
*******************************************************************************
	Initialise the RTC:
	- check for power fail, reset date/time (see stmrtc_waspowerlost)
	- 1 Hz alarm A


	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_setirqhandler(void (*h)(void))
{
	stmrtc_irqhandler = h;
}

/******************************************************************************
	function: stmrtc_init
*******************************************************************************
	Initialise the RTC:
	- check for power fail, reset date/time (see stmrtc_waspowerlost)
	- 1 Hz alarm A

	Implementation for STM32F401 and STM32L4 - must enable appropriate code path with #if


	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_init(void)
{
	fprintf(file_pri,"STM RTC init... ");


	// Enable power
#if 0
	RCC->APB1ENR |= 1<<28;		// STM32F401 (sec 5.1.2 reference manual)
#endif
#if 1
	RCC->APB1ENR1 |= 1<<28;		// STM32L4 (sec 5.1.5 reference manual for STM32L4): Enable the power interface clock by setting the PWREN bits in the Section 6.4.19: APB1 peripheral clock enable register 1 (RCC_APB1ENR1)
#endif

	// Disable backup domain write protection
#if 0
	PWR->CR|=1<<8;		// STM32F4 (sec 5.1.2 reference manual)
#endif
#if 1
	PWR->CR1|=1<<8;		// Set the DBP bit in the Power control register 1 (PWR_CR1) to enable access to the backup domain
#endif

	// Select the RTC clock source in the Backup domain control register (RCC_BDCR)
	// Enable RTC clock, select LSE clock source, enable LSE
#if 0
	RCC->BDCR=(1<<15)|(1<<8)|(1<<0);		// STM32F401: RTCEN (15) | RTCSEL=01 (LSE) | LSEON (0)
#endif
#if 1
	RCC->BDCR=(1<<15)|(1<<8)|(1<<0);		// STM32L4: RTCEN (15) | RTCSEL=01 (LSE) | LSEON (0)
#endif

	// RTC interrupt Init
	NVIC_SetPriority(RTC_Alarm_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(RTC_Alarm_IRQn);

	// Disable protection
	stmrtc_unprotect();
	// Default registers, 24hr format, clear all interrupts
	stmrtc_resetdefaults();

	// Set prescalers if necessary
	stmrtc_setprescalers_unp(127,255);				// 32768Hz -> 1Hz


	// Enter initialisation
	//stmrtc_enterinit();

	// Alarm A can only be set in init mode, or if alarm disabled and flag
	while(!(RTC->ISR&1));
	//fprintf(file_pri,"ALMAWF: %d\n",RTC->ISR&1);

	// Set alarm A mask every second
	//              3       2       1
	//              1       3       5       7
	RTC->ALRMAR = 0b10000000100000001000000010000000;

	// Enable alarm A and interrupt
	RTC->CR|=1<<8;	// ALRAE
	RTC->CR|=1<<12;	// ALRAIE

	// Enable the EXTI interrupts
#if 0
	// STM32F401
	EXTI->IMR|=(1<<17);		// Enable line 17
	EXTI->RTSR|=(1<<17);	// Rising edge line 17
#endif
#if 1
	// STM32L4: RTC_ALARM RTC alarms through EXTI line 18 interrupts (sec. 13.3)
	EXTI->IMR1|=(1<<18);		// Enable line 18
	EXTI->RTSR1|=(1<<18);		// Rising edge line 18
#endif


	// Protect
	stmrtc_protect();

	// Check if data loss
	if(stmrtc_getbkpreg(0)!=0x32F2)
	{
		fprintf(file_pri,"Power was lost... ");
		__stmrtc_powerlost=1;
		stmrtc_setbkpreg(0,0x32F2);

		// Reset the time to 00:00:00 and date to 01.01.01
		//stmrtc_write...
		stmrtc_writedatetime(1,1,1,0,0,0,0);
	}

	char buffer[64];
	unsigned char hour,min,sec,day,month,year,weekday;
	stmrtc_readdatetime_conv_int(0,&hour,&min,&sec,&weekday,&day,&month,&year);
	stmrtc_formatdatetime(buffer,weekday,day,month,year,hour,min,sec);
	fprintf(file_pri,"Date/Time: %s... ",buffer);

	fprintf(file_pri,"Done\n");
}

/******************************************************************************
	function: stmrtc_resetdefaults
*******************************************************************************
	Resets CR and ISR to defaults.
	Note that this looses information from the BKP bits in CR and tamper in ISR.


	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_resetdefaults()
{
	// Default CR:
	// 24 hr mode
	// All interrupts disables
	// No wakeup
	//          3      2     1
	//          1      4     8         8
	RTC->CR = 0b0000000000000000000000000000000;

	// Default ISR:
	// Keep free-running mode
	// Clear all interrupts, tamper, wakeup flags
	//           1  1
	//           6  3    8
	RTC->ISR = 0b00000000000000000;
}
/******************************************************************************
	function: stmrtc_waspowerlost
*******************************************************************************



	Parameters:
		-
	Returns:
		-

******************************************************************************/
unsigned stmrtc_waspowerlost(void)
{
	return __stmrtc_powerlost;
}

/******************************************************************************
	function: stmrtc_printregs
*******************************************************************************
	Print all the STM RTC registers

	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_printregs(FILE *f)
{
	fprintf(f,"STM RTC Registers:\n");

	uint32_t *a;
	a = (uint32_t*)RTC;
	for(int i=0;i<=39;i++)
	{
		fprintf(f,"R%02X %02X: %08X\n",i,i*4,(unsigned)*(a+i));
	}

}
/******************************************************************************
	function: stmrtc_protect
*******************************************************************************
	Protect the RTC registers

	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_protect()
{
	//fprintf(file_pri,"DBP bit in PWR before: %d\n",(PWR->CR>>8)&0b1);
	RTC->WPR = 0xFF;
	//fprintf(file_pri,"DBP bit in PWR after: %d\n",(PWR->CR>>8)&0b1);
}
/******************************************************************************
	function: stmrtc_unprotect
*******************************************************************************
	Unprotect the RTC registers.
	Compatibility: STM32F401, STM32L4

	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_unprotect()
{
	//fprintf(file_pri,"DBP bit in PWR before: %d\n",(PWR->CR>>8)&0b1);

	RTC->WPR = 0xCA;
	RTC->WPR = 0x53;

	//fprintf(file_pri,"DBP bit in PWR after: %d\n",(PWR->CR>>8)&0b1);
}
/******************************************************************************
	function: stmrtc_enterinit
*******************************************************************************
	Enter the initialisation mode (i.e. stop the clock to allow updates).

	If the system is already in init mode this function returns immediately.

	Parameters:
		-
	Returns:
		0			Did not enter init mode, as the RTC was already in init mode
		1			Entered init mode (previously the RTC was not in init mode)

******************************************************************************/
int stmrtc_enterinit()
{
	// Check if init mode.
	//fprintf(file_pri,"enterinit: %08X\n",RTC->ISR);
	if(RTC->ISR&0b1000000)
	{
		fprintf(file_pri,"already in\n");
		return 0;
	}

	// Set bit 7, and keep all bits unchanged (only actions happen when writing zeros).
	//RTC->ISR = RTC->ISR|0x80;
	RTC->ISR = 0xFFFFFFFF;
	//fprintf(file_pri,"after setting init: %08X\n",RTC->ISR);
	// Wait for INITF to be set
	while(!(RTC->ISR&0b1000000))
	{
		//fprintf(file_pri,"after setting init: %08X\n",RTC->ISR);
		HAL_Delay(10);
	}

	// In init mode
	return 1;
}
/******************************************************************************
	function: stmrtc_leaveinit
*******************************************************************************
	Leave the initialisation mode

	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_leaveinit()
{
	// Clear bit 7 (as in the LL library, set all bits)
	RTC->ISR = ~0x00000080;
}
/******************************************************************************
	function: stmrtc_setprescalers
*******************************************************************************
	Set synchronous and asynchronous prescalers.
	Frequency of RTC output is frtc/(asyncprescaler+1)/(syncprescaler+1)

	The current prescalers are checked; if they match the desired prescaler
	the function returns.
	Otherwise, the RTC enters initialisation mode (stops the clock) to set
	the prescaler.
	The init stat of the RTC is restored


	Parameters:
		asyncprescaler:			Value from 0 to 127 inclusive
		syncprescaler:			Value from 0 to 32737 inclusive
	Returns:
		-

******************************************************************************/
void stmrtc_setprescalers(unsigned asyncprescaler,unsigned syncprescaler)
{
	stmrtc_unprotect();
	stmrtc_setprescalers_unp(asyncprescaler,syncprescaler);
	stmrtc_protect();
}
void stmrtc_setprescalers_unp(unsigned asyncprescaler,unsigned syncprescaler)
{
	// Basic sanitation
	asyncprescaler=(asyncprescaler&0x7f)<<16;
	syncprescaler&=0x7fff;

	unsigned nprer = asyncprescaler | syncprescaler;
	unsigned cprer = RTC->PRER;

	//fprintf(file_pri,"Current prescaler: %08X\n",cprer);

	// Current prescaler matches
	if( (cprer & 0x7fffff) == nprer)
	{
		//fprintf(file_pri,"Prescaler unchanged: %08X\n",cprer);
		return;
	}

	fprintf(file_pri,"Prescaler updated from %08X to %08X. ",cprer,nprer);

	// Prescaler must be updated - requires to enter init mode
	int didinit = stmrtc_enterinit();
	// Prescaler must be updated in two separate writes: program first the synchronous prescaler  and then program the asynchronous prescaler
	RTC->PRER = (RTC->PRER&0xFFFF8000)|syncprescaler;		// Program the synchronous prescaler (other unchanged)
	RTC->PRER = (RTC->PRER&0xFF80FFFF)|asyncprescaler;		// Program the asynchronous prescaler (other unchanged)


	fprintf(file_pri,"After update: %08X\n",(unsigned)RTC->PRER);

	// Restores the RTC init state to what was on function call
	if(didinit)
		stmrtc_leaveinit();
}
/******************************************************************************
	function: stmrtc_enablebypass
*******************************************************************************



	Parameters:
		enable:				0 - disable bypass: calendar values taken from shadow registers
							1 - enable bypasss: calendar values taken directly from registers
	Returns:
		-

******************************************************************************/
void stmrtc_enablebypass(unsigned bypass)
{
	stmrtc_unprotect();

	bypass=bypass?1:0;

	RTC->CR=(RTC->CR&0xFFFFFFDF) | (bypass<<5);

	stmrtc_protect();
}
/******************************************************************************
	function: stmrtc_getbkpreg
*******************************************************************************
	Returns the backup register stmrtc_getbkpreg


	Parameters:
		reg:				Register from 0 to 19 inclusive
		syncprescaler:			Value from 0 to 32737 inclusive
	Returns:
		-

******************************************************************************/
unsigned stmrtc_getbkpreg(unsigned reg)
{
	volatile uint32_t *bkp = &RTC->BKP0R;

	/*fprintf(file_pri,"RTC: %p\n",RTC);
	fprintf(file_pri,"RTC BKP0r: %p\n",bkp);
	fprintf(file_pri,"RTC BKP1r: %p %p\n",bkp+1,&RTC->BKP1R);
	fprintf(file_pri,"RTC BKPx: %p\n",bkp+reg);*/

	return (unsigned)*(bkp+reg);
}
/******************************************************************************
	function: stmrtc_setbkpreg
*******************************************************************************
	Set the backup register


	Parameters:
		reg:				Register from 0 to 19 inclusive
		val:				Value
	Returns:
		-

******************************************************************************/
void stmrtc_setbkpreg(unsigned reg,unsigned val)
{
	volatile uint32_t *bkp = &RTC->BKP0R;

	//fprintf(file_pri,"RTC: %p\n",RTC);
	//fprintf(file_pri,"RTC BKP0r: %p\n",bkp);
	//fprintf(file_pri,"RTC BKP1r: %p %p\n",bkp+1,&RTC->BKP1R);
	//fprintf(file_pri,"RTC BKPx: %p\n",bkp+reg);

	//stmrtc_unprotect();
	*(bkp+reg) = val;
	//stmrtc_protect();
}
/******************************************************************************
	function: stmrtc_set24hr
*******************************************************************************
	Set 24hr format


	Parameters:
		-
	Returns:
		-

******************************************************************************/
void stmrtc_set24hr()
{
	// Clear bit 6
	RTC->CR = RTC->CR&(~(1<<6));		// 24 hr format
}
/******************************************************************************
	function: ds3232_init
*******************************************************************************	
	Initialisation of the DS3232 with the following:
	- 1 Hz square wave on INTC#/SWQ pin when powered on; disabled (hiZ) when powered off (VBat only)
	- 32 KHz pin disabled when powered on and powered off
	
	Parameters:
		-
	Returns:
		-

******************************************************************************/
/*void ds3232_init(void)
{
	unsigned char r __attribute__((unused));
	fprintf_P(file_pri,PSTR("DS3232 init... "));
	//fprintf(file_pri,"Before write control\n");
	//_delay_ms(500);
	r = ds3232_write_control(0b00011000);			// Enable oscillator, disable battery backed 1Hz SQW, enable normal square wave 1Hz
	//printf("write control: %02Xh\n",r);
	//fprintf(file_pri,"Before write status\n");
	//_delay_ms(500);
	r = ds3232_write_status(0b00000000);			// Clear stopped flag, clear alarm flag, disable battery-backed 32KHz, disable normal 32KHz
	//printf("write status: %02Xh\n",r);
	fprintf_P(file_pri,PSTR("done\n"));
}*/


/******************************************************************************
	function: ds3232_readtimeregisters
*******************************************************************************	
	Read the DS3232 date/time registers immediately (registers 0-6).
	
	Parameters:
		time		-	Pointer to a buffer of 7 bytes which will hold the raw time registers
	
	Returns:
		0			-	Success
		1			-	Error
******************************************************************************/
/*unsigned char ds3232_readtimeregisters(unsigned char *regs)
{
	unsigned char r;
	
	// Perform read.
	r = i2c_readregs(DS3232_ADDRESS,0,7,regs);
	if(r!=0)
	{
		memset(regs,0,7);
	}
	return 0;
}*/

/*


	Returns the time from DS3232 at the instant the second register changes.
	This can be used to initialise local timers.
	
	time:				7 bytes long buffer that will receive the first 7 time register of DS3232
	
	Return:			zero in case of success, nonzero for i2c error
*/
/*unsigned char ds3232_readtime_sync(unsigned char *time)
{
	unsigned char r;
	unsigned char val1[7];
	
	// Perform initial read.
	r = i2c_readregs(DS3232_ADDRESS,0,7,val1);
	//printf("%d\n",r);
	if(r!=0x00)
		return r;
	//for(int i=0;i<7;i++)
		//printf("%02X ",val1[i]);
			
	// Continuous read until second change
	do
	{
		r = i2c_readregs(DS3232_ADDRESS,0,7,time);
		//printf("%d\n",r);
		if(r!=0x00)
			return r;
		//for(int i=0;i<7;i++)
			//printf("%02X ",time[i]);
	} 
	while(val1[0] == time[0]);
	return 0;
}*/
/*
	Returns the time from DS3232 at the instant the second register changes.
	This can be used to initialise local timers.
	
	This function synchronizes using the square wave signal, which is assumed
	to be handled by an interrupt handler that increments a 
	variable called rtc_time_sec.
	
	time:				7 bytes long buffer that will receive the first 7 time register of DS3232
	
	Return:			zero in case of success, nonzero for i2c error
*/
/*extern unsigned long rtc_time_sec;
unsigned char ds3232_readtime_sqwsync(unsigned char *time)
{
	unsigned char r;
	
	unsigned long st;
	
	// Check current time
	st = rtc_time_sec;
	// Loop until time changes
	while(rtc_time_sec==st);
	// Read the time	
	r = i2c_readregs(DS3232_ADDRESS,0,7,time);
	return r;
}*/





/******************************************************************************
	ds3232_readtime_conv
*******************************************************************************	
	
	Return value:
		0:			success
		1:			error
******************************************************************************/
/*unsigned char ds3232_readtime_conv(unsigned char *hour,unsigned char *min,unsigned char *sec)
{
	unsigned char val[7];
	unsigned char r;
	
	r = ds3232_readtime(val);
	if(r!=0)
		return r;
		
	ds3232_convtime(val,hour,min,sec);

	return 0;
}*/



/******************************************************************************
	function: ds3232_sync_fall
*******************************************************************************	
	Waits until a falling edge of the DS3232 SQW pin. 
	
	The DS3232 time registers are updated on the falling edge.
	
	Waiting for a falling edge leaves 1 second to read the time info before the 
	time registers change again.
	
	Parameters:
		-

	Returns:
		-

******************************************************************************/
/*void ds3232_sync_fall(void)
{
	unsigned char pc,pn;		// pc: current pin state, pn: new pin state
	// Wait for the falling edge of the RTC int
	pc=system_getrtcint();
	while( !( ((pn=system_getrtcint())!=pc) && pn==0) )		// Loop until pn!=pc and pn==0
		pc=pn;
}*/
/******************************************************************************
	function: ds3232_sync_rise
*******************************************************************************	
	Waits until a rising edge of the DS3232 SQW pin. 
	
	The DS3232 time registers are updated on the falling edge.
	
	Waiting for a rising edge leaves 500ms to read the time info before the time
	registers change.
			
	Parameters:
		-

	Returns:
		-

******************************************************************************/
/*void ds3232_sync_rise(void)
{
	unsigned char pc,pn;		// pc: current pin state, pn: new pin state
	// Wait for the rising edge of the RTC int
	pc=system_getrtcint();
	while( !( ((pn=system_getrtcint())!=pc) && pn==1) )		// Loop until pn!=pc and pn==1
		pc=pn;
	return;
}*/

/******************************************************************************
	function: stmrtc_readdatetime_conv_int
*******************************************************************************	
	Returns the DS3232 time in hh:mm.ss format, optionally with synchronising return.
	
	In synchronised mode (sync=1), this function waits for a change of second and then reads and returns the date/time.
	In immediate mode (sync=0) this function immediately reads and returns the date/time.
	
	Use the synchronised mode when the precise moment at which the time is read is important. This
	mode introduces a latency of up to one second, as it must wait for a second change before returning.
	
	If a pointer to the variables receiving the return values is null that value is not returned.
		
	Parameters:
		sync		-	0: to read and return the time immediately, 1 to read and return the time at a change of seconds.
		hour		-	Pointer to the variable holding the returned hours (pass 0 if not needed)
		min			-	Pointer to the variable holding the returned min (pass 0 if not needed)
		sec			-	Pointer to the variable holding the returned sec (pass 0 if not needed)
		day			-	Pointer to the variable holding the returned day (pass 0 if not needed)
		month		-	Pointer to the variable holding the returned month (pass 0 if not needed)
		year		-	Pointer to the variable holding the returned year (pass 0 if not needed)

	Returns:
		0			-	Success, the returned time is provided in the variables hour, min, sec.
		1			-	Error
******************************************************************************/
/*unsigned char stmrtc_readdatetime_conv_int_legacy(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *day,unsigned char *month,unsigned char *year)
{
	//unsigned char regs[7];

	if(sync)
	{
		// Wait for the falling edge of the RTC int
		//ds3232_sync_fall();
	}
	// Read time

#if STMRTC_LL==1
	// TODO: must lock time to ensure date does not wrap around
	unsigned int time = LL_RTC_TIME_Get(RTC);
	unsigned int date = LL_RTC_DATE_Get(RTC);


	if(hour) *hour=__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_HOUR(time));
	if(min) *min=__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MINUTE(time));
	if(sec) *sec=__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_SECOND(time));
	if(day) *day=__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_DAY(date));
	if(month) *month=__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_MONTH(date));
	if(year) *year=__LL_RTC_CONVERT_BCD2BIN(__LL_RTC_GET_YEAR(date));
#endif
#if STMRTC_HAL==1
		RTC_TimeTypeDef tim;
		RTC_DateTypeDef dat;
		HAL_RTC_WaitForSynchro(&hrtc);
		HAL_RTC_GetTime(&hrtc, &tim, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &dat, RTC_FORMAT_BIN);

		fprintf(file_pri,"stmrtc_readdatetime_conv_int: %d %d %d\n",tim.Hours,tim.Minutes,tim.Seconds);
		if(hour)
			*hour = tim.Hours;
		if(min)
			*min = tim.Minutes;
		if(sec)
			*sec = tim.Seconds;
		//tim.Hours
#endif


	return 0;
}*/
unsigned char stmrtc_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *weekday,unsigned char *day,unsigned char *month,unsigned char *year)
{
	if(sync)
	{
		// Wait for a change of second
		unsigned char s1;
		stmrtc_readdatetime_conv_int(0,0,0,&s1,0,0,0,0);
		unsigned char s2 = s1;
		while(s2==s1)
			stmrtc_readdatetime_conv_int(0,0,0,&s2,0,0,0,0);
	}

	// Read time
	stm_sync();

	unsigned tr = RTC->TR;
	unsigned dr = RTC->DR;

	if(hour)
		*hour = STMRTC_BCD2BIN((tr>>16)&0b1111111);
	if(min)
		*min = STMRTC_BCD2BIN((tr>>8)&0b1111111);
	if(sec)
		*sec = STMRTC_BCD2BIN((tr>>0)&0b1111111);

	if(year)
		*year = STMRTC_BCD2BIN((dr>>16)&0b11111111);
	if(weekday)
		*weekday = STMRTC_BCD2BIN((dr>>13)&0b111);
	if(month)
		*month = STMRTC_BCD2BIN((dr>>8)&0b11111);
	if(day)
		*day = STMRTC_BCD2BIN((dr>>0)&0b111111);



	return 0;
}

// Synchronise to the latch of the time register in shadow registers
void stm_sync()
{
	// Clear RSF
	RTC->ISR = 0xFFFFFFDF;
	// Wait until RSF set
	while(!(RTC->ISR&0b100000));
}
/******************************************************************************
	ds3232_readdate_conv
*******************************************************************************	
	
	Return value:
		0:			success
		1:			error
******************************************************************************/
/*unsigned char ds3232_readdate_conv(unsigned char *date,unsigned char *month,unsigned char *year)
{
	unsigned char r;
	unsigned char data[4];
	//unsigned char day;
	
	// Perform read.
	r = i2c_readregs(DS3232_ADDRESS,3,4,data);
	if(r!=0)
	{
		*date=*month=*year=0;
		return r;
	}
		
	//day = data[0];
	*date = ds3232_bcd2dec(data[1]);
	*month = ds3232_bcd2dec(data[2]);
	*year = ds3232_bcd2dec(data[3]);
	
	return 0;
}*/


/******************************************************************************
	ds3232_readdate_conv_int
*******************************************************************************	
	
	I2C transactional
	
	Return value:
		0:			success
		1:			error
******************************************************************************/
/*unsigned char ds3232_readdate_conv_int(unsigned char *date,unsigned char *month,unsigned char *year)
{
	unsigned char r;
	unsigned char data[4];
	//unsigned char day;
	
	// Perform read.
	r = i2c_readregs_int(DS3232_ADDRESS,3,4,data);
	if(r!=0)
	{
		*date=*month=*year=0;
		return r;
	}
		
	//day = data[0];
	*date = ds3232_bcd2dec(data[1]);
	*month = ds3232_bcd2dec(data[2]);
	*year = ds3232_bcd2dec(data[3]);
	
	return 0;
}*/

/*unsigned char ds3232_readtime_sync_conv(unsigned char *hour,unsigned char *min,unsigned char *sec)
{
	unsigned char val[7];
	unsigned char r;
	
	r = ds3232_readtime_sync(val);
	if(r!=0)
		return r;
		
	ds3232_convtime(val,hour,min,sec);

	return 0;
}

unsigned char ds3232_bcd2dec(unsigned char bcd)
{
	return (bcd&0xf) + (bcd>>4)*10;
}*/

/******************************************************************************
	function:	ds3232_convtime
*******************************************************************************
	Convert the time (register 0-2) of DS3232 into hours, minutes and seconds.
	
	The pointers to the return variable can be null, in which case the corresponding
	parameters is not returned.
	
	
	Parameters:
		val			-		Pointer to register 0-6 of date/time
		hour		-		Pointer to the variable receiving the hours, or 0 if unneeded
		min			-		Pointer to the variable receiving the minutes, or 0 if unneeded
		sec			-		Pointer to the variable receiving the seconds, or 0 if unneeded
		
	Returns:
		-
******************************************************************************/
/*void ds3232_convtime(unsigned char *val,unsigned char *hour,unsigned char *min,unsigned char *sec)
{
	// Do the conversion
	if(sec)
		*sec = ds3232_bcd2dec(val[0]);
	if(min)
		*min = ds3232_bcd2dec(val[1]);
	if(hour)
	{
		if(val[2]&0x40)
		{
			// 12hr
			*hour = (val[2]&0xf);
			if(val[2]&0x20)
				*hour+=12;
		}
		else
		{
			// 24hr
			*hour = ds3232_bcd2dec(val[2]);
		}
	}
}*/
/******************************************************************************
	function:	ds3232_convdate
*******************************************************************************
	Convert the date (register 3-6) of DS3232 into day, month, year.
	
	The pointers to the return variable can be null, in which case the corresponding
	parameters is not returned.
		
	Parameters:
		val			-		Pointer to register 0-6 of date/time
		day			-		Pointer to the variable receiving the day, or 0 if unneeded
		month		-		Pointer to the variable receiving the month, or 0 if unneeded
		year		-		Pointer to the variable receiving the year, or 0 if unneeded
		
	Returns:
		-
******************************************************************************/
/*void ds3232_convdate(unsigned char *val,unsigned char *day,unsigned char *month,unsigned char *year)
{
	if(day)
		*day = ds3232_bcd2dec(val[4]);
	if(month)
		*month = ds3232_bcd2dec(val[5]);
	if(year)
		*year = ds3232_bcd2dec(val[6]);
	
}*/


/******************************************************************************
	ds3232_readtemp
*******************************************************************************	
	Reads the ds3232 temperature.
	
	Returns temperature*100 (i.e. 2125 is 21.25°C)
	
	Return value:
		0:			success
		1:			error
******************************************************************************/
/*unsigned char ds3232_readtemp(signed short *temp)
{
	unsigned char r;
	unsigned char t[2];
	
	// Perform read.
	r = i2c_readregs(DS3232_ADDRESS,0x11,2,t);
	if(r!=0)
	{
		*temp=0;
		return r;
	}
	
	
	// Convert temperature
	*temp = t[0]*100;
	*temp += (t[1]>>6)*25;
	
	//printf("%02d %02d -> %04d\n",t[0],t[1],*temp);
	
	return 0;
}
*/


/******************************************************************************
	ds3232_readtemp_int_cb
*******************************************************************************	
	Reads the ds3232 temperature in background using the I2C transactional 
	mechanism.
	
	Calls the provided callback with status (0=success) and temperature*100 
	(i.e. 2125 is 21.25°C) when the reading is completed.
	
	Return value:
		0:			successfully issued a conversion
		1:			error
******************************************************************************/
/*unsigned char ds3232_readtemp_int_cb(void (*cb)(unsigned char,signed short))
{
	unsigned char r = i2c_readregs_int_cb(DS3232_ADDRESS,0x11,2,0,ds3232_readtemp_int_cb_cb,(void*)cb);
	if(r)
		return 1;
			
	return 0;	
}



unsigned char ds3232_readtemp_int_cb_cb(I2C_TRANSACTION *t)
{
	//printf("xx %02x %02x   %02x %02x %02x\n",t->status,t->i2cerror,t->data[0],t->data[1],t->data[2]);
	// Call the final user callback
	signed short temp;
	temp = t->data[0]*100;
	temp += (t->data[1]>>6)*25;
	void (*cb)(unsigned char,signed short);
	cb = (void(*)(unsigned char,signed short))t->user;
	cb(t->status,temp);
	return 0;	
}
*/

/*
	Write val in the DS3232 control register (address 0x0E)
	
	Returns:	zero success
						nonzero error; value indicates I2C error
*/
/*unsigned char ds3232_write_control(unsigned char val)
{
	return i2c_writereg(DS3232_ADDRESS,0x0E,val);
}*/

/*
	Write val in the DS3232 status register (address 0x0E)
	
	Returns:	zero success
						nonzero error; value indicates I2C error
*/
/*unsigned char ds3232_write_status(unsigned char val)
{
	return i2c_writereg(DS3232_ADDRESS,0x0F,val);
}*/


/*void ds3232_printreg(FILE *file)
{
		unsigned char r1 __attribute__((unused));
		unsigned char data[32];
	
		fprintf_P(file,PSTR("DS3232 registers:"));
		
		r1 = i2c_readregs(DS3232_ADDRESS,0,16,data);
		r1 = i2c_readregs(DS3232_ADDRESS,16,16,data+16);
		//fprintf_P(file,PSTR("readregs return: %d\n"),r1);
		
		// Pretty print as 2 rows of 16 registers
		for(int j=0;j<2;j++)
		{
			fprintf_P(file,PSTR("\n\t%02X-%02X: "),j*16,j*16+15);
			for(int i=0;i<16;i++)
			{		
				fprintf_P(file,PSTR("%02X "),data[j*16+i]);
			}
		}
		fprintf_P(file,PSTR("\n"));

}*/
/******************************************************************************
	function: stm32_writetime
*******************************************************************************
	Writes the time to the RTC.

	Disables write protection and enters initialisation mode, updates,
	and leaves init and re-protect

	Range of values:
		sec € [0;59]
		min € [0;59]
		hour € [0;23]

	Return value:
		0:		Success
		other:	Error
******************************************************************************/
void stm32_writetime(unsigned int hour,unsigned int min,unsigned int sec)
{
	stmrtc_unprotect();
	int didinit = stmrtc_enterinit();

	uint32_t r = (STMRTC_BIN2BCD(hour)<<16) | (STMRTC_BIN2BCD(min)<<8) | (STMRTC_BIN2BCD(sec)<<0);

	RTC->TR = r;

	if(didinit)
		stmrtc_leaveinit();
	stmrtc_protect();



}


/******************************************************************************
	stm32_writetime
*******************************************************************************
	Writes the time to the RTC. 
	Range of values:
		sec € [0;59]
		min € [0;59]
		hour € [0;23]
	
	Return value:
		0:			Success
		other:	Error
******************************************************************************/
/*unsigned int stm32_writetime_st(unsigned int hour,unsigned int min,unsigned int sec)
{
#if STMRTC_HAL==1
	RTC_TimeTypeDef sTime;

	sTime.Hours = hour;
	sTime.Minutes = min;
	sTime.Seconds = sec;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
	{
		return 1;
	}
	return 0;
#endif
#if STMRTC_LL==1
	LL_RTC_TimeTypeDef RTC_TimeStruct;
	RTC_TimeStruct.Hours = hour;
	RTC_TimeStruct.Minutes = min;
	RTC_TimeStruct.Seconds = sec;
	LL_RTC_TIME_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_TimeStruct);
	return 0;
#endif
}*/

/******************************************************************************
	stm32_writedate_st
*******************************************************************************
	Writes the date in day,month,year to the RTC.
	Range of values:
		day € [1;7]
		date € [1;30]
		month € [1;12]
		year € [0;99]

	Return value:
		0:			Success
		other:	Error
******************************************************************************/
/*unsigned char stm32_writedate_st(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year)
{
	unsigned char r;
	unsigned char v;

#if STMRTC_HAL==1
	return 0;
#endif

#if STMRTC_LL==1
	LL_RTC_DateTypeDef RTC_DateStruct;
	RTC_DateStruct.WeekDay = weekday;
	RTC_DateStruct.Day = day;
	RTC_DateStruct.Month = month;
	RTC_DateStruct.Year = year;
	LL_RTC_DATE_Init(RTC, LL_RTC_FORMAT_BIN, &RTC_DateStruct);
	return 0;
#endif

}*/

/******************************************************************************
	stm32_writedate
*******************************************************************************
	Writes the date in day,month,year to the RTC. 
	Range of values:
		day € [1;7]
		date € [1;30]
		month € [1;12]
		year € [0;99]
	
	Return value:
		0:			Success
		other:	Error
******************************************************************************/
void stm32_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year)
{
	//fprintf(file_pri,"stm32_writedate\n");
	stmrtc_unprotect();
	//fprintf(file_pri,"unp\n");
	int didinit = stmrtc_enterinit();
	//fprintf(file_pri,"enter init\n");

	uint32_t r = (STMRTC_BIN2BCD(year)<<16) | (STMRTC_BIN2BCD(weekday)<<13) | (STMRTC_BIN2BCD(month)<<8) | (STMRTC_BIN2BCD(day)<<0);

	RTC->DR = r;

	if(didinit)
		stmrtc_leaveinit();
	stmrtc_protect();
}
/******************************************************************************
	function: stm32_writedatetime
*******************************************************************************
	Writes the date and time to the RTC.

	Disables write protection and enters initialisation mode, updates,
	and leaves init and re-protect

	Range of values:
		day € [1;7]
		date € [1;30]
		month € [1;12]
		year € [0;99]
		sec € [0;59]
		min € [0;59]
		hour € [0;23]

	Return value:
		0:		Success
		other:	Error
******************************************************************************/
void stmrtc_writedatetime(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec)
{
	stmrtc_unprotect();
	int didinit = stmrtc_enterinit();


	uint32_t tr = (STMRTC_BIN2BCD(hour)<<16) | (STMRTC_BIN2BCD(min)<<8) | (STMRTC_BIN2BCD(sec)<<0);
	uint32_t dr = (STMRTC_BIN2BCD(year)<<16) | (weekday<<13) | (STMRTC_BIN2BCD(month)<<8) | (STMRTC_BIN2BCD(day)<<0);

	RTC->TR = tr;
	RTC->DR = dr;

	if(didinit)
		stmrtc_leaveinit();
	stmrtc_protect();
}
void stmrtc_formatdatetime(char *buffer,unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec)
{
	sprintf(buffer,"%02u:%02u:%02u (%d) %02d.%02d.%02d",hour,min,sec,weekday,day,month,year);
}


/*void ds3232_off(void)
{
	unsigned char r;
	unsigned char v[4];
	unsigned char hour,min,sec;
	hour=0;
	min=0;
	sec=0;
	
	fprintf_P(file_pri,PSTR("Register initially\n"));
	ds3232_printreg(file_pri);
	
	r = ds3232_writedate(1,1,1,10);
	fprintf_P(file_pri,PSTR("Write date: %d\n"),r);
	r = ds3232_writetime(hour,min,sec);
	fprintf_P(file_pri,PSTR("Write time: %d\n"),r);
	
	
	// deactivate 1Hz, activate alarm 1
	r = ds3232_write_control(0b00011101);			
	// Clear stopped flag, clear alarm flag, disable battery-backed 32KHz, enable normal 32KHz	
	r = ds3232_write_status(0b00001000);			
	
	// Alarm 1: 0x07-0x0A
	// Time
	sec+=10;
	v[0] = ((sec/10)<<4) + (sec%10);
	v[1] = ((min/10)<<4) + (min%10);
	v[2] = ((hour/10)<<4) + (hour%10);
	r = i2c_writeregs(DS3232_ADDRESS,0x07,v,3);
	fprintf_P(file_pri,PSTR("Write alm time: %d\n"),r);
	r = i2c_writereg(DS3232_ADDRESS,0x0a,1);
	fprintf_P(file_pri,PSTR("Write alm day: %d\n"),r);
	
	ds3232_printreg(file_pri);
	
	

}*/
/*void ds3232_alarm_at(unsigned char date, unsigned char month, unsigned char year,unsigned char hour,unsigned char min,unsigned char sec)
{
	unsigned char v[4];
	unsigned char r __attribute__((unused));
	
	// Alarm 1: 0x07-0x09 is time
	v[0] = ((sec/10)<<4) + (sec%10);
	v[1] = ((min/10)<<4) + (min%10);
	v[2] = ((hour/10)<<4) + (hour%10);
	r = i2c_writeregs(DS3232_ADDRESS,0x07,v,3);
	// Alarm 1: 0x0A-0x0C is date
	v[0] = ((date/10)<<4) + (date%10);
	v[1] = ((month/10)<<4) + (month%10);
	v[2] = ((year/10)<<4) + (year%10);
	r = i2c_writeregs(DS3232_ADDRESS,0x0a,v,3);
	
	r = ds3232_write_control(0b00011101);			// Enable oscillator, disable battery backed 1Hz SQW, enable interrupt, enable alarm 1
	r = ds3232_write_status(0b00000000);			// Clear stopped flag, clear alarm flag, disable battery-backed 32KHz, disable normal 32KHz
	
	ds3232_printreg(file_pri);
}*/
/*
void ds3232_alarm_in(unsigned short insec)
{
	unsigned char r __attribute__((unused));
	unsigned char hour,min,sec;
	unsigned char date,month,year;
	unsigned short hour2,min2,sec2;
	unsigned char day;
	hour=0;
	min=0;
	sec=0;
	
	
	r = ds3232_readdatetime_conv_int(0,&hour,&min,&sec,&date,&month,&year);
		
	fprintf_P(file_pri,PSTR("Cur: %02d.%02d.%02d %02d:%02d:%02d\n"),date,month,year,hour,min,sec);
	
	day = TimeAddSeconds(hour,min,sec,insec,&hour2,&min2,&sec2);
	
	fprintf_P(file_pri,PSTR("Alm: %02d.%02d.%02d %02d:%02d:%02d (+%d d)\n"),date,month,year,hour2,min2,sec2,day);
	
	if(day)
		fprintf_P(file_pri,PSTR("Cannot set alarm on different day\n"));
	else
		ds3232_alarm_at(date,month,year,hour2,min2,sec2);
}
*/



/**
  * @brief Real-Time Clock
  */
/*
typedef struct
{
  __IO uint32_t TR;      //!< RTC time register,                                        Address offset: 0x00
  __IO uint32_t DR;      //!< RTC date register,                                        Address offset: 0x04
  __IO uint32_t CR;      //!< RTC control register,                                     Address offset: 0x08
  __IO uint32_t ISR;     //!< RTC initialization and status register,                   Address offset: 0x0C
  __IO uint32_t PRER;    //!< RTC prescaler register,                                   Address offset: 0x10
  __IO uint32_t WUTR;    //!< RTC wakeup timer register,                                Address offset: 0x14
  __IO uint32_t CALIBR;  //!< RTC calibration register,                                 Address offset: 0x18
  __IO uint32_t ALRMAR;  //!< RTC alarm A register,                                     Address offset: 0x1C
  __IO uint32_t ALRMBR;  //!< RTC alarm B register,                                     Address offset: 0x20
  __IO uint32_t WPR;     //!< RTC write protection register,                            Address offset: 0x24
  __IO uint32_t SSR;     //!< RTC sub second register,                                  Address offset: 0x28
  __IO uint32_t SHIFTR;  //!< RTC shift control register,                               Address offset: 0x2C
  __IO uint32_t TSTR;    //!< RTC time stamp time register,                             Address offset: 0x30
  __IO uint32_t TSDR;    //!< RTC time stamp date register,                             Address offset: 0x34
  __IO uint32_t TSSSR;   //!< RTC time-stamp sub second register,                       Address offset: 0x38
  __IO uint32_t CALR;    //!< RTC calibration register,                                 Address offset: 0x3C
  __IO uint32_t TAFCR;   //!< RTC tamper and alternate function configuration register, Address offset: 0x40
  __IO uint32_t ALRMASSR;//!< RTC alarm A sub second register,                          Address offset: 0x44
  __IO uint32_t ALRMBSSR;//!< RTC alarm B sub second register,                          Address offset: 0x48
  uint32_t RESERVED7;    //!< Reserved, 0x4C
  __IO uint32_t BKP0R;   //!< RTC backup register 1,                                    Address offset: 0x50
  __IO uint32_t BKP1R;   //!< RTC backup register 1,                                    Address offset: 0x54
  __IO uint32_t BKP2R;   //!< RTC backup register 2,                                    Address offset: 0x58
  __IO uint32_t BKP3R;   //!< RTC backup register 3,                                    Address offset: 0x5C
  __IO uint32_t BKP4R;   //!< RTC backup register 4,                                    Address offset: 0x60
  __IO uint32_t BKP5R;   //!< RTC backup register 5,                                    Address offset: 0x64
  __IO uint32_t BKP6R;   //!< RTC backup register 6,                                    Address offset: 0x68
  __IO uint32_t BKP7R;   //!< RTC backup register 7,                                    Address offset: 0x6C
  __IO uint32_t BKP8R;   //!< RTC backup register 8,                                    Address offset: 0x70
  __IO uint32_t BKP9R;   //!< RTC backup register 9,                                    Address offset: 0x74
  __IO uint32_t BKP10R;  //!< RTC backup register 10,                                   Address offset: 0x78
  __IO uint32_t BKP11R;  //!< RTC backup register 11,                                   Address offset: 0x7C
  __IO uint32_t BKP12R;  //!< RTC backup register 12,                                   Address offset: 0x80
  __IO uint32_t BKP13R;  //!< RTC backup register 13,                                   Address offset: 0x84
  __IO uint32_t BKP14R;  //!< RTC backup register 14,                                   Address offset: 0x88
  __IO uint32_t BKP15R;  //!< RTC backup register 15,                                   Address offset: 0x8C
  __IO uint32_t BKP16R;  //!< RTC backup register 16,                                   Address offset: 0x90
  __IO uint32_t BKP17R;  //!< RTC backup register 17,                                   Address offset: 0x94
  __IO uint32_t BKP18R;  //!< RTC backup register 18,                                   Address offset: 0x98
  __IO uint32_t BKP19R;  //!< RTC backup register 19,                                   Address offset: 0x9C
} RTC_TypeDef;
*/
