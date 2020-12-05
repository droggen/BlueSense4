/*
 * mode_modulation.h
 *
 *  Created on: 10 oct. 2020
 *      Author: droggen
 */

#ifndef MODE_MODULATION_H_
#define MODE_MODULATION_H_

void mode_modulation(void);
unsigned char CommandParserModeModulation(char *buffer,unsigned char size);
void v21_init(unsigned bps);
void vpsk_init(unsigned bps);
void v21_siggen(unsigned short *buffer,unsigned n);
void v21_siggen_8n1(unsigned short *buffer,unsigned n);
void vpsk_siggen_8n1(unsigned short *buffer,unsigned n);
void vpsk_siggen_test(unsigned short *buffer,unsigned n);
unsigned v21_getnext(unsigned char *byte);
void v21_siggen_bit(unsigned short *buffer,unsigned n);
unsigned char CommandParserModulationBit(char *buffer,unsigned char size);
unsigned char CommandParserModulationPhase(char *buffer,unsigned char size);
unsigned char CommandParserModulationStop(char *buffer,unsigned char size);
unsigned char CommandParserV21(char *buffer,unsigned char size);
unsigned char CommandParserV21Bench(char *buffer,unsigned char size);
unsigned char CommandParserV21Bench2(char *buffer,unsigned char size);
unsigned char CommandParserV21Demod(char *buffer,unsigned char size);
unsigned char CommandParserV21DemodProd(char *buffer,unsigned char size);



#endif /* MODE_MODULATION_H_ */
