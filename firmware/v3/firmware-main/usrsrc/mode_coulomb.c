#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "usrmain.h"
#include "global.h"
#include "serial.h"
#include "i2c.h"
//#include "pkt.h"
#include "wait.h"
//#include "init.h"
#include "uiconfig.h"
#include "helper.h"
#include "i2c/i2c_transaction.h"
#include "system.h"


#include "commandset.h"
#include "mode_global.h"
#include "mode_coulomb.h"
#include "ltc2942.h"
#include "mode.h"

#if ENABLEMODECOULOMB==1

const char help_cq[] ="Q[,<q>]: gets or sets the counter charge";
const char help_cr[] ="R: reset accumulated charge";
const char help_cp[] ="P,<p>: with p on 3 bits, sets the prescaler to 2^p";
const char help_ca[] ="A,<a>: temp/voltage conversion: 3=auto, 2=single voltage, 1=single temp, 0=off";
const char help_I[] = "Initialise LTC2942";
const char help_coulomb_r[] = "Continuously print coulomb info until key press";
const char help_coulomb_printreg[] = "Print LTC2942 registers";

signed long acccharge=0,accpwr=0,accuah=0,acctime=0;
unsigned long initcharge=0;

const COMMANDPARSER CommandParsersCoulomb[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'I', CommandParserCoulombInit,help_I},
	{'R', CommandParserCoulombPrintReg,help_coulomb_printreg},
	{'r', CommandParserCoulombContinuous,help_coulomb_r},
	{'Q', CommandParserCoulombCharge,help_cq},
	{'P', CommandParserCoulombPrescaler,help_cp},
	{'A', CommandParserCoulombADC,help_ca},
	{'X', CommandParserCoulombReset,help_cr},
	{'!', CommandParserQuit,help_quit}
};
unsigned char CommandParsersCoulombNum=sizeof(CommandParsersCoulomb)/sizeof(COMMANDPARSER);

unsigned char CommandParserCoulombInit(char *buffer,unsigned char size)
{
	(void)buffer; (void) size;

	ltc2942_init();

	return 0;
}

unsigned char CommandParserCoulomb(char *buffer,unsigned char size)
{
	(void)buffer; (void) size;
	CommandChangeMode(APP_MODE_COULOMB);
		
	return 0;
}
unsigned char CommandParserCoulombReset(char *buffer,unsigned char size)
{
	(void)buffer; (void) size;
	acccharge=0;
	accpwr=0;
	accuah=0;
	acctime=0;
	initcharge = ltc2942_getcharge();
	
	return 0;
}

unsigned char CommandParserCoulombADC(char *buffer,unsigned char size)
{
	(void)size;
	unsigned char rv;
	int adc;
	
	rv = ParseCommaGetInt(buffer,1,&adc);
	if(rv)
	{
		return 2;
	}
	if(ltc2942_setadc(adc))
		return 1;
	return 0;
}
unsigned char CommandParserCoulombPrescaler(char *buffer,unsigned char size)
{
	(void)size;
	unsigned char rv;
	int prescaler;
	
	rv = ParseCommaGetInt(buffer,1,&prescaler);
	if(rv)
	{
		return 2;
	}
	if(ltc2942_setprescaler(prescaler))
		return 1;
	return 0;
}

unsigned char CommandParserCoulombCharge(char *buffer,unsigned char size)
{
	(void)size;
	unsigned char rv;
	int charge;
	
	if(ParseCommaGetNumParam(buffer)==1)
	{
		rv = ParseCommaGetInt(buffer,1,&charge);
		if(rv)
		{
			return 2;
		}
		if(ltc2942_setchargectr(charge))
			return 1;
	}

	unsigned short c = ltc2942_getchargectr();
	fprintf(file_pri,"Charge: %d\n",c);


	
	return 0;
}

/*
void coulomb(unsigned short q1,unsigned short q2,unsigned short voltage,unsigned short elapsed,unsigned char prescaler)
{
	signed long dq;

	dq = q2-q1;									// dq: uncalibrated charge difference
	dq = (85*(1<<prescaler)*dq)/128;			// dq: charge difference in uAh
	
	signed long dpwr;
	dpwr = dq*voltage;							// dpwr: 
	
	
	
}*/
unsigned char CommandParserCoulombContinuous(char *buffer,unsigned char size)
{
	WAITPERIOD p=0;
	unsigned long refcharge = ltc2942_getcharge();
	initcharge=refcharge;
	acccharge=0;
	accpwr=0;
	accuah=0;
	acctime=0;
	while(1)
	{
		timer_waitperiod_ms(1000,&p);
		acctime++;

		//ltc2942_printreg(file_pri);
		unsigned long charge = ltc2942_getcharge();
		unsigned short chargectr = ltc2942_getchargectr();
		unsigned short voltage = ltc2942_getvoltage();
		unsigned short temperature = ltc2942_gettemperature();
		fprintf(file_pri,"%lds: chargectr: %u charge: %lu voltage: %u temperature: %u\n",acctime,chargectr,charge,voltage,temperature);
		fprintf(file_pri,"%lds: dQ from start/reset: %ld uAh\n",acctime,charge-initcharge);
		fprintf(file_pri,"%lds: Avg power from start/reset: %ld uW\n",acctime,ltc2942_getavgpower(initcharge,charge,voltage,acctime*1000));

		refcharge = charge;

		if(fgetc(file_pri)!=-1)
			break;
	}


	return 0;


}

unsigned char CommandParserCoulombPrintReg(char *buffer,unsigned char size)
{
	(void)buffer; (void) size;

	ltc2942_printreg(file_pri);

	return 0;
}

void mode_coulomb(void)
{
	fprintf(file_pri,"CLB>\n");


	while(1)
	{
		while(CommandProcess(CommandParsersCoulomb,CommandParsersCoulombNum));

		if(CommandShouldQuit())
			break;


		//HAL_Delay(500);

	}
	fprintf(file_pri,"<CLB\n");
}

#endif
