#ifndef MODE_DAC_H_
#define MODE_DAC_H_



void mode_dactest(void);
unsigned char CommandParserDACTest1(char *buffer,unsigned char size);
unsigned char CommandParserDACStart(char *buffer,unsigned char size);
unsigned char CommandParserDACStop(char *buffer,unsigned char size);
unsigned char CommandParserDACSetVal(char *buffer,unsigned char size);
unsigned char CommandParserDACGetVal(char *buffer,unsigned char size);
unsigned char CommandParserDACSquare(char *buffer,unsigned char size);
unsigned char CommandParserDACTimer(char *buffer,unsigned char size);
unsigned char CommandParserModeDAC(char *buffer,unsigned char size);
unsigned char CommandParserDACBenchmark(char *buffer,unsigned char size);
unsigned char CommandParserDACBenchmarkCPUOverhead(char *buffer,unsigned char size);

#endif
