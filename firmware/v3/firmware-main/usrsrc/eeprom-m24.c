/*
 * eeprom.c
 *
 *  Created on: 26 Sep 2019
 *      Author: droggen
 */

#include <eeprom-m24.h>
#include "m24xx.h"
#include "global.h"
#include <stdio.h>

void eeprom_write_byte(unsigned addr,uint8_t d8)
{
	m24xx_write(addr,d8);
}
void eeprom_write_word(unsigned addr,uint16_t d16)
{
	m24xx_write(addr,d16&0xff);
	m24xx_write(addr+1,d16>>8);
}
void eeprom_write_dword(unsigned addr,uint32_t d32)
{
	m24xx_write(addr,d32&0xff);
	m24xx_write(addr+1,d32>>8);
	m24xx_write(addr+2,d32>>16);
	m24xx_write(addr+3,d32>>24);
}
/******************************************************************************
	function: eeprom_write_buffer_try_nowait
*******************************************************************************
	Writes a buffer to the eeprom in a single write operation.

	The maximum size of a buffer is limited by the I2C transaction subsystem
	and is 16-3=13 bytes.

	Writes must not cross a page boundary (32 bytes)

	Interrupts:
		Suitable for call from interrupts. Does not guarantee success of operation

	Parameters:
		buffer	-		Pointer to the buffer with the data to write
		n		-		Number of bytes to write

	Returns:
		0		-		Success
		1		-		Error

******************************************************************************/
unsigned char eeprom_write_buffer_try_nowait(unsigned addr,unsigned char *buffer, unsigned n)
{
	if(n>13)
		return 1;

	return m24xx_write_buffer_nowait(addr,buffer,n);
}


unsigned char eeprom_read_byte(unsigned addr,unsigned char checkvalid,unsigned char defval)
{
	unsigned char b;

	m24xx_read(addr,&b);

	if(checkvalid)
	{
		if(b==0xff)
		{
			b = defval;
			// Write back
			eeprom_write_byte(addr,b);
		}
	}

	return b;
}
unsigned short eeprom_read_word(unsigned addr,unsigned char checkvalid,unsigned short defval)
{
	unsigned char b[2];
	unsigned short d;

	for(int i=0;i<2;i++)
		m24xx_read(addr+i,&b[i]);

	d=(unsigned short)b[0] | (((unsigned short)b[1])<<8);

	if(checkvalid)
	{
		if(d==0xffff)
		{
			d = defval;
			// Write back
			eeprom_write_word(addr,d);
		}
	}

	return d;
}
unsigned eeprom_read_dword(unsigned addr,unsigned char checkvalid,unsigned defval)
{
	unsigned char b[4];
	unsigned d;

	for(int i=0;i<4;i++)
	{
		m24xx_read(addr+i,&b[i]);
	}

	d=((unsigned)b[0]) | (((unsigned)b[1])<<8) | (((unsigned)b[2])<<16) | (((unsigned)b[3])<<24);

	if(checkvalid)
	{
		if(d==0xffffffff)
		{
			d = defval;
			// Write back
			eeprom_write_dword(addr,d);
		}
	}

	return d;
}
