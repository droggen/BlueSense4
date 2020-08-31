/*
 * rtcwrapper.h
 *
 *  Created on: 29 août 2020
 *      Author: droggen
 */

#ifndef MEGALOL_RTCWRAPPER_H_
#define MEGALOL_RTCWRAPPER_H_

#include "max31343.h"
#include "stmrtc.h"

char *rtc_name();
unsigned char rtc_isok();
unsigned char rtc_init();
unsigned char rtc_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *weekday,unsigned char *day,unsigned char *month,unsigned char *year);
unsigned char rtc_writetime(unsigned int hour,unsigned int min,unsigned int sec);
unsigned char rtc_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year);
unsigned char rtc_writedatetime(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec);
unsigned char rtc_alarm_at(unsigned char day, unsigned char month, unsigned char year,unsigned char hour,unsigned char min,unsigned char sec);
unsigned char rtc_alarm_in(unsigned short insec);
int rtc_get_temp();
void rtc_setirqhandler(void (*h)(void));

#endif /* MEGALOL_RTCWRAPPER_H_ */
