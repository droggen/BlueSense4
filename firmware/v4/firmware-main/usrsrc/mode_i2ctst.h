/*
 * mode-i2ctst.h
 *
 *  Created on: 30 sept. 2019
 *      Author: droggen
 */

#ifndef MODE_I2CTST_H_
#define MODE_I2CTST_H_

void mode_i2ctst();

unsigned char CommandParserI2CTstPrintReg(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstInitFrq(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstInit10(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstInit100(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstInit400(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstInit2(char *buffer,unsigned char size);
unsigned char CommandParserI2CTst1(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstDeinitInt(char *buffer,unsigned char size);
unsigned char CommandParserI2CTst2(char *buffer,unsigned char size);
unsigned char CommandParserI2CTst3(char *buffer,unsigned char size);
unsigned char CommandParserI2CTst4(char *buffer,unsigned char size);
unsigned char CommandParserI2CTst5(char *buffer,unsigned char size);
unsigned char CommandParserI2CTst6(char *buffer,unsigned char size);
unsigned char CommandParserI2CReset(char *buffer,unsigned char size);
unsigned char CommandParserI2CWrite(char *buffer,unsigned char size);
unsigned char CommandParserI2CRead(char *buffer,unsigned char size);
unsigned char CommandParserI2CTstReset(char *buffer,unsigned char size);

#endif /* MODE_I2CTST_H_ */
