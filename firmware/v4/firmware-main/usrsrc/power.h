#ifndef __POWER_H
#define __POWER_H

typedef struct {
	unsigned char valid;
	//unsigned char offh,offm,offs,offday,offmonth,offyear;
	//unsigned char onh,onm,ons,onday,onmonth,onyear;
	unsigned long charge;
	unsigned short voltage;
	unsigned long time;
} _POWERUSE_DATA;
extern _POWERUSE_DATA _poweruse_data_atoff,_poweruse_data_aton;
//char _poweruse_data_str[5][64];

void power_print_off_on_info();

#endif
