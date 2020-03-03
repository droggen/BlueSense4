/*
 * m24xx.h
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

#ifndef M24XX_H_
#define M24XX_H_

#include "stdio.h"

#define M24XX_ADDRESS 0x51

unsigned char m24xx_isok();
unsigned char m24xx_read(unsigned short addr16,unsigned char *d);
unsigned char m24xx_write(unsigned short addr16,unsigned char d);
void m24xx_printreg(FILE *f,unsigned from,unsigned to);

#endif /* M24XX_H_ */
