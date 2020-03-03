/*
 * megalol_i2c.h
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

#ifndef MEGALOL_I2C_H_
#define MEGALOL_I2C_H_

unsigned char i2c_readreg(unsigned char addr7,unsigned char reg,unsigned char *val);
unsigned char i2c_readreg_a16(unsigned char addr7,unsigned short reg,unsigned char *val,unsigned char dostop);
unsigned char i2c_readregs(unsigned char addr7,unsigned char reg,unsigned char *val,unsigned char n);
unsigned char i2c_writereg(unsigned char addr7,unsigned char reg,unsigned char val);
unsigned char i2c_writereg_a16(unsigned char addr7,unsigned short reg,unsigned char val);
unsigned char i2c_writeregs(unsigned char addr7,unsigned char reg,unsigned char *val,unsigned char n);

#endif /* MEGALOL_I2C_H_ */
