/*
 * command_datetime.h
 *
 *  Created on: 19 Jul 2019
 *      Author: droggen
 */

#ifndef COMMAND_DATETIME_H_
#define COMMAND_DATETIME_H_

unsigned char CommandParser1(char *buffer,unsigned char size);
unsigned char CommandParserTime(char *buffer,unsigned char size);
unsigned char CommandParserDate(char *buffer,unsigned char size);
unsigned char CommandParserTimeTest(char *buffer,unsigned char size);
unsigned char CommandParserRTCTest(char *buffer,unsigned char size);
unsigned char CommandParserRTCReadBackupRegisters(char *buffer,unsigned char size);
unsigned char CommandParserRTCReadAllRegisters(char *buffer,unsigned char size);
unsigned char CommandParserRTCWriteBackupRegister(char *buffer,unsigned char size);
unsigned char CommandParserRTCWriteRegister(char *buffer,unsigned char size);
unsigned char CommandParserRTCTest1(char *buffer,unsigned char size);
unsigned char CommandParserRTCTest2(char *buffer,unsigned char size);
unsigned char CommandParserRTCTest3(char *buffer,unsigned char size);
unsigned char CommandParserRTCTest4(char *buffer,unsigned char size);
unsigned char CommandParserRTCBypass(char *buffer,unsigned char size);
unsigned char CommandParserRTCInit(char *buffer,unsigned char size);
unsigned char CommandParserRTCPrescaler(char *buffer,unsigned char size);
unsigned char CommandParserRTCProtect(char *buffer,unsigned char size);
unsigned char CommandParserRTCShowDateTime(char *buffer,unsigned char size);
unsigned char CommandParserRTCShowDateTimeMs(char *buffer,unsigned char size);
unsigned char CommandParserRTCInitTimeFromRTC(char *buffer,unsigned char size);

#endif /* COMMAND_DATETIME_H_ */
