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

void benchmark_interface(FILE *fbench,FILE *finfo);

#endif /* MODE_BENCHMARKS_H_ */
