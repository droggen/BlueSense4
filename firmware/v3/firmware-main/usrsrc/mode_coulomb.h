#ifndef __MODE_COULOMB_H
#define __MODE_COULOMB_H

#define ENABLEMODECOULOMB 1

#include "command.h"

extern const char help_cq[];
extern const char help_cc[];
extern const char help_cr[];


void mode_coulomb(void);

unsigned char CommandParserCoulomb(char *buffer,unsigned char size);
unsigned char CommandParserCoulombInit(char *buffer,unsigned char size);
unsigned char CommandParserCoulombContinuous(char *buffer,unsigned char size);
unsigned char CommandParserCoulombCharge(char *buffer,unsigned char size);
//unsigned char CommandParserCoulombControl(char *buffer,unsigned char size);
unsigned char CommandParserCoulombPrescaler(char *buffer,unsigned char size);
unsigned char CommandParserCoulombADC(char *buffer,unsigned char size);
unsigned char CommandParserCoulombReset(char *buffer,unsigned char size);
unsigned char CommandParserCoulombPrintReg(char *buffer,unsigned char size);

#endif

