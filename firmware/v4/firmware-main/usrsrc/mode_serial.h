/*
 * mode_serial.h
 *
 *  Created on: 1 sept. 2020
 *      Author: droggen
 */

#ifndef MODE_SERIAL_H_
#define MODE_SERIAL_H_

unsigned char CommandParserModeSerial(char *buffer,unsigned char size);
unsigned char CommandParserModeSerialReset(char *buffer,unsigned char size);
unsigned char CommandParserModeSerialLast(char *buffer,unsigned char size);
void mode_serialtest(void);
unsigned char CommandParserSerTestSendEnter(char *buffer,unsigned char size);
unsigned char CommandParserSerTestSendLeave(char *buffer,unsigned char size);
unsigned char CommandParserSerTestSendWithoutConnection(char *buffer,unsigned char size);
unsigned char CommandParserSerTestSendH(char *buffer,unsigned char size);
unsigned char CommandParserSerTestRead(char *buffer,unsigned char size);
unsigned char CommandParserSerTestInit(char *buffer,unsigned char size);
unsigned char CommandParserSerTestSendCr(char *buffer,unsigned char size);
unsigned char CommandParserSerTestSendNl(char *buffer,unsigned char size);
unsigned char CommandParserSerTestDMAEn(char *buffer,unsigned char size);
unsigned char CommandParserSerTestDMARead(char *buffer,unsigned char size);
unsigned char CommandParserSerTestDMAClear(char *buffer,unsigned char size);
void serial_uart_dma_rx(unsigned char serialid);
unsigned char CommandParserSerTestDMATx(char *buffer,unsigned char size);
unsigned char CommandParserSerTestDMATx2(char *buffer,unsigned char size);
unsigned char CommandParserSerTestDMATx3(char *buffer,unsigned char size);
unsigned char CommandParserSerTestDMAIsEn(char *buffer,unsigned char size);
unsigned char CommandParserSerTestWrite(char *buffer,unsigned char size);
unsigned char CommandParserSerTestWrite2(char *buffer,unsigned char size);
unsigned char CommandParserSerTestWrite3(char *buffer,unsigned char size);

#endif /* MODE_SERIAL_H_ */
