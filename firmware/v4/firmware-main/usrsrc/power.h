#ifndef __POWER_H
#define __POWER_H



typedef struct {
	unsigned char valid;
	unsigned long charge;
	unsigned short voltage;
	unsigned long time;
} _POWERUSE_DATA;
extern _POWERUSE_DATA _poweruse_data_atoff,_poweruse_data_aton;
//char _poweruse_data_str[5][64];

void power_print_off_on_info();
unsigned char power_get_state(_POWERUSE_DATA *pud);
void power_measure_start();
signed long power_measure_stop(unsigned long *timems);
signed long power_compute(_POWERUSE_DATA *pudstart,_POWERUSE_DATA *pudend,unsigned long *timems,signed long *pwruw);

#endif
