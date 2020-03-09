/*
 * mode_eeprom.h
 *
 *  Created on: 16 Sep 2019
 *      Author: droggen
 */

#ifndef MODE_INTERFACE_H_
#define MODE_INTERFACE_H_


void mode_interface();
unsigned char CommandParserIntfWriteBench(char *buffer,unsigned char size);
unsigned char CommandParserIntfBuf(char *buffer,unsigned char size);
unsigned char CommandParserIntfStatus(char *buffer,unsigned char size);
unsigned char CommandParserIntfEvents(char *buffer,unsigned char size);

#endif /* MODE_INTERFACE_H_ */
