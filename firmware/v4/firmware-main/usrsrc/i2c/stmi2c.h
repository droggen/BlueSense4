/*
   MEGALOL - ATmega LOw level Library
	I2C Module
   Copyright (C) 2010-2015:
         Daniel Roggen, droggen@gmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef __I2CSENSOR_H
#define __I2CSENSOR_H


//#include "i2c_internal.h"


//typedef unsigned char(*I2C_CALLBACK)(struct _I2C_TRANSACTION *);






/******************************************************************************
*******************************************************************************
GENERAL & WRAPPER
*******************************************************************************
******************************************************************************/
void i2c_printconfig(void);
//void i2c_init(void);
void i2c_init(void);
void i2c_deactivateinterrupts();
void i2c_deinit(void);
unsigned char i2c_readreg_i(unsigned char addr7,unsigned char reg,unsigned char *val);
unsigned char i2c_readregs_i(unsigned char addr7,unsigned char reg,unsigned char n,unsigned char *val);
unsigned char i2c_writereg_i(unsigned char addr7,unsigned char reg,unsigned char val);
unsigned char i2c_writeregs_i(unsigned char addr7,unsigned char reg,unsigned char *val,unsigned char n);


/******************************************************************************
*******************************************************************************
DIRECT ACCESS   DIRECT ACCESS   DIRECT ACCESS   DIRECT ACCESS   DIRECT ACCESS   
*******************************************************************************
******************************************************************************/

//void i2c_scan(unsigned long long *h,unsigned long long *l);
//void i2c_check(void);

/*unsigned char i2c_readreg_poll(unsigned char addr7,unsigned char reg,unsigned char *val);
unsigned char i2c_readregs_poll(unsigned char addr7,unsigned char reg,unsigned char n,unsigned char *val);
unsigned char i2c_writereg_poll(unsigned char addr7,unsigned char reg,unsigned char val);*/

/******************************************************************************
*******************************************************************************
INTERRUPT-DRIVEN ACCESS   INTERRUPT-DRIVEN ACCESS   INTERRUPT-DRIVEN ACCESS   
*******************************************************************************
******************************************************************************/
/*void i2c_transaction_setup(I2C_TRANSACTION *t,unsigned char addr7,unsigned char mode,unsigned char stop,unsigned char n);
unsigned char i2c_transaction_execute(I2C_TRANSACTION *trans,unsigned char blocking);
void i2c_transaction_cancel(void);
// 

unsigned char i2c_transaction_queue(unsigned char n,unsigned char blocking,...);
unsigned char i2c_transaction_getqueued(void);
void i2c_transaction_removenext(void);*/

/*unsigned char i2c_readreg_int(unsigned char addr7,unsigned char reg,unsigned char *val);
unsigned char i2c_readreg_int_cb(unsigned char addr7,unsigned char reg,unsigned char *val,I2C_CALLBACK cb);
unsigned char i2c_readregs_int(unsigned char addr7,unsigned char reg,unsigned char n,unsigned char *val);
unsigned char i2c_readregs_int_cb(unsigned char addr7,unsigned char reg,unsigned char n,unsigned char *val,I2C_CALLBACK cb,void *user);
unsigned char i2c_writereg_int(unsigned char addr7,unsigned char reg,unsigned char val);*/



#endif



