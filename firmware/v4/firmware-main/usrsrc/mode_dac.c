#include <stdio.h>
#include <string.h>

#include "main.h"
#include "global.h"
#include "wait.h"
#include "system.h"
#include "system-extra.h"
#include "helper.h"
#include "commandset.h"
#include "uiconfig.h"
#include "mode.h"
#include "mode_main.h"
#include "mode_dac.h"
#include "stmdac.h"
//#include "stm32l4xx_hal_dac.h"
#include "dac.h"

const COMMANDPARSER CommandParsersDACTST[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},

	{0,0,"---- General functions  ----"},

	{'S', CommandParserDACStart,"Start DAC in DC mode"},
	{'s', CommandParserDACStop,"Stop DAC (waveform and DC)"},
	{'V', CommandParserDACSetVal,"V,<val> Set value in DC mode"},
	{'v', CommandParserDACGetVal,"Get value in DC mdoe"},
	{0,0,"---- Test functions  ----"},
	{'K', CommandParserDACBenchmark,"Benchmark waveform generation speed (max sample rate"},
	{'k', CommandParserDACBenchmarkCPUOverhead,"Benchmark CPU overhead during waveform generation"},
	/*{0,0,"---- Developer  ----"},
	{'1', CommandParserDACTest1,"stuff"},
	{'Q', CommandParserDACSquare,"Q,<amplitude>,<dt> Generate square wave (polling)"},
	{'2', CommandParserDACTimer,"Start timer"},*/
};
unsigned char CommandParsersDACTSTNum=sizeof(CommandParsersDACTST)/sizeof(COMMANDPARSER);



void mode_dactest(void)
{
	mode_run("DACTST",CommandParsersDACTST,CommandParsersDACTSTNum);
}



unsigned char CommandParserDACTest1(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"DAC triangle\n");


	HAL_DAC_Start(&hdac1,DAC_CHANNEL_1);


	HAL_DAC_Stop(&hdac1,DAC_CHANNEL_1);

	return 0;
}

unsigned char CommandParserDACStart(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"DAC start DC mode\n");

	//HAL_StatusTypeDef s = HAL_DAC_Start(&hdac1,DAC_CHANNEL_1);
	//fprintf(file_pri,"%d\n",s);
	stmdac_init(0,0);

	return 0;
}
unsigned char CommandParserDACStop(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"DAC stop\n");

	//HAL_StatusTypeDef s = HAL_DAC_Stop(&hdac1,DAC_CHANNEL_1);
	//fprintf(file_pri,"%d\n",s);
	stmdac_deinit();


	return 0;
}
unsigned char CommandParserDACSetVal(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	int value;

	if(ParseCommaGetInt(buffer,1,&value))
		return 2;

	fprintf(file_pri,"DAC set DC value: %d\n",value);
	stmdac_setval(value);





	return 0;
}

unsigned char CommandParserDACGetVal(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	unsigned value = stmdac_getval();
	fprintf(file_pri,"DAC current DC value: %d\n",value);


	return 0;
}
unsigned char CommandParserDACSquare(char *buffer,unsigned char size)
{
	int value,dt;

	if(ParseCommaGetInt(buffer,2,&value,&dt))
		return 2;

	HAL_DAC_Start(&hdac1,DAC_CHANNEL_1);

	fprintf(file_pri,"Generating square wave until keypress: A=%d dt=%d\n",value,dt);

	while(1)
	{
		if(fgetc(file_pri)!=-1)
			break;

		HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_1,DAC_ALIGN_12B_R,value);
		HAL_Delay(dt);
		HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_1,DAC_ALIGN_12B_R,0);
		HAL_Delay(dt);

	}
	fprintf(file_pri,"Done\n");

	HAL_DAC_Stop(&hdac1,DAC_CHANNEL_1);

	return 0;
}
/*unsigned char CommandParserDACTimer(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	stmdac_init();


	return 0;
}*/

unsigned char CommandParserModeDAC(char *buffer,unsigned char size)
{
	// Parse parameters
	unsigned int frq[5]={0xffff,0xffff,0xffff,0xffff,0xffff};
	unsigned int vol[5]={0xffff,0xffff,0xffff,0xffff,0xffff};
	unsigned int sr;


	unsigned char n;

	n = ParseCommaGetNumParam(buffer);

	//fprintf(file_pri,"n: %d\n",n);

	if(n == 0)
	{
		// Terminate generation
		fprintf(file_pri,"DAC: stopping\n");
		stmdac_deinit();
		if(stmdac_stat_totframes()==0)
			fprintf(file_pri,"==========");
		fprintf(file_pri,"Generated samples: %lu; lost samples: %lu\n",stmdac_stat_totframes(),stmdac_stat_lostframes());
		return 0;
	}

	if(n<3 || n>11)
	{
		fprintf(file_pri,"Error: invalid number of parameters\n");
		return 2;
	}

	if(!(n&1))
	{
		fprintf(file_pri,"Error: each waveform must be a pair of frq,vol\n");
		return 2;
	}



	//unsigned char rv = ParseCommaGetInt(buffer,n,&frq[0],&vol[0]);
	unsigned char rv = ParseCommaGetInt(buffer,n,&sr,&frq[0],&vol[0],&frq[1],&vol[1],&frq[2],&vol[2],&frq[3],&vol[3],&frq[4],&vol[4]);
	if(rv)
	{
		fprintf(file_pri,"Cannot parse parameters\n");
		return 2;
	}
	//fprintf(file_pri,"rv: %d\n",rv);
	//fprintf(file_pri,"Params: sr %d - %d %d  %d %d  %d %d  %d %d  %d %d\n",sr,frq[0],vol[0],frq[1],vol[1],frq[2],vol[2],frq[3],vol[3],frq[4],vol[4]);



	// Clear all waveforms
	stmdac_deinit();

	// Start generating at the specified sample rate with the built-in cosine generator
	stmdac_init(sr,stm_dac_cosinegenerator_siggen);

	// Initialise the cosine waveform generator
	// Note that stmdac_init must be called before, as this stores the sample rate (effective) in internal variables used by the cosine generator to find the phase increment.
	stm_dac_cosinegenerator_init();
	for(int i=0;i<(n-1)/2;i++)
	{
		fprintf(file_pri,"DAC Waveform %d: frq=%d; vol=%d\n",i,frq[i],vol[i]);
		if(stm_dac_cosinegenerator_addwaveform(frq[i],vol[i]))
			fprintf(file_pri,"Error adding waveform %d.\n",i);
	}





	return 0;
}
unsigned char CommandParserDACBenchmark(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	stmdac_benchmaxspeed();

	return 0;
}
unsigned char CommandParserDACBenchmarkCPUOverhead(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	stmdac_benchcpuoverhead();

	return 0;
}
