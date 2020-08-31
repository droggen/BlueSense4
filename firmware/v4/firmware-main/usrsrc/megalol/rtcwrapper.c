/*
 * rtcwrapper.c
 *
 *  Created on: 29 août 2020
 *      Author: droggen
 */

#include <stdio.h>
#include "global.h"
#include "rtcwrapper.h"

/******************************************************************************
	file: rtcwrapper
*******************************************************************************
	Common functions directed either to the internal RTC or the external RTC.

******************************************************************************/

// Sanity check of compile settings

// RTC: internal or external must be defined
#ifndef ENABLE_RTC_INTERNAL
#error ENABLE_RTC_INTERNAL must be set to 0 or 1
#endif
#ifndef ENABLE_RTC_EXTERNAL
#error ENABLE_RTC_EXTERNAL must be set to 0 or 1
#endif
// RTC: both cannot be one
#if ENABLE_RTC_INTERNAL==1 && ENABLE_RTC_EXTERNAL==1
#error Only one of ENABLE_RTC_INTERNAL or ENABLE_RTC_EXTERNAL must be set to 1
#endif
// RTC: both cannot be zero
#if ENABLE_RTC_INTERNAL==0 && ENABLE_RTC_EXTERNAL==0
#error Only one of ENABLE_RTC_INTERNAL or ENABLE_RTC_EXTERNAL must be set to 0
#endif


char *rtc_name()
{
	static char name[10];
#if ENABLE_RTC_INTERNAL==1
	strcpy(name,"internal");
#endif
#if ENABLE_RTC_EXTERNAL==1
	strcpy(name,"MAX31343");
#endif
	return name;
}
unsigned char rtc_isok()
{
#if ENABLE_RTC_INTERNAL==1
	return 1;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_isok();
#endif
}
unsigned char rtc_init()
{
#if ENABLE_RTC_INTERNAL==1
	stmrtc_init();
	return 0;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_init();
#endif
}
unsigned char rtc_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *weekday,unsigned char *day,unsigned char *month,unsigned char *year)
{
#if ENABLE_RTC_INTERNAL==1
	return stmrtc_readdatetime_conv_int(sync,hour,min,sec,weekday,day,month,year);
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_readdatetime_conv_int(sync,hour,min,sec,weekday,day,month,year);
#endif
}
unsigned char rtc_writetime(unsigned int hour,unsigned int min,unsigned int sec)
{
#if ENABLE_RTC_INTERNAL==1
	stmrtc_writetime(hour,min,sec);
	return 0;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_writetime(hour,min,sec);
#endif
}
unsigned char rtc_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year)
{
#if ENABLE_RTC_INTERNAL==1
	stmrtc_writedate(weekday,day,month,year);
	return 0;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_writedate(weekday,day,month,year);
#endif

}
unsigned char rtc_writedatetime(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec)
{
#if ENABLE_RTC_INTERNAL==1
	stmrtc_writedatetime(weekday,day,month,year,hour,min,sec);
	return 0;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_writedatetime(weekday,day,month,year,hour,min,sec);
#endif
}
unsigned char rtc_alarm_at(unsigned char day, unsigned char month, unsigned char year,unsigned char hour,unsigned char min,unsigned char sec)
{
#if ENABLE_RTC_INTERNAL==1
#warning rtc_alarm_at not implemented with internal RTC
	fprintf(file_pri,"Alarm wakeup not implemented with internal RTC\n");
	return 1;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_alarm_at(day,month,year,hour,min,sec);
#endif

}
unsigned char rtc_alarm_in(unsigned short insec)
{
#if ENABLE_RTC_INTERNAL==1
#warning rtc_alarm_in not implemented with internal RTC
	fprintf(file_pri,"Alarm wakeup not implemented with internal RTC\n");
	return 1;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_alarm_in(insec);
#endif

}
int rtc_get_temp()
{
#if ENABLE_RTC_INTERNAL==1
#warning rtc_get_temp not implemented with internal RTC
	return 0;
#endif
#if ENABLE_RTC_EXTERNAL==1
	return max31343_get_temp();
#endif
}
void rtc_setirqhandler(void (*h)(void))
{
#if ENABLE_RTC_INTERNAL==1
#warning rtc_get_temp not implemented with internal RTC
	stmrtc_setirqhandler(h);
#endif
#if ENABLE_RTC_EXTERNAL==1
	max31343_setirqhandler(h);
#endif
}

