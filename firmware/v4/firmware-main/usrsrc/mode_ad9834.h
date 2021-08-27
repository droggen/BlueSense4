/*
*/

// Daniel Roggen, 2021
// AD9834 stuff

#ifndef MODE_AD9834_H_
#define MODE_AD9834_H_

unsigned char CommandParserModeAD9834(char *buffer,unsigned char size);
void mode_ad9834(void);
unsigned char CommandParserAD9834InitPort(char *buffer,unsigned char size);
unsigned char CommandParserAD9834Reset(char *buffer,unsigned char size);
unsigned char CommandParserAD9834Frq(char *buffer,unsigned char size);
unsigned char CommandParserAD9834Phase(char *buffer,unsigned char size);
unsigned char CommandParserAD9834Write(char *buffer,unsigned char size);
unsigned char CommandParserAD9834Test1(char *buffer,unsigned char size);
unsigned char CommandParserAD9834InitAD9834(char *buffer,unsigned char size);
void AD9834Write(unsigned int word);
void AD9834InitPort(void);
void AD9834InitControlReg(int ensqw);
void AD9834InitFP0FP1(unsigned int frq0, unsigned short ph0, unsigned int frq1, unsigned short ph1);
void AD9834InitFP0(unsigned int frq0, unsigned short ph0);
void AD9834InitF0(unsigned int frq0);
void AD9834SetFrqPin(unsigned x);
unsigned char CommandParserAD9834Sqw(char *buffer,unsigned char size);

unsigned long AD9834Frq2Reg(unsigned long frq);
unsigned long AD9834Reg2Frq(unsigned long reg);
unsigned char CommandParserAD9834ModulateFSK(char *buffer,unsigned char size);
unsigned char CommandParserAD9834InitUart(char *buffer,unsigned char size);
unsigned char CommandParserAD9834UartWrite(char *buffer,unsigned char size);
unsigned char CommandParserAD9834CalcFrq(char *buffer,unsigned char size);
unsigned char CommandParserAD9834ModulateFSK2(char *buffer,unsigned char size);
void CommandParserAD9834ModulateFSKTerminal_Help(void);
unsigned CheckFSKInit();
void AD9834ModulateFSKSendSequence(char *seq,unsigned len,unsigned delay);
unsigned char CommandParserAD9834ModulateFSKTerminal(char *buffer,unsigned char size);
unsigned char CommandParserAD9834ModulateFSKSequence(char *buffer,unsigned char size);
unsigned char CommandParserAD9834ModulateFSKSine(char *buffer,unsigned char size);
unsigned char CommandParserAD9834ModulateFSKTune(char *buffer,unsigned char size);

#endif /* MODE_AD9834_H_ */
