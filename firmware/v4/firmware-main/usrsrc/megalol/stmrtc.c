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
	
		* stm32_init:						Initialisation of the STM32 RTC
		* stmrtc_setirqhandler				Sets a function to be called every second
		* stmrtc_readdatetime_conv_int		Get date and time
		* stmrtc_writetime					Write time
		* stmrtc_writedate					Write date
		* stmrtc_writedatetime				Write date and time

		
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
	//fprintf(file_pri,"A\n");

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
	//stmrtc_enterinit();							// Does not work when this is called?

	// Alarm A can only be set in init mode, or if alarm disabled and flag
	//fprintf(file_pri,"Whiling rtc isr\n");
	while(!(RTC->ISR&1));
	//fprintf(file_pri,"ALMAWF: %d\n",RTC->ISR&1);

	//HAL_Delay(1000);
	//fprintf(file_pri,"About to set interrupt mask\n");

	// Set alarm A mask every second
	//              3       2       1
	//              1       3       5       7
	RTC->ALRMAR = 0b10000000100000001000000010000000;

	//fprintf(file_pri,"After interrupt mask\n");

	//HAL_Delay(1000);
	//fprintf(file_pri,"About to enable interrupt\n");

	// Enable alarm A and interrupt
	RTC->CR|=1<<8;	// ALRAE
	RTC->CR|=1<<12;	// ALRAIE

	//fprintf(file_pri,"After enable interrupt\n");

	// Enable the EXTI interrupts
#if 0
	// STM32F401
	EXTI->IMR|=(1<<17);		// Enable line 17
	EXTI->RTSR|=(1<<17);	// Rising edge line 17
#endif
#if 1
	//HAL_Delay(1000);
	//fprintf(file_pri,"About to EXTI18\n");
	// STM32L4: RTC_ALARM RTC alarms through EXTI line 18 interrupts (sec. 13.3)
	EXTI->IMR1|=(1<<18);		// Enable line 18
	EXTI->RTSR1|=(1<<18);		// Rising edge line 18
	//fprintf(file_pri,"After EXTI18\n");
#endif


	//fprintf(file_pri,"Before protect\n");
	// Protect
	stmrtc_protect();
	//fprintf(file_pri,"After protect\n");

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

	return;
	//fprintf(file_pri,"Before geting time\n");

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
	function: stmrtc_writetime
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
void stmrtc_writetime(unsigned int hour,unsigned int min,unsigned int sec)
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
	stmrtc_writedate
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
void stmrtc_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year)
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
	function: stmrtc_writedatetime
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
