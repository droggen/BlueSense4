#ifndef __POWER_H
#define __POWER_H

typedef struct {
	unsigned char valid;
	unsigned char offh,offm,offs,offday,offmonth,offyear;
	unsigned char onh,onm,ons,onday,onmonth,onyear;
	unsigned long offcharge,oncharge;
	unsigned short offvoltage,onvoltage;
	unsigned long offtime,ontime;
	char str[5][64];
} _POWERUSE_OFF;
extern _POWERUSE_OFF _poweruse_off;

#endif
