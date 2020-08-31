/*
 * command_datetime.c
 *
 *  Created on: 19 Jul 2019
 *      Author: droggen
 */

#include <stdio.h>

#include "global.h"
#include "helper.h"

//#include "stm32f4xx_ll_rtc.h"
//#include "stm32f4xx_hal_rtc.h"
#include "stmrtc.h"
#include "main.h"
#include "usrmain.h"
#include "wait.h"
#include "serial_itm.h"


/******************************************************************************
	function: CommandParserTime
*******************************************************************************
	Either displays the RTC time if no argument is provided, or sets the
	RTC time to the hhmmss argument.
	The local time is synchronised to the RTC (epoch).

	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid

******************************************************************************/
unsigned char CommandParserTime(char *buffer,unsigned char size)
{
	unsigned char h,m,s;

	if(size==0)
	{
		// Query time
		rtc_readdatetime_conv_int(1,&h,&m,&s,0,0,0,0);
		fprintf(file_pri,"%02d:%02d:%02d\n",h,m,s);
		return 0;
	}
	// Set time
	if(size!=7 || buffer[0]!=',')
		return 2;

	buffer++;	// Skip comma


	// Check the digits are in range
	if(checkdigits(buffer,6))
		return 2;

	str2xxyyzz(buffer,&h,&m,&s);

	fprintf(file_pri,"Time: %02d:%02d:%02d\n",h,m,s);

	if(h>23 || m>59 || s>59)
	{
		return 2;
	}

	// Execute
	unsigned char rv = rtc_writetime(h,m,s);
	if(rv)
		return 1;
	// Synchronise local time to RTC time
	system_settimefromrtc();
	return 0;
}
unsigned char CommandParser1(char *buffer,unsigned char size)
{


	return 0;
}


unsigned char CommandParserDate(char *buffer,unsigned char size)
{
	unsigned char wd,d,m,y;
	if(size==0)
	{
		// Query date

		rtc_readdatetime_conv_int(0,0,0,0,&wd,&d,&m,&y);

		fprintf(file_pri,"(%d) %02d.%02d.%02d\n",wd,d,m,y);
		return 0;
	}
	// Set date
	if(size!=7 || buffer[0]!=',')
		return 2;

	buffer++;	// Skip comma

	// Check the digits are in range
	if(checkdigits(buffer,6))
		return 2;

	str2xxyyzz(buffer,&d,&m,&y);

	fprintf(file_pri,"Date: %02d.%02d.%02d\n",d,m,y);

	if(d>31 || d<1 || m>12 || m<1 || y>99)
	{
		return 2;
	}

	unsigned char rv = rtc_writedate(1,d,m,y);
	if(rv)
		return 1;

	// Synchronise local time to RTC time
	system_settimefromrtc();
	return 0;
}
unsigned char CommandParserTimeTest(char *buffer,unsigned char size)
{
	/*int cmd;

	fprintf(file_pri,"CommandParserTimeTest\n");

	unsigned char rv = ParseCommaGetInt((char*)buffer,1,&cmd);
	if(rv)
		return 2;

	switch(cmd)
	{
	case 0:
	{
		fprintf(file_pri,"Press any key to interrupt\n");
		while(1)
		{
			unsigned char day,month,year,h,m,s;
			// Query date

			stmrtc_readdatetime_conv_int(0,&h,&m,&s,&day,&month,&year);
			fprintf(file_pri,"%02d.%02d.%02d %02d:%02d:%02d\n",day,month,year,h,m,s);
			HAL_Delay(250);

			short c=fgetc(file_pri);

			if(c!=-1)
				break;
		}
	}
	case 1:
	{
		fprintf(file_pri,"Backup registers:\n");
		for(int i=0;i<20;i++)
		{
			unsigned int r = LL_RTC_BAK_GetRegister(RTC, LL_RTC_BKP_DR0+i);
			fprintf(file_pri,"%02X: %08X\n",i,r);
		}
	}
	default:
		fprintf(file_pri,"Unknown command\n");
		return 2;
	}*/
	return 0;
}
unsigned char CommandParserRTCTest(char *buffer,unsigned char size)
{
	return 0;
}
unsigned char CommandParserRTCReadBackupRegisters(char *buffer,unsigned char size)
{
	fprintf(file_pri,"Backup registers:\n");
	for(int i=0;i<20;i++)
	{
		fprintf(file_pri,"%02X: %08X\n",i,stmrtc_getbkpreg(i));

	}
	return 0;
}
unsigned char CommandParserRTCReadAllRegisters(char *buffer,unsigned char size)
{
	stmrtc_printregs(file_pri);
	return 0;
}
unsigned char CommandParserRTCWriteBackupRegister(char *buffer,unsigned char size)
{
	unsigned addr,word;


	if(ParseCommaGetUnsigned(buffer,2,&addr,&word))
		return 2;

	fprintf(file_pri,"RTC write %08X in backup register %02X\n",word,addr);


	stmrtc_setbkpreg(addr,word);

	return 0;
}
unsigned char CommandParserRTCWriteRegister(char *buffer,unsigned char size)
{
	unsigned addr,word;


	if(ParseCommaGetUnsigned(buffer,2,&addr,&word))
		return 2;

	fprintf(file_pri,"RTC write %08X at address %02X\n",word,addr);

	unsigned *a = RTC;

	*(a+addr) = word;

	return 0;
}
unsigned char CommandParserRTCInit(char *buffer,unsigned char size)
{
	fprintf(file_pri,"Init RTC\n");
	stmrtc_init();
	return 0;
}

void rtc_alarm_callback()
{
	itmprintf("alarm callback\n");
}

void printalm()
{
	/*fprintf(file_pri,"AlarmOutEvent: %d\n",LL_RTC_GetAlarmOutEvent(RTC));
	fprintf(file_pri,"HourFmt: %d\n",LL_RTC_GetHourFormat(RTC));
	fprintf(file_pri,"ShadowRegBypassEnabled: %d\n",LL_RTC_IsShadowRegBypassEnabled(RTC));
	fprintf(file_pri,"AsynchPrescaler: %d\n",LL_RTC_GetAsynchPrescaler(RTC));
	fprintf(file_pri,"SynchPrescaler: %d\n",LL_RTC_GetSynchPrescaler(RTC));
	uint32_t m = LL_RTC_ALMA_GetMask(RTC);
	fprintf(file_pri,"Mask: %08X ",m);
	if(m&LL_RTC_ALMA_MASK_ALL)
		fprintf(file_pri,"all ");
	if(m&LL_RTC_ALMA_MASK_SECONDS)
		fprintf(file_pri,"seconds ");
	if(m&LL_RTC_ALMA_MASK_MINUTES)
		fprintf(file_pri,"min ");
	if(m&LL_RTC_ALMA_MASK_HOURS)
		fprintf(file_pri,"hours ");
	if(m&LL_RTC_ALMA_MASK_DATEWEEKDAY)
		fprintf(file_pri,"date/weekday ");
	if(m&LL_RTC_ALMA_MASK_NONE)
		fprintf(file_pri,"none ");
	fprintf(file_pri,"\n");


	fprintf(file_pri,"Alarm A day: %d\n",__LL_RTC_CONVERT_BCD2BIN(LL_RTC_ALMA_GetDay(RTC)));
	fprintf(file_pri,"Alarm A weekday: %d\n",LL_RTC_ALMA_GetWeekDay(RTC));
	fprintf(file_pri,"Alarm A hms: %d:%d:%d\n",__LL_RTC_CONVERT_BCD2BIN(LL_RTC_ALMA_GetHour(RTC)),
			__LL_RTC_CONVERT_BCD2BIN(LL_RTC_ALMA_GetMinute(RTC)),
			__LL_RTC_CONVERT_BCD2BIN(LL_RTC_ALMA_GetSecond(RTC))
			);*/
}
unsigned char CommandParserRTCTest1(char *buffer,unsigned char size)
{
	// Info about systick:
	//
	fprintf(file_pri,"Do something\n");

	//SysTick_Config();
	fprintf(file_pri,"Systick ctrl: %08X\n",SysTick->CTRL);
	fprintf(file_pri,"Systick load: %u\n",SysTick->LOAD);
	fprintf(file_pri,"Systick val: %u\n",SysTick->VAL);
	fprintf(file_pri,"Systick calib: %u\n",SysTick->CALIB);


	HAL_Delay(1000);


		while(1)
		{
			for(int i=0;i<100;i++)
			{
				fprintf(file_pri,"Systick val: %u %u\n",SysTick->VAL,dsystick_getus());
				//HAL_Delay(1);
			}

			HAL_Delay(500);

			if(fgetc(file_pri)!=-1)
				break;
		}

	return 0;
}


unsigned char CommandParserRTCShowDateTime(char *buffer,unsigned char size)
{
	fprintf(file_pri,"RCC->BDCR %08X\n",RCC->BDCR);


	while(1)
	{
		//LL_RTC_ReadReg(RTC, DR);		// Does nothing (second doesn't change)
		//LL_RTC_ReadReg(RTC, RTC_SSR);
		// GetTime or GetDate without printf get optimised out
		//unsigned int time=0,date=0;
		//date = LL_RTC_DATE_Get(RTC);	// Works: seconds do change (wihout bypass) if GetDate called (and printf to prevent it being optimised out). Seems date must be called before time.
		//time = LL_RTC_TIME_Get(RTC);	// KO: seconds don't change, despite this call.

		//LL_RTC_WaitForSynchro(RTC);




		//fprintf(file_pri,"%u %d ",date,time);

		unsigned char hour,min,sec,day,month,year,weekday;

		stmrtc_readdatetime_conv_int(0,&hour,&min,&sec,&weekday,&day,&month,&year);

		//stmrtc_readdatetime_conv_int_new(1,&hour,&min,&sec,&weekday,&day,&month,&year);

		fprintf(file_pri,"%02u:%02u:%02u (%d) %02d.%02d.%02d\n",hour,min,sec,weekday,day,month,year);

		//uint32_t ss = LL_RTC_TIME_GetSubSecond(RTC);
		//uint32_t s = LL_RTC_TIME_GetSecond(RTC);
		//fprintf(file_pri,"%lu %lu\n",s,ss);

		//fprintf(file_pri,"%08X\n",RTC->ISR);

		HAL_Delay(200);

		if(fgetc(file_pri)!=-1)
			break;
	}

	return 0;
}
unsigned char CommandParserRTCShowDateTimeMs(char *buffer,unsigned char size)
{
	unsigned char hour,min,sec,day,month,year,weekday;
	char str[64];

	stmrtc_readdatetime_conv_int(0,&hour,&min,&sec,&weekday,&day,&month,&year);
	stmrtc_formatdatetime(str,weekday,day,month,year,hour,min,sec);
	fprintf(file_pri,"%s\n",str);



	while(1)
	{
		for(int i=0;i<50;i++)
		{
			unsigned long ms = timer_ms_get();
			unsigned long us = timer_us_get();
			fprintf(file_pri,"%ums %uus\n",ms,us);
		}

		HAL_Delay(500);

		if(fgetc(file_pri)!=-1)
			break;
	}

	return 0;
}
void extien()
{
	// STM32F4: RTC is on EXTI 17
	// STM32L4: RTC is on EXTI 18




#if 0
	// STM32F4
	fprintf(file_pri,"IMR: %08X\n",EXTI->IMR);
	EXTI->IMR|=(1<<17);		// Enable line 17
	EXTI->RTSR|=(1<<17);	// Rising edge line 17
	fprintf(file_pri,"IMR: %08X\n",EXTI->IMR);
#endif
#if 1
	// STM32L4
	fprintf(file_pri,"IMR1: %08X\n",EXTI->IMR1);
	EXTI->IMR1|=(1<<18);	// Enable line 18
	EXTI->RTSR1|=(1<<18);	// Rising edge line 18
	fprintf(file_pri,"IMR1: %08X\n",EXTI->IMR1);
#endif




/*
	// PD0 is connected to EXTI_Line0
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	// Enable interrupt
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	// Interrupt mode
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	// Triggers on rising and falling edge
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	// Add to EXTI
	EXTI_Init(&EXTI_InitStruct);
	*/
}

/*
To enable the RTC Alarm interrupt, the following sequence is required:
1. Configure and enable the EXTI Line 17 in interrupt mode and select the rising edge sensitivity.
2. Configure and enable the RTC_Alarm IRQ channel in the NVIC.
3. Configure the RTC to generate RTC alarms (Alarm A or Alarm B).
 */
unsigned char CommandParserRTCTest2(char *buffer,unsigned char size)
{

	while(1)
	{
		fprintf(file_pri,"%u (%u)",SysTick->VAL,HAL_GetTick());
		dsystick_clear();
		fprintf(file_pri," %u (%u)\n",SysTick->VAL,HAL_GetTick());

		HAL_Delay(100);

		if(fgetc(file_pri)!=-1)
			break;
	}

	return 0;
}
unsigned char CommandParserRTCTest3(char *buffer,unsigned char size)
{
	unsigned t1[2048],t2[2048],t3[2048];

	while(1)
	{
		for(int i=0;i<2048;i++)
		{
			t1[i] = HAL_GetTick();
			t2[i] = dsystick_getus();
			t3[i] = timer_us_get();
		}
		for(int i=0;i<2048;i++)
			fprintf(file_pri,"%d %u (%u) %u\n",i,t1[i],t2[i],t3[i]);
		fprintf(file_pri,"\n");

		HAL_Delay(1332);

		if(fgetc(file_pri)!=-1)
			break;
	}

	return 0;
}
unsigned char CommandParserRTCTest4(char *buffer,unsigned char size)
{

	while(1)
	{
		fprintf(file_pri,"%u\n",timer_ms_get());

		HAL_Delay(124);

		if(fgetc(file_pri)!=-1)
			break;
	}

	return 0;
}

unsigned char CommandParserRTCProtect(char *buffer,unsigned char size)
{
	int protect;

	if(ParseCommaGetNumParam(buffer)!=1 && ParseCommaGetNumParam(buffer)!=0)
		return 2;

	if(ParseCommaGetNumParam(buffer)==1)
	{
		if(ParseCommaGetInt(buffer,1,&protect))
			return 2;

		if(protect)
		{
			fprintf(file_pri,"Enable protection\n");
			stmrtc_protect();
		}
		else
		{
			fprintf(file_pri,"Disable protection\n");
			stmrtc_unprotect();
		}
	}
	//fprintf(file_pri,"DBP bit in PWR_CR now: %d\n",(PWR->CR>>8)&0b1);	// STM32F4
	fprintf(file_pri,"DBP bit in PWR_CR now: %d\n",(PWR->CR1>>8)&0b1);	// STM32L4
	//fprintf(file_pri,"PWR_CR now: %08X\n",PWR->CR);		// STM32F4
	fprintf(file_pri,"PWR_CR now: %08X\n",PWR->CR1);		// STM32L4


	return 0;
}

unsigned char CommandParserRTCBypass(char *buffer,unsigned char size)
{
	int bypass;

	if(ParseCommaGetInt(buffer,1,&bypass))
		return 2;

	if(bypass)
	{
		fprintf(file_pri,"Enable shadow bypass\n");
		stmrtc_enablebypass(1);
	}
	else
	{
		fprintf(file_pri,"Disable shadow bypass\n");
		stmrtc_enablebypass(0);
	}
	fprintf(file_pri,"CR: %08X (BYPSHAD: %d)\n",RTC->CR,(RTC->CR&0b100000)?1:0);

	return 0;

}
unsigned char CommandParserRTCPrescaler(char *buffer,unsigned char size)
{
	int async,sync;

	if(ParseCommaGetNumParam(buffer)!=2 && ParseCommaGetNumParam(buffer)!=0)
		return 2;

	if(ParseCommaGetNumParam(buffer)==2)
	{
		if(ParseCommaGetInt(buffer,2,&async,&sync))
			return 2;


		fprintf(file_pri,"Setting prescaler: async=%d sync=%d\n",async,sync);
		stmrtc_setprescalers(async,sync);
	}

	fprintf(file_pri,"Prescaler now: async=%d\n",(RTC->PRER>>16)&0b1111111);
	fprintf(file_pri,"Prescaler now: sync=%d\n",RTC->PRER&0b111111111111111);

	return 0;
}

unsigned char CommandParserRTCInitTimeFromRTC(char *buffer,unsigned char size)
{
	// Set time from RTC
	system_settimefromrtc();
	return 0;
}



