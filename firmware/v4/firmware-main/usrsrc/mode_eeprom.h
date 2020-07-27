/*
 * mode_eeprom.h
 *
 *  Created on: 16 Sep 2019
 *      Author: droggen
 */

#ifndef MODE_EEPROM_H_
#define MODE_EEPROM_H_

unsigned char CommandParserEEPROMRead(char *buffer,unsigned char size);
unsigned char CommandParserEEPROMWrite(char *buffer,unsigned char size);
void mode_eeprom(void);

#endif /* MODE_EEPROM_H_ */
