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
void AD9834Write(unsigned int word);
void AD9834InitPort(void);

#endif /* MODE_AD9834_H_ */
