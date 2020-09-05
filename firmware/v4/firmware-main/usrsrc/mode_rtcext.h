/*
 * mode_rtcext.h
 *
 *  Created on: 29 août 2020
 *      Author: droggen
 */

#ifndef MODE_RTCEXT_H_
#define MODE_RTCEXT_H_


void mode_rtcext(void);
unsigned char CommandParserRTCExtInit(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtReadAllRegisters(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtWriteRegister(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtReadRegister(char *buffer,unsigned char size);

unsigned char CommandParserRTCExtReadDateTime(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtPollDateTime(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtWriteDateTime(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtWriteAlarm(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtWriteAlarmIn(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtSwRst(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtBgdReadStatus(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtTest1(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtTest2(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtTemp(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtBootStatus(char *buffer,unsigned char size);
unsigned char CommandParserRTCExtPowerSel(char *buffer,unsigned char size);

#endif /* MODE_RTCEXT_H_ */
