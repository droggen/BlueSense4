/*
 * mode_adc.h
 *
 *  Created on: 11 janv. 2020
 *      Author: droggen
 */

#ifndef MODE_ADC_H_
#define MODE_ADC_H_

void mode_adc(void);


unsigned char CommandParserADCTest(char *buffer,unsigned char size);
unsigned char CommandParserADCTest1(char *buffer,unsigned char size);
unsigned char CommandParserADCTest2(char *buffer,unsigned char size);
unsigned char CommandParserADCTest3(char *buffer,unsigned char size);
unsigned char CommandParserADCTestDMA(char *buffer,unsigned char size);
unsigned char CommandParserADCTestB(char *buffer,unsigned char size);
unsigned char CommandParserADCRead(char *buffer,unsigned char size);
unsigned char CommandParserADCTestInit(char *buffer,unsigned char size);
unsigned char CommandParserADCTestReg(char *buffer,unsigned char size);
unsigned char CommandParserADCTestInit2(char *buffer,unsigned char size);
unsigned char CommandParserADCTestTimer(char *buffer,unsigned char size);
unsigned char CommandParserADCTestTimerInit(char *buffer,unsigned char size);
unsigned char CommandParserADCTestComplete(char *buffer,unsigned char size);
unsigned char CommandParserADCReadCont(char *buffer,unsigned char size);
unsigned char CommandParserADCAcquire(char *buffer,unsigned char size);
unsigned char CommandParserADCBench(char *buffer,unsigned char size);

unsigned char CommandParserADCTestFgetc(char *buffer,unsigned char size);


unsigned char CommandParserADCFastStart(char *buffer,unsigned char size);
unsigned char CommandParserADCFastStop(char *buffer,unsigned char size);
unsigned char CommandParserADCFastBenchmark(char *buffer,unsigned char size);
unsigned char CommandParserADCFastBenchmarkMem(char *buffer,unsigned char size);
unsigned char CommandParserADCFastAcq(char *buffer,unsigned char size);


#endif /* MODE_ADC_H_ */
