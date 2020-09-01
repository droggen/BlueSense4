/*
 * max31343.h
 *
 *  Created on: 27 juil. 2020
 *      Author: droggen
 */

#ifndef MEGALOL_MAX31343_H_
#define MEGALOL_MAX31343_H_

#include "i2c/i2c_transaction.h"
#include "i2c.h"



// 7-bit address
#define MAX31343_ADDRESSS 0x68
// Time is updated 1 second after writing to date/time registers - delay in millisecond to wait until time is updated
#define MAX31341_WAITTIME 1050
//#define MAX31341_WAITTIME 2050

#define MAX31341_BCD2BIN(value) ((unsigned int)(((unsigned int)(value>>4))*10 + (value&0x0f)))
#define MAX31341_BIN2BCD(value) (((value/10)<<4) + (value%10))

extern void (*max31343_irqhandler)();

void max31343_defaultirqhandler();
void max31343_setirqhandler(void (*h)(void));



unsigned char max31343_isok();
unsigned char max31343_init();
void max31343_printregs(FILE *f);
unsigned char max31343_writereg(unsigned char r,unsigned char val);
unsigned char max31343_readreg(unsigned char r,unsigned char *val);
unsigned char max31343_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *weekday,unsigned char *day,unsigned char *month,unsigned char *year);
unsigned char max31343_writetime(unsigned int hour,unsigned int min,unsigned int sec);
unsigned char max31343_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year);
unsigned char max31343_writedatetime(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec);
unsigned char max31343_swrst(unsigned char reset);
unsigned char max31343_alarm_at(unsigned char day, unsigned char month, unsigned char year,unsigned char hour,unsigned char min,unsigned char sec);
unsigned char max31343_alarm_in(unsigned short insec);
unsigned char __max31343_trans_read_status_done(I2C_TRANSACTION *t);
void max31343_background_read_status();
void max31342_sync();
int max31343_get_temp();
unsigned char max31341_sqw();

#endif /* MEGALOL_MAX31343_H_ */
