#ifndef __STMRTC_H
#define __STMRTC_H

#include <stdio.h>
#include <string.h>

// Select which API to use
#define STMRTC_HAL 0
#define STMRTC_LL 1


#define STMRTC_BIN2BCD(__VALUE__) (uint32_t)((((__VALUE__) / 10U) << 4U) | ((__VALUE__) % 10U))
#define STMRTC_BCD2BIN(__VALUE__) (uint32_t)(((uint8_t)((__VALUE__) & (uint8_t)0xF0U) >> (uint8_t)0x4U) * 10U + ((__VALUE__) & (uint8_t)0x0FU))

extern void (*stmrtc_irqhandler)();

void stmrtc_init(void);
void stmrtc_defaultirqhandler();
void stmrtc_setirqhandler(void (*h)(void));
void stmrtc_printregs(FILE *f);
void stmrtc_protect();
void stmrtc_unprotect();
int stmrtc_enterinit();
void stmrtc_leaveinit();
void stmrtc_setprescalers(unsigned asyncprescaler,unsigned syncprescaler);
void stmrtc_setprescalers_unp(unsigned asyncprescaler,unsigned syncprescaler);
unsigned stmrtc_getbkpreg(unsigned reg);
void stmrtc_setbkpreg(unsigned reg,unsigned val);
unsigned stmrtc_waspowerlost(void);
void stmrtc_set24hr();
void stmrtc_writetime(unsigned int hour,unsigned int min,unsigned int sec);
void stmrtc_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year);
void stmrtc_writedatetime(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec);
unsigned char stmrtc_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *weekday,unsigned char *day,unsigned char *month,unsigned char *year);
void stm_sync();
void stmrtc_enablebypass(unsigned bypass);
void stmrtc_formatdatetime(char *buffer,unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec);
void stmrtc_resetdefaults();
/*void ds3232_printreg(FILE *file);
void ds3232_sync_fall(void);
void ds3232_sync_rise(void);

unsigned char ds3232_readtimeregisters(unsigned char *regs);
void ds3232_convtime(unsigned char *val,unsigned char *hour,unsigned char *min,unsigned char *sec);
void ds3232_convdate(unsigned char *val,unsigned char *day,unsigned char *month,unsigned char *year);


void ds3232_off(void);
void ds3232_alarm_in(unsigned short insec);

unsigned char ds3232_readtemp(signed short *temp);
unsigned char ds3232_readtemp_int_cb(void (*cb)(unsigned char,signed short));
unsigned char ds3232_readtemp_int_cb_cb(I2C_TRANSACTION *t);



// I2C transactional
unsigned char ds3232_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *day,unsigned char *month,unsigned char *year);

unsigned char ds3232_readdate_conv_int(unsigned char *date,unsigned char *month,unsigned char *year);
*/

#endif
