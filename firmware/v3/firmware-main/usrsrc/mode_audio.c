#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//#include "arm_math.h"

#include "usrmain.h"
#include "global.h"
#include "serial.h"
#include "i2c.h"
#include "pkt.h"
#include "wait.h"
//#include "init.h"
#include "uiconfig.h"
#include "helper.h"
#include "i2c/i2c_transaction.h"
#include "system.h"

#include "helper_num.h"
#include "commandset.h"
#include "mode_global.h"
#include "mode_audio.h"
#include "usrmain.h"
#include "main.h"
#include "mode.h"
#include "dfsdm.h"
#include "stmdfsdm.h"
#include "ufat.h"

//#include "i2s.h"
//#include "spi.h"
//#include "pdm2pcm_glo.h"
//#include "pdm2pcm.h"
//#include "stm32f4xx_hal_rcc.h"

/*
Settings:
	DFSDM1 clock: 20MHz
	Oversampling: 1024
	Clock divider: 12

Time 5000 samples:
	fastsinc: 5123630
	1 order: 1282198
	2 order: 2564364
	3 order: 3844158
	5 order: 6406599


20MHz, sinc5, 64x, 1x, /12: 407386us 407400us: 81.4772us/sample
20MHz, sinc5, 64x, 1x, /12, systemclock: 977625 us -> 195.525us/sample

*/
/*
	Filter 2 = channel 2 = rising edge = U1 = LR=0 (below SDA/SCL on extension)
	Filter 3 = channel 3 = falling edge = U2 = LR=VCC (next to AVCC on extension)

	Internal: LR=0 -> Rising edge -> channel 5 = filter 0 = rising
	Test with internal pull-up on AUD_DAT (internal mic): filter 0 = rising is OK. filter 1 = falling does not work.
	-> Internal mic "rising" edge.
 */
/*
	Audio sample rate

	Fast mode = 0 (i.e. allows to convert multiple channels)

	Sample rate = fckin / [Fosr*(Iosr-1 + Ford) + (Ford+1) ]

	filtdiv = [Fosr*(Iosr-1 + Ford) + (Ford+1) ]


	Default settings:
		Sinc3: Ford = 3
		Iosr=1
		Fosr = to be defined

		filtdiv = [Fosr*3+4]
		fckin = 20 MHz / divider

		20MHz/divider/[Fosr*3+4] = 8000


		divider = 10 -> fmic = 2MHz
			Fosr = 82:	SR = 2MHz / [82*3 + 4] = 8000
			Fosr = 83:	SR = 2MHz / [83*3 + 4] = 7905
			Fosr = 81:	SR = 2MHz / [81*3 + 4] = 8097

	-> hdfsdm1_filter0.Init.FilterParam.Oversampling = 82;
	-> hdfsdm1_channel5.Init.OutputClock.Divider = divider;


		fmic: [1.2;3.25]MHz

		SR = 8000 -> divide 2500


*/
int aud_int_channel6=1;

//#define AUDBUFSIZ 4096
#define AUDBUFSIZ 128
//#define AUDBUFSIZ (STM_DFSMD_BUFFER_SIZE*2)				// Twice size for copy on half full and full
int audbuf[AUDBUFSIZ];


const char help_aud_1[] = "Tests";
const char help_aud_reg[] = "Print I2S registers";
const char help_aud_init[] = "Initialisation";
const char help_aud_filter_init[] = "Initialisation of filter";
const char help_aud_rec[] = "R,<r> with r=0 for 0%; r=1 for 100%; r=2 for 50%; r=3 for 25%; r=4 for 75%";
const char help_aud_printrec[] = "Print current recording";


const COMMANDPARSER CommandParsersAudio[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},

	{0,0,"---- General help: internal microphone ----"},
	{0,0,"Use I to initialise a sampling mode followed by L to log audio to a file. Sampling is done by DMA."},
	{0,0,"Custom sampling is initialised with i, followed by either: 1) D for DMA acquisition and L to log data; or 2) S for polling acquisition and P to acquire/print data"},
	{0,0,"---- General help: external microphone ----"},
	{0,0,"External microphone data is acquired by polling with S,1 followed by p."},
	{0,0,"---- Audio test functions ----"},
	{'I',CommandParserAudInit,"I[,<mode>] Initialise internal audio acquisition. Without parameters prints available modes."},
	{'L',CommandParserAudLog,"L,<lognum> Log audio to lognum. Use I (or i and D) beforehand to start sampling)"},
	{'K',CommandParserAudBench,"Benchmark internal mic acquisition overhead"},
	{0,0,"---- Developer functions ----"},
	{'R',CommandParserAudReg,"Print DFSDM registers"},
	{'O',CommandParserAudInternalOffset,"Measure internal mic zero offset (issue S beforehand)"},
	{'Z',CommandParserAudZeroCalibrate,"Zero-calibration of internal mic"},
	{'z',CommandParserAudZeroCalibrateClear,"Clears zero-calibration of internal mic"},
	{'D',CommandParserAudDMA,"Start sampling with DMA acquisition"},
	{'d',CommandParserAudDMAStop,"Stop sampling with DMA acquisition"},
	{'S',CommandParserAudStart,"S[,<ext>] Start sampling with polling acquisition on internal mic or external mic when ext=1"},
	{'s',CommandParserAudStop,"s[,<ext>] Stop sampling with polling acquisition on internal mic or external mic when ext=1"},

	//{'t',CommandParserAudTest,"Test frame buffers"},


	{'A',CommandParserAudDMAAcquire,"Audio acquisition via DMA (use I, or i and D, beforehand to start sampling)"},
	{'a',CommandParserAudDMAAcquireStat,"Audio acquisition via DMA and print statistics after keypress"},
	{'P',CommandParserAudPoll,"Audio acquisition via polling on internal mic (use S beforehand to start sampling)"},
	{'p',CommandParserAudPollExtStereo,"Audio acquisition via polling on external mic (use S,1 beforehand to start sampling)"},

	{'i',CommandParserAudInitDetailed,"i,<order>,<fosr>,<isr>,<div>,<rightshift> Low-level initialisation of audio acquisition (overrides I)"},
	{'F',CommandParserAudFFT,"FFT on internal mic"},



	/*{'1',CommandParserAudPoll2,"Poll"},

	{'2',CommandParserAudPoll3,"2,<filter> with filter 0..3. Filter 0,1=internal, 2,3=external. Poll "},


	{'4',CommandParserAudPoll4,"Poll"},
	{'5',CommandParserAudPoll5,"Poll"},*/





};
unsigned char CommandParsersAudioNum=sizeof(CommandParsersAudio)/sizeof(COMMANDPARSER);



void mode_audio(void)
{
	mode_run("AUD",CommandParsersAudio,CommandParsersAudioNum);
}


unsigned char CommandParserAudStart(char *buffer,unsigned char size)
{
	(void)size;
	int ext=0;

	if(ParseCommaGetNumParam(buffer)>1)
		return 2;
	if(ParseCommaGetNumParam(buffer)==1)
	{
		if(ParseCommaGetInt(buffer,1,&ext))
			return 2;
	}
	if(ext<0 || ext>1)
		return 2;

	/*

	aud_int_channel6 = channel;

	fprintf(file_pri,"Using channel: %d %s\n",aud_int_channel6,aud_int_channel6?"ch6":"ch5");

	// Filter 0: channel 5
	// Filter 1: channel 6
	fprintf(file_pri,"Start internal\n");*/

	fprintf(file_pri,"Start audio acquisition for polling on %s microphone\n",ext?"external":"internal");

	if(ext)
	{
		HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter2);			// External left/right channels
		HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter3);			// External left/right channels
	}
	else
	{
		HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter0);			// filter 0 is internal mic filter
		// HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter1);		// filter 1 is not used
	}


	return 0;
}
unsigned char CommandParserAudStop(char *buffer,unsigned char size)
{
	(void)size;
	int ext=0;

	if(ParseCommaGetNumParam(buffer)>1)
		return 2;
	if(ParseCommaGetNumParam(buffer)==1)
	{
		if(ParseCommaGetInt(buffer,1,&ext))
			return 2;
	}
	if(ext<0 || ext>1)
		return 2;

	fprintf(file_pri,"Stop audio acquisition for polling on %s microphone\n",ext?"external":"internal");

	if(ext)
	{
		HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter2);			// External left/right channels
		HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter3);			// External left/right channels
	}
	else
	{
		HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter0);			// filter 0 is internal mic filter
		// HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter1);		// filter 1 is not used
	}


	return 0;
}
unsigned char CommandParserAudPoll(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	unsigned maxsample=10000;
	int data[maxsample];
	unsigned long dt = stm_dfsdm_acq_poll_internal(data,maxsample);

	for(int i=0;i<maxsample;i++)
	{
		fprintf(file_pri,"%d\n",data[i]);
	}
	unsigned long long sr = (unsigned long long)maxsample*1000000/dt;


	fprintf(file_pri,"%% Acquisition time for %u samples: %lu us. Sample rate: %lu\n",maxsample,dt,(unsigned long)sr);



	return 0;
}
unsigned char CommandParserAudPoll2(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;


	DFSDM_Filter_HandleTypeDef *filter;

	if(aud_int_channel6)
		filter = &hdfsdm1_filter0;
	else
		filter = &hdfsdm1_filter2;

	HAL_StatusTypeDef s1 = HAL_DFSDM_FilterPollForRegConversion(filter,1000);
	HAL_StatusTypeDef s2 = HAL_DFSDM_FilterPollForRegConversion(filter,1000);
	HAL_StatusTypeDef s3 = HAL_DFSDM_FilterPollForRegConversion(filter,1000);

	fprintf(file_pri,"Conv (ch6: %d) %u %u %u\n",aud_int_channel6,s1,s2,s3);


	unsigned long t1 = timer_us_get();
	for(int i=0;i<10000;i++)
	{
		HAL_DFSDM_FilterPollForRegConversion(filter,1000);
	}
	unsigned long t2 = timer_us_get();
	fprintf(file_pri,"Conv (ch6: %d): %lu us\n",aud_int_channel6,t2-t1);




	return 0;
}
unsigned char CommandParserAudPoll3(char *buffer,unsigned char size)
{
//	(void)size; (void)buffer;

	int f;
	ParseCommaGetInt(buffer,1,&f);

	DFSDM_Filter_HandleTypeDef *filters[4] = {&hdfsdm1_filter0,&hdfsdm1_filter1,&hdfsdm1_filter2,&hdfsdm1_filter3};
	int filtersnum[4] = {0,1,2,3};
	DFSDM_Channel_HandleTypeDef *channels[4] = {&hdfsdm1_channel5,&hdfsdm1_channel6,&hdfsdm1_channel2,&hdfsdm1_channel3};
	int channelsnum[4] = {5,6,2,3};

	if(f<0 || f>4)
		return 2;

	int maxsample=5000;
	int data[maxsample];

	DFSDM_Filter_HandleTypeDef *filter = filters[f];


	unsigned long t1 = timer_us_get();
	for(int i=0;i<maxsample;i++)
	{
		HAL_DFSDM_FilterPollForRegConversion(filter,1000);
		//for(int c=0;c<8;c++)
		int c;
		int v = HAL_DFSDM_FilterGetRegularValue(filter,(uint32_t*)&c);
		//fprintf(file_pri,"%d %d\n",v,c);
		data[i] = v;

	}
	unsigned long t2 = timer_us_get();


	for(int i=0;i<maxsample;i++)
	{
		//for(int c=0;c<8;c++)
			//fprintf(file_pri,"%d ",data[i][c]);
		//fprintf(file_pri,"\n");
		fprintf(file_pri,"%d\n",data[i]);

	}

	fprintf(file_pri,"Conv (filter %d) time: %lu us\n",f,t2-t1);

	fprintf(file_pri,"Channel states:\n");
	for(int i=0;i<4;i++)
		fprintf(file_pri,"\tChannel %d: %d\n",channelsnum[i],HAL_DFSDM_ChannelGetState(channels[i]));
	fprintf(file_pri,"Filter states:\n");
	for(int i=0;i<4;i++)
		fprintf(file_pri,"\tFilter %d: %d\n",filtersnum[i],HAL_DFSDM_FilterGetState(filters[i]));


	return 0;
}
unsigned char CommandParserAudPollExtStereo(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int maxsample=10000;
	int data[maxsample][2];

	DFSDM_Filter_HandleTypeDef *filter[2];


	filter[0] = &hdfsdm1_filter2;
	filter[1] = &hdfsdm1_filter3;


	unsigned long t1 = timer_us_get();
	for(int i=0;i<maxsample;i++)
	{
		for(int c=0;c<2;c++)
			HAL_DFSDM_FilterPollForRegConversion(filter[c],1000);

		int ch;
		for(int c=0;c<2;c++)
		{
			int v = HAL_DFSDM_FilterGetRegularValue(filter[c],(uint32_t *)&ch);
			data[i][c] = v;
		}

	}
	unsigned long t2 = timer_us_get();
	unsigned long dt = t2-t1;


	for(int i=0;i<maxsample;i++)
	{
		fprintf(file_pri,"%d %d\n",data[i][0],data[i][1]);
	}
	unsigned long long sr = (unsigned long long)maxsample*1000000/dt;
	fprintf(file_pri,"%% Acquisition time for %u samples: %lu us. Sample rate: %lu\n",maxsample,dt,(unsigned long)sr);


	return 0;
}

unsigned char CommandParserAudPoll4(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int maxsample=10000;

	unsigned long t1 = timer_us_get();
	for(int i=0;i<maxsample;i++)
	{
		HAL_DFSDM_FilterPollForRegConversion(&hdfsdm1_filter0,1000);
		HAL_DFSDM_FilterPollForRegConversion(&hdfsdm1_filter2,1000);
		int c1;
		int v1 = HAL_DFSDM_FilterGetRegularValue(&hdfsdm1_filter0,(uint32_t *)&c1);
		int c2;
		int v2 = HAL_DFSDM_FilterGetRegularValue(&hdfsdm1_filter2,(uint32_t *)&c2);
		fprintf(file_pri,"%d %d %d %d\n",v1,c1,v2,c2);


	}
	unsigned long t2 = timer_us_get();



	fprintf(file_pri,"Conv: %lu us\n",t2-t1);

//	fprintf(file_pri,"Channel state: %d\n",HAL_DFSDM_ChannelGetState(&hdfsdm1_channel6));

	return 0;
}
unsigned char CommandParserAudPoll5(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int maxsample=10000;
	int data[maxsample];

	unsigned long t1 = timer_us_get();
	for(int i=0;i<maxsample;i++)
	{
		HAL_DFSDM_FilterPollForRegConversion(&hdfsdm1_filter1,1000);
		//for(int c=0;c<8;c++)
		int c;
		int v = HAL_DFSDM_FilterGetRegularValue(&hdfsdm1_filter1,(uint32_t *)&c);
		//fprintf(file_pri,"%d %d\n",v,c);
		data[i] = v;

	}
	unsigned long t2 = timer_us_get();


	for(int i=0;i<maxsample;i++)
	{
		//for(int c=0;c<8;c++)
			//fprintf(file_pri,"%d ",data[i][c]);
		//fprintf(file_pri,"\n");
		fprintf(file_pri,"%d\n",data[i]);

	}

	fprintf(file_pri,"Conv: %lu us\n",t2-t1);

	fprintf(file_pri,"Channel state: %d\n",HAL_DFSDM_ChannelGetState(&hdfsdm1_channel3));

	return 0;
}


unsigned char CommandParserAudReg(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	DFSDM_Channel_TypeDef *channels[8]={	DFSDM1_Channel0,DFSDM1_Channel1,DFSDM1_Channel2,DFSDM1_Channel3,
											DFSDM1_Channel4,DFSDM1_Channel5,DFSDM1_Channel6,DFSDM1_Channel7};
	for(int i=0;i<8;i++)
	{
		fprintf(file_pri,"Channel %d (%p)\n",i,channels[i]);
		fprintf(file_pri,"\tCHCFGR1: %08X %s\n",(unsigned)channels[i]->CHCFGR1,dfsdm_chcfgr1(channels[i]->CHCFGR1));
		fprintf(file_pri,"\tCHCFGR2: %08X %s\n",(unsigned)channels[i]->CHCFGR2,dfsdm_chcfgr2(channels[i]->CHCFGR2));
		fprintf(file_pri,"\tCHAWSCDR: %08X\n",(unsigned)channels[i]->CHAWSCDR);
		fprintf(file_pri,"\tCHWDATAR: %08X\n",(unsigned)channels[i]->CHWDATAR);
		fprintf(file_pri,"\tCHDATINR: %08X\n",(unsigned)channels[i]->CHDATINR);
	}

	DFSDM_Filter_TypeDef *filters[4]={	DFSDM1_Filter0,DFSDM1_Filter1,DFSDM1_Filter2,DFSDM1_Filter3};
	for(int i=0;i<4;i++)
	{
		fprintf(file_pri,"Filter %d (%p)\n",i,filters[i]);
		fprintf(file_pri,"\tFLTCR1: %08X FLTCR2: %08X\n",(unsigned)filters[i]->FLTCR1,(unsigned)filters[i]->FLTCR2);
		fprintf(file_pri,"\tFLTISR: %08X FLTICR: %08X\n",(unsigned)filters[i]->FLTISR,(unsigned)filters[i]->FLTICR);
		fprintf(file_pri,"\tFLTJCHGR: %08X FLTFCR: %08X\n",(unsigned)filters[i]->FLTJCHGR,(unsigned)filters[i]->FLTFCR);
		fprintf(file_pri,"\tFLTJDATAR: %08X FLTRDATAR: %08X\n",(unsigned)filters[i]->FLTJDATAR,(unsigned)filters[i]->FLTRDATAR);
		fprintf(file_pri,"\tFLTAWHTR: %08X FLTAWLTR: %08X\n",(unsigned)filters[i]->FLTAWHTR,(unsigned)filters[i]->FLTAWLTR);
		fprintf(file_pri,"\tFLTAWSR: %08X FLTAWCFR: %08X\n",(unsigned)filters[i]->FLTAWSR,(unsigned)filters[i]->FLTAWCFR);
		fprintf(file_pri,"\tFLTEXMAX: %08X FLTEXMIN: %08X\n",(unsigned)filters[i]->FLTEXMAX,(unsigned)filters[i]->FLTEXMIN);
		fprintf(file_pri,"\tFLTCNVTIMR: %08X\n",(unsigned)filters[i]->FLTCNVTIMR);
	}


	return 0;

}
unsigned char CommandParserAudInternalOffset(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;


	// Stop DMA
	stm_dfsdm_acquirdmaeoff();

	// Start poll
	HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter0);			// filter 0 is internal mic filter

	int offset = stm_dfsdm_calib_zero_internal();

	fprintf(file_pri,"Internal microphone measured offset: %d\n",offset);

	return 0;
}
unsigned char CommandParserAudZeroCalibrate(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	// Stop DMA
	stm_dfsdm_acquirdmaeoff();

	// Start poll
	HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter0);			// filter 0 is internal mic filter

	int offset = stm_dfsdm_calib_zero_internal();

	fprintf(file_pri,"Zero calibration offset: %d\n",offset);

	HAL_StatusTypeDef s = HAL_DFSDM_ChannelModifyOffset(&hdfsdm1_channel5,offset);

	fprintf(file_pri,"Cancelled offset of internal mic (%d)\n",s);

	return 0;
}
unsigned char CommandParserAudZeroCalibrateClear(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	HAL_StatusTypeDef s = HAL_DFSDM_ChannelModifyOffset(&hdfsdm1_channel5,0);

	fprintf(file_pri,"Cleared internal mic offset (%d)\n",s);
	return 0;
}
unsigned char CommandParserAudTest(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	stm_dfsdm_data_test();

	return 0;
}

unsigned char CommandParserAudDMA(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	// Stop polling if running
	HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter0);			// filter 0 is internal mic filter

	stm_dfsdm_acquirdmaeon();

	return 0;
}
unsigned char CommandParserAudDMAStop(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	stm_dfsdm_acquirdmaeoff();

	return 0;

}
unsigned char CommandParserAudDMAAcquireStat(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	STM_DFSMD_TYPE buf[STM_DFSMD_BUFFER_SIZE];

	unsigned long timems,timeold=0,timestart=0;
	unsigned long ctr=0;
	unsigned long pkt;

	fprintf(file_pri,"Audio data acquired via DMA. Press return to stop and print statistics.\n");
	while(1)
	{
		if(!stm_dfsdm_data_getnext(buf,&timems,&pkt))
		{
			if(timestart==0)
			{
				timestart = timems;
				ctr=0;
			}
			else
				ctr++;

			if((ctr%100)==0)
			{
				fprintf(file_pri,"timems: %lu. dt: %lu. level: %u. %lu ms for %lu frames. \n",timems,timems-timeold,(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
				unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
				fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

			}

			timeold=timems;
		}

		if(fgetc(file_pri)!=-1)
			break;
	}

	return 0;
}
unsigned char CommandParserAudDMAAcquire(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	STM_DFSMD_TYPE buf[STM_DFSMD_BUFFER_SIZE];

	unsigned long timems,timestart=0;
	unsigned long ctr=0;
	unsigned long pkt;

	fprintf(file_pri,"Audio data acquired via DMA\n");
	while(1)
	{
		if(!stm_dfsdm_data_getnext(buf,&timems,&pkt))
		{
			if(timestart==0)
			{
				timestart = timems;
				ctr=0;
			}
			else
				ctr++;

			for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i+=2)
				//fprintf(file_pri,"%10d %08X\n ",buf[i],buf[i]);
				//fprintf(file_pri,"%d\n",buf[i]>>8);				// Print data without channel
				fprintf(file_pri,"%d\n",buf[i]);
		}

		if(fgetc(file_pri)!=-1)
			break;
	}

	fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
	unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
	fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

	return 0;
}


unsigned char CommandParserAudLog(char *buffer,unsigned char size)
{
	(void)size;

	int lognum;

	ParseCommaGetInt(buffer,1,&lognum);

	FILE *mode_sample_file_log = ufat_log_open(lognum);
	if(!mode_sample_file_log)
	{
		fprintf(file_pri,"Error opening log\n");
		return 1;
	}

	STM_DFSMD_TYPE buf[STM_DFSMD_BUFFER_SIZE];

	unsigned long timems,timestart=0;
	unsigned long ctr=0;
	unsigned long pkt;

	// Clear buffers
	stm_dfsdm_data_clear();

	fprintf(file_pri,"Audio data acquired via DMA and stored to log %d. Press return to stop\n",lognum);
	while(1)
	{
		if(!stm_dfsdm_data_getnext(buf,&timems,&pkt))
		{
			if(timestart==0)
			{
				timestart = timems;
				ctr=0;
			}
			else
				ctr++;

			for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
			{
				//fprintf(file_pri,"%10d %08X\n ",buf[i],buf[i]);
				//fprintf(file_pri,"%d\n",buf[i]>>8);
				//fprintf(mode_sample_file_log,"%d\n",buf[i]>>8);
				//fprintf(mode_sample_file_log,"%d\n",buf[i]);
				//fprintf(mode_sample_file_log,"0000000\n");
				// Faster via s16toa
				char strbuf[8];
				s16toa(buf[i],strbuf);
				strbuf[7]='\n';
				fwrite(strbuf,1,8,mode_sample_file_log);
				//fprintf(mode_sample_file_log,"%d\n",buf[i]);
			}
		}

		if(fgetc(file_pri)!=-1)
			break;
	}

	ufat_log_close();

	fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
	unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
	fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

	return 0;
}

unsigned char CommandParserAudInit(char *buffer,unsigned char size)
{
	(void)size;

	unsigned mode=STM_DFSMD_INIT_16K;

	if(ParseCommaGetInt(buffer,1,&mode))
	{
		stm_dfsdm_printmodes(file_pri);
	}
	else
	{
		if(mode<0 || mode>STM_DFSMD_INIT_48K)
		{
			return 2;
		}
		stm_dfsdm_init(mode);
	}
	return 0;

}


unsigned char CommandParserAudInitDetailed(char *buffer,unsigned char size)
{
	(void)size;

	unsigned order,fosr,iosr,div,rightshift;

	if(ParseCommaGetInt(buffer,5,&order,&fosr,&iosr,&div,&rightshift))
		return 2;
	if(order<1 || order>5)
	{
		fprintf(file_pri,"Order must be e[1;5]\n");
		return 2;
	}

	fprintf(file_pri,"Init with sinc order %d filter oversampling %d, integrator oversampling %d, clock divider %d, rightshift %d\n",order,fosr,iosr,div,rightshift);
	stm_dfsdm_initsampling(order,fosr,iosr,div);
	_stm_dfsdm_rightshift=rightshift;



	return 0;

}
unsigned char CommandParserAudBench(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	long int perf,refperf;
	//unsigned long int mintime=10;
	unsigned long int mintime=1;

	stm_dfsdm_init(STM_DFSMD_INIT_OFF);
	unsigned long p = stm_dfsmd_perfbench_withreadout(mintime);
	refperf=p;
	fprintf(file_pri,"Reference performance: %lu\n",p);

	for(int mode = 0; mode<=3; mode++)
	{
		stm_dfsdm_init(mode);

		perf = stm_dfsmd_perfbench_withreadout(mintime);

		stm_dfsdm_init(STM_DFSMD_INIT_OFF);

		long load = 100-(perf*100/refperf);
		if(load<0)
			load=0;

		fprintf(file_pri,"\tMode %d: perf: %lu (instead of %lu). CPU load %lu %%\n",mode,perf,refperf,load);
	}







	return 0;
}
unsigned char CommandParserAudFFT(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

//	arm_rfft_fast_init_f32(&S, FFT_SampleNum );

	return 0;
}

