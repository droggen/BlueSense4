/*
 * eeprom.h
 *
 *  Created on: 26 Sep 2019
 *      Author: droggen
 */

#ifndef EEPROM_M24_H_
#define EEPROM_M24_H_

#include <stdint.h>

void eeprom_write_byte(unsigned addr,uint8_t d8);
void eeprom_write_word(unsigned addr,uint16_t d16);
void eeprom_write_dword(unsigned addr,uint32_t d32);
unsigned char eeprom_write_buffer_try_nowait(unsigned addr,unsigned char *buffer, unsigned n);

unsigned char eeprom_read_byte(unsigned addr,unsigned char checkvalid,unsigned char defval);
unsigned short eeprom_read_word(unsigned addr,unsigned char checkvalid,unsigned short defval);
unsigned eeprom_read_dword(unsigned addr,unsigned char checkvalid,unsigned defval);

#endif /* EEPROM_M24_H_ */
