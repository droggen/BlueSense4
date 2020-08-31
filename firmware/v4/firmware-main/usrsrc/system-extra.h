#ifndef __SYSTEM_EXTRA_H
#define __SYSTEM_EXTRA_H

#include "power.h"

extern volatile unsigned char usb_must_enable;

#define BATTERY_THRESHOLD_CONNECTED 4500

extern unsigned char system_enable_lcd;

#define BATTERY_VERYVERYLOW 3350
#define BATTERY_VERYLOW 3600
#define BATTERY_LOW 3700



unsigned char system_batterystat(unsigned char unused);



/*void system_callback_battery_sample_init(void);
unsigned char system_callback_battery_sample(unsigned char);*/
unsigned short system_getbattery(void);
/*unsigned long system_getbatterytime(void);*/
void system_off(void);
/*void system_power_low(void);
void system_power_normal(void);
void system_adcpu_off(void);
void system_adcpu_on(void);*/
unsigned char *system_getdevicename(void);
//unsigned char system_getrtcint(void);
void system_settimefromrtc(void);
/*void system_storepoweroffdata(void);*/
/*void system_storepoweroffdata2(void);*/
void system_storepowerdata();
void system_loadpowerdata(_POWERUSE_DATA *pu);
void system_buttonchange(unsigned char newpc);
unsigned long system_perfbench_motionread(unsigned long mintime);
unsigned long system_perfbench(unsigned long mintime);
unsigned char system_issdpresent();
void system_info_cpu();

#endif
