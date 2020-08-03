/*
 * max31343.c
 *
 *  Created on: 27 juil. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include "i2c/megalol_i2c.h"
#include "max31343.h"
#include "i2c.h"
#include "usrmain.h"
#include "global.h"



// Return: 1 if ok
unsigned char max31343_isok()
{
	unsigned char d;

	unsigned char rv = i2c_readreg(MAX31343_ADDRESSS,0,&d);

	if(rv!=0)
		return 0;

	fprintf("Status: %02X\n",d);

	return 0;
}
