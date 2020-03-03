/*
 * command_mputest.h
 *
 *  Created on: 23 sept. 2019
 *      Author: droggen
 */

#ifndef COMMAND_MPUTEST_H_
#define COMMAND_MPUTEST_H_

unsigned char CommandParserMPUTest1(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest2(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest3(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest4(char *buffer,unsigned char size);
void mpu_writereg_wait_poll_block(unsigned char reg,unsigned char v);
unsigned char mpu_readreg_wait_poll_block(unsigned char reg);
unsigned char CommandParserMPUInit1(char *buffer,unsigned char size);
unsigned char CommandParserMPUInit2(char *buffer,unsigned char size);
unsigned char CommandParserMPUInit3(char *buffer,unsigned char size);
unsigned char CommandParserMPUInit4(char *buffer,unsigned char size);
unsigned char CommandParserMPUInitSPICube(char *buffer,unsigned char size);
unsigned char CommandParserMPUSS(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestPrintModes(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestPoweron(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestPoweroff(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestPowerosc(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestSSosc(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestXchange(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestPrintSPIRegisters(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestReadRegistersPoll(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestReadRegistersInt(char *buffer,unsigned char size);
unsigned char CommandParserMPUTest5(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestPrintMPURegisters(char *buffer,unsigned char size);
unsigned char CommandParserMPUTestWriteRegistersPoll(char *buffer,unsigned char size);


#endif /* COMMAND_MPUTEST_H_ */
