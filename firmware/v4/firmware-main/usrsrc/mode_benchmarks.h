/*
 * mode_benchmarkcpu.h
 *
 *  Created on: 7 déc. 2019
 *      Author: droggen
 */

#ifndef MODE_BENCHMARKS_H_
#define MODE_BENCHMARKS_H_

#include <stdio.h>

void mode_benchmarkcpu(void);

unsigned char CommandParserBenchmarkCPU1(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkCPU2(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkCPUMem(char *buffer,unsigned char size);
unsigned char CommandParserBenchUSB(char *buffer,unsigned char size);
unsigned char CommandParserBenchBT(char *buffer,unsigned char size);
unsigned char CommandParserBenchITM(char *buffer,unsigned char size);
unsigned char CommandParserBenchFile(char *buffer,unsigned char size);

unsigned char CommandParserIntfWriteBench(char *buffer,unsigned char size);
unsigned char CommandParserIntfBuf(char *buffer,unsigned char size);
unsigned char CommandParserIntfStatus(char *buffer,unsigned char size);
unsigned char CommandParserIntfEvents(char *buffer,unsigned char size);
unsigned char CommandParserIntfEventsClear(char *buffer,unsigned char size);

unsigned char CommandParserBenchmarkPower(char *buffer,unsigned char size);
unsigned char CommandParserFlush(char *buffer,unsigned char size);
unsigned char CommandParserDump(char *buffer,unsigned char size);
unsigned char CommandParserInt(char *buffer,unsigned char size);
unsigned char CommandParserUSBDirect(char *buffer,unsigned char size);
unsigned char CommandParserSerInfo(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkFlush(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkLatency(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkPerf(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkTimer(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkTimer2(char *buffer,unsigned char size);
unsigned char CommandParserBenchmarkNumberConversion(char *buffer,unsigned char size);
void benchmark_interface(FILE *fbench,FILE *finfo);

#endif /* MODE_BENCHMARKS_H_ */
