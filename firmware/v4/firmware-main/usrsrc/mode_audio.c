#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//#include "arm_math.h"

#include "usrmain.h"
#include "global.h"
#include "serial.h"
#include "i2c.h"
#include "wait.h"
//#include "init.h"
#include "uiconfig.h"
#include "helper.h"
#include "i2c/i2c_transaction.h"
#include "system.h"

#include "helper_num.h"
#include "commandset.h"
#include "mode_main.h"
#include "mode_global.h"
#include "mode_audio.h"
#include "usrmain.h"
#include "main.h"
#include "mode.h"
#include "dfsdm.h"
#include "stmdfsdm.h"
#include "ufat.h"





/*
 * V3:
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
//int aud_int_channel6=1;

//#define AUDBUFSIZ 4096
/*#define AUDBUFSIZ 128
//#define AUDBUFSIZ (STM_DFSMD_BUFFER_SIZE*2)				// Twice size for copy on half full and full
int audbuf[AUDBUFSIZ];*/




const COMMANDPARSER CommandParsersAudio[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},

	{0,0,"---- General information ----"},
	{0,0,"Use S to initialise and start sampling via DMA. Stream data with d or x; log data with L."},
	{0,0,"Low level initialisation-only (sampling is not started): I or i for pre-defined or custom settings. Start sampling with D or P."},
	{0,0,"<left_right> must be identical to the initialisation value (S, I, i) for all subsequent commands (D, P, p, O, Z, etc)."},
	{0,0,"<left_right>: 0=left, 1=right, 2=stereo."},
	{0,0,"Stop sampling (s) and re-initialise to change betewen DMA and polling."},
	{0,0,"---- Initialisation and start/stop of sampling ----"},
	{'S',CommandParserAudInitStartDMA,"S[,<mode>,<left_right>] Initialise microphone acquisition from a pre-defined mode, zero calibrate, and start sampling with DMA.\n\t\tNo parameters: prints available modes. "},
	{'I',CommandParserAudInitOnly,"I[,<mode>,<left_right>] Initialise microphone acquisition from a pre-defined mode, without starting sampling.\n\t\tStart sampling with D or P."},
	{'i',CommandParserAudInitOnlyDetailed,"i,<order>,<fosr>,<isr>,<div>,<rightshift>,<left_right> Initialise microphone acquisition with custom settings.\n\t\tStart sampling with D or P."},
	{'D',CommandParserAudDMAStart,"D,<left_right> Start DMA sampling. Use after I or i, with the same left_right parameter."},
	{'P',CommandParserAudPollStart,"P,<left_right> Start polling sampling. Use after I or i, with the same left_right parameter."},
	{'s',CommandParserAudStop,"Stop DMA and polling sampling"},
	{0,0,"---- Display acquired data ----"},
	{'d',CommandParserAudDMAPrint,"d[,<downsample>] Stream data acquired via DMA in frame format. Sampling must be started with S, or I|i and D.\n\t\tDownsample=0: display summary statistics. downsample>=1: display samples with downsampling (1=no downsampling)."},
	{'X',CommandParserAudDMAPrintStereo,"X[,<downsample>] Stream data acquired via DMA in stereo 2-column format (same as L). Sampling must be started with S, or I|i and D.\n\t\tOptionally downsample by the specified factor (1=no downsampling)."},
	{'x',CommandParserAudDMAPrintStereoBin,"x[,<downsample>] Stream data acquired via DMA in a 30-bit binary stereo format (\"D;ss\" in DScope). Sampling must be started with S, or I|i and D.\n\t\tOptionally downsample by the specified factor (1=no downsampling)."},
	{'p',CommandParserAudPollPrint,"p,<left_right> Stream data acquired via polling.\n\t\tleft_right must be identical to that specified in I or i, otherwise the function blocks on unavailable channels."},
	{0,0,"---- Logging ----"},
	{'L',CommandParserAudLog,"L,<lognum> Log data acquired via DMA to lognum. Sampling must be started with S, or I|i and D."},
	{0,0,"--- Microphone offset cancellation ---"},
	{'Z',CommandParserAudOffsetAutoCalibrate,"Z,<left_right> Zero-calibrate. Polling sampling (P) must be active."},
	{'z',CommandParserAudOffsetClear,"Clears zero-calibration"},
	{'O',CommandParserAudOffsetInternalMeasure,"O,<left_right>: Measure offset. Polling sampling (P) must be active."},
	{'o',CommandParserAudOffsetSet,"o,<off_l>,<off_r>: Set left and right offset (offset subtracted from samples). Polling sampling (P) must be active."},
	{0,0,"---- Statistics ----"},
	{'T',CommandParserAudStatPrint,"Print DMA acquisition statistics"},
	{'t',CommandParserAudStatClear,"Clear DMA acquisition statistics"},
	{0,0,"---- Various ----"},
	{'K',CommandParserAudBench,"Benchmark DMA acquisition overhead"},
	{'q',CommandParserAudStatus,"Channel and filter status"},
	{'G',CommandParserAudReg,"Print DFSDM registers"},
	{'R',CommandParserAudRightshift,"R,<shift_l>,<shift_r> Set the hardware right-shift"},

	{'1',CommandParserAudSync,"Sync"},


	//{'d',CommandParserAudDMAStop,"Stop sampling with DMA acquisition"},
	//{'q',CommandParserAudStart,"S[,<ext>] Start sampling with polling acquisition on internal mic or external mic when ext=1"},


	//{'t',CommandParserAudTest,"Test frame buffers"},



	//{'p',CommandParserAudPollExtStereo,"Audio acquisition via polling on external mic (use S,1 beforehand to start sampling)"},


	//{'F',CommandParserAudFFT,"FFT on internal mic"},



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
	// External code path not maintained.
#if 0
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

	fprintf(file_pri,"Stop audio acquisition on %s microphone\n",ext?"external":"internal");

	if(ext)
	{
#warning Codepath for external mics not maintained.
		HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter2);			// External left/right channels
		HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter3);			// External left/right channels
	}
	else
	{
#endif
		stm_dfsdm_acquire_stop_all();
#if 0
	}
#endif


	return 0;
}

#if 0
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
#endif
#if 0
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
#endif
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

	stm_dfsdm_printreg();

	return 0;

}
unsigned char CommandParserAudOffsetInternalMeasure(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int left_right;

	if(ParseCommaGetInt(buffer,1,&left_right))
		return 2;
	if(left_right<0 || left_right>2)
		return 2;


	_stm_dfsdm_offset_measure(left_right,0,0,1);


	return 0;
}
unsigned char CommandParserAudOffsetSet(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int ol,or;

	if(ParseCommaGetInt(buffer,2,&ol,&or))
		return 2;

	stm_dfsdm_offset_set(STM_DFSDM_LEFT,ol);
	stm_dfsdm_offset_set(STM_DFSDM_RIGHT,or);


	return 0;
}
unsigned char CommandParserAudOffsetAutoCalibrate(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int left_right;

	if(ParseCommaGetInt(buffer,1,&left_right))
		return 2;
	if(left_right<0 || left_right>2)
		return 2;

	_stm_dfsdm_offset_calibrate(left_right,1);

	return 0;
}
unsigned char CommandParserAudOffsetClear(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	// Clear left&right
	stm_dfsdm_offset_set(STM_DFSDM_STEREO,0);


	return 0;
}
unsigned char CommandParserAudTest(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	stm_dfsdm_data_test();

	return 0;
}
/******************************************************************************
	function: CommandParserAudPollStart
*******************************************************************************
	Start polling acquisition.

	parameter left_right mandatory

	Parameters:
		-
	Returns:
		-
******************************************************************************/
unsigned char CommandParserAudPollStart(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int left_right;

	if(ParseCommaGetInt(buffer,1,&left_right))
		return 2;
	if(left_right<0 || left_right>2)
		return 2;

	stm_dfsdm_acquire_start_poll(left_right);		// This starts the left/right/stereo as appropriate

	return 0;
}

/******************************************************************************
	function: CommandParserAudDMA
*******************************************************************************
	Start DMA acquisition.

	parameter left_right mandatory

	Parameters:
		-
	Returns:
		-
******************************************************************************/
unsigned char CommandParserAudDMAStart(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int left_right;

	if(ParseCommaGetInt(buffer,1,&left_right))
		return 2;
	if(left_right<0 || left_right>2)
		return 2;


	stm_dfsdm_acquire_start_dma(left_right);		// This starts the left/right/stereo as appropriate

	return 0;
}
/******************************************************************************
	function: CommandParserAudPollAcquire
*******************************************************************************
	Print data acquired by polling.

	The command parameter left_right indicates which microphone to acquire.
	The data is printed in single or two columns in mono and stereo respectively


	Parameters:
		-
	Returns:
		-
******************************************************************************/
#if 0
unsigned char CommandParserAudPollPrint(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int left_right;

	if(ParseCommaGetInt(buffer,1,&left_right))
		return 2;
	if(left_right<0 || left_right>2)
		return 2;

	//unsigned maxsample=10000;
	unsigned maxsample=100;
	int data[maxsample];
	unsigned long dt = stm_dfsdm_acq_poll_internal(left_right,data,maxsample);

	int inc = 1;
	if(left_right==STM_DFSDM_STEREO)	// In stereo data is interleaved
		inc=2;

	for(int i=0;i<maxsample;i+=inc)
	{
		if(left_right!=STM_DFSDM_STEREO)
			fprintf(file_pri,"%d\n",data[i]);
		else
			fprintf(file_pri,"%d %d\n",data[i],data[i+1]);
	}
	unsigned long long sr = (unsigned long long)maxsample*1000000/dt;


	fprintf(file_pri,"%% Acquisition time for %u samples: %lu us. Sample rate: %lu\n",maxsample,dt,(unsigned long)sr);



	return 0;
}
#endif
unsigned char CommandParserAudPollPrint(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int left_right;

	if(ParseCommaGetInt(buffer,1,&left_right))
		return 2;
	if(left_right<0 || left_right>2)
		return 2;

	fprintf(file_pri,"Left right: %d\n",left_right);

	//unsigned maxsample=10000;
	unsigned maxsample=54;
	int data[maxsample];
	unsigned datat[maxsample];
	unsigned long dt = stm_dfsdm_acq_poll_internal_t(left_right,data,datat,maxsample);

	int inc = 1;
	if(left_right==STM_DFSDM_STEREO)	// In stereo data is interleaved
		inc=2;

	for(int i=0;i<maxsample;i+=inc)
	{
		if(left_right!=STM_DFSDM_STEREO)
		{
			fprintf(file_pri,"%d (%u)\n",data[i],datat[i]);
		}
		else
		{
			fprintf(file_pri,"%d (%u) %d (%u)\n",data[i],datat[i],data[i+1],datat[i+1]);
		}
	}
	unsigned long long sr = (unsigned long long)maxsample*1000000/dt;


	fprintf(file_pri,"%% Acquisition time for %u samples: %lu us. Sample rate: %lu\n",maxsample,dt,(unsigned long)sr);



	return 0;
}

/******************************************************************************
	function: CommandParserAudDMAAcquire
*******************************************************************************
	Print data acquired by DMA.

	The parameter summary can be set to 1 to only print summary statistics.

	Parameters:
		-
	Returns:
		-
******************************************************************************/
unsigned char CommandParserAudDMAPrint(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int downsample=1;
	if(ParseCommaGetNumParam(buffer)>1)
		return 2;
	if(ParseCommaGetNumParam(buffer)>=1 && ParseCommaGetInt(buffer,1,&downsample))
		return 2;
	if(downsample<0)
		return 2;

	fprintf(file_pri,"Downsample: %d\n",downsample);

	STM_DFSMD_TYPE buf[STM_DFSMD_BUFFER_SIZE];

	unsigned long timems,timestart=0;
	unsigned long ctr=0;
	unsigned long pkt;
	unsigned char left_right;

	fprintf(file_pri,"Print %s audio data acquired via DMA. Press enter to stop.\n",(downsample==0)?"summary statistics of":"");
	while(1)
	{
		if(fischar(file_pri))		// Faster than fgetc (fgets is ~30uS/call)
			break;
		if(stm_dfsdm_data_getnext(buf,&timems,&pkt,&left_right))
		{
			continue;
		}
		// Counter starting with the first frame
		if(timestart==0)
		{
			timestart = timems;
			ctr=0;
		}
		else
			ctr++;

		if(downsample!=0)
		{
			// Print the received data
			fprintf(file_pri,"%lu %lu %u ",timems,pkt,(unsigned)left_right);
			for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i+=downsample)		// Downsampled for speed reasons
				fprintf(file_pri,"%d ",buf[i]);
			fprintf(file_pri,"\n");
		}
		else
		{
			if((ctr%100)==0)
			{
				fprintf(file_pri,"After frame %lu: timems: %lu. buffer level: %u. time from start: %lu ms.\n",ctr,timems,(unsigned)stm_dfsdm_data_level(),timems-timestart);
				unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
				fprintf(file_pri,"\tTotal frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
				fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

			}
		}
	}

	fprintf(file_pri,"level: %u. %lu ms for %lu frames.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
	unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
	fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

	return 0;
}

/******************************************************************************
	function: CommandParserAudLog
*******************************************************************************
	Log data acquired via DMA.

	Command line: lognum

	Data is stored in 2 column format (stereo) regardless of whether the data
	is acquired in mono or stereo.

	Parameters:
		-
	Returns:
		-
******************************************************************************/
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

	// Handle stereo
	STM_DFSMD_TYPE buf[2][STM_DFSMD_BUFFER_SIZE],c_buf[STM_DFSMD_BUFFER_SIZE];
	memset(buf,0,sizeof(buf));




	unsigned long timems,timestart=0;
	unsigned long ctr=0;
	unsigned long c_pkt=0,p_pkt=0;		// Current and previous packet counter - used to commit frames to log
	unsigned char c_lr;

	// Clear buffers
	stm_dfsdm_data_clear();

	fprintf(file_pri,"Logging DMA-sampled data to log %d. Press return to stop\n",lognum);

	//fprintf(file_pri,"%d %d\n",sizeof(buf),sizeof(c_buf));

	// Print in two column format when using the stereo mode

	while(1)
	{
		// Check if interruption
		if(fischar(file_pri))		// Faster than fgetc (fgets is ~30uS/call)
			break;
		// Get data with "current packet" and "current lr" in "current buf"
		if(stm_dfsdm_data_getnext(c_buf,&timems,&c_pkt,&c_lr))
		{
			continue;
		}

		// Initialise the timing/counters upon reception of the first data
		if(timestart==0)
		{
			timestart = timems;
			ctr=0;
			p_pkt = c_pkt;
		}
		else
			ctr++;

		//fprintf(file_pri,"%d %d %d\n",c_lr,c_pkt,p_pkt);


		// Sanity check on L/R
		if(c_lr<0 || c_lr>1)
		{
			fprintf(file_pri,"Unexpected L/R channels\n");
			break;
		}



		// Move the data to the appropriate buffer
		memcpy(buf[c_lr],c_buf,sizeof(c_buf));

		// Current packet identical to previous: nothing to commit yet.
		if(c_pkt==p_pkt)
			continue;

		if((c_pkt%100)==0)
		{
			fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
			fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
		}

		//fprintf(file_pri,"commit\n");

		// Commit data
		// As multiple calls to fwrite are slow, assemble the data in a buffer and call fwrite once
		char strbuf[STM_DFSMD_BUFFER_SIZE*14];		// One sample: 14 bytes
		for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
		{
			// Faster via s16toa
			s16toa(buf[0][i],strbuf+i*14);
			strbuf[i*14+6]=' ';
			s16toa(buf[1][i],strbuf+i*14+7);
			strbuf[i*14+13]='\n';
		}
		fwrite(strbuf,1,STM_DFSMD_BUFFER_SIZE*14,mode_sample_file_log);

		// Update the previous packet counter
		p_pkt = c_pkt;
		// Clear buffers
		memset(buf,0,sizeof(buf));

	}

	ufat_log_close();

	fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
	unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
	fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

	return 0;
}
/******************************************************************************
	function: CommandParserAudDMAPrintStereo
*******************************************************************************
	Log data acquired via DMA.

	Command line: downsample

	Data is stored in 2 column format (stereo) regardless of whether the data
	is acquired in mono or stereo.

	Parameters:
		-
	Returns:
		-
******************************************************************************/
unsigned char CommandParserAudDMAPrintStereo(char *buffer,unsigned char size)
{
	(void)size; (void) size;


	unsigned t1 = timer_ms_get();
	unsigned c = 10000;
	for(int i=0;i<c;i++)
		fgetc(file_pri);
	unsigned t2 = timer_ms_get();
	fprintf(file_pri,"%u in %u ms\n",c,t2-t1);

	HAL_Delay(500);


	/*
	// Keypress to quit - faster than parsing user commands.
				if(fischar(file_pri))		// Faster than fgetc
					break;*/

	int downsample;

	if(!ParseCommaGetInt(buffer,1,&downsample))
	{
		// Parse successful: check validity
		if(downsample<1)
			return 2;
	}
	else
	{
		// Default downsample
		downsample=1;
	}


	// Handle stereo
	STM_DFSMD_TYPE buf[2][STM_DFSMD_BUFFER_SIZE],c_buf[STM_DFSMD_BUFFER_SIZE];
	memset(buf,0,sizeof(buf));

	unsigned long timems,timestart=0;
	unsigned long ctr=0;
	unsigned long err=0;
	unsigned long c_pkt=0,p_pkt=0;		// Current and previous packet counter - used to commit frames to log
	unsigned char c_lr;

	// Clear buffers
	stm_dfsdm_data_clear();

	fprintf(file_pri,"Streaming DMA-sampled data. Press return to stop\n");


	// Print in two column format when using the stereo mode

	while(1)
	{
		// Check if interruption
		if(fischar(file_pri))		// Faster than fgetc (fgets is ~30uS/call)
			break;
		// Get data with "current packet" and "current lr" in "current buf"
		if(stm_dfsdm_data_getnext(c_buf,&timems,&c_pkt,&c_lr))
		{
			continue;
		}

		// Initialise the timing/counters upon reception of the first data
		if(timestart==0)
		{
			timestart = timems;
			ctr=0;
			p_pkt = c_pkt;
		}
		else
			ctr++;

		//fprintf(file_pri,"%d %d %d\n",c_lr,c_pkt,p_pkt);


		// Sanity check on L/R
		if(c_lr<0 || c_lr>1)
		{
			fprintf(file_pri,"Unexpected L/R channels\n");
			break;
		}



		// Move the data to the appropriate buffer
		memcpy(buf[c_lr],c_buf,sizeof(c_buf));

		// Current packet identical to previous: nothing to commit yet.
		if(c_pkt==p_pkt)
			continue;

		if((c_pkt%100)==0)
		{
			fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
			fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
		}

		for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i+=downsample)
		{
			// Faster via s16toa
			char strbuf[32];
			s16toa(buf[0][i],strbuf);
			strbuf[6]=' ';
			s16toa(buf[1][i],strbuf+7);
			strbuf[13]='\n';
			strbuf[14]=0;
			//fwrite(strbuf,1,14,file_pri);
			fputs(strbuf,file_pri);
			//err += fputbuf(file_pri,strbuf,14);
		}

		// Update the previous packet counter
		p_pkt = c_pkt;
		// Clear buffers
		memset(buf,0,sizeof(buf));

	}

	fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
	fprintf(file_pri,"Samples not transmitted: %lu\n",err);
	unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
	fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

	return 0;
}
unsigned char CommandParserAudDMAPrintStereoBin(char *buffer,unsigned char size)
{
	(void)size; (void) size;


	unsigned t1 = timer_ms_get();
	unsigned c = 10000;
	for(int i=0;i<c;i++)
		fgetc(file_pri);
	unsigned t2 = timer_ms_get();
	fprintf(file_pri,"%u in %u ms\n",c,t2-t1);

	HAL_Delay(500);


	/*
	// Keypress to quit - faster than parsing user commands.
				if(fischar(file_pri))		// Faster than fgetc
					break;*/

	int downsample;

	if(!ParseCommaGetInt(buffer,1,&downsample))
	{
		// Parse successful: check validity
		if(downsample<1)
			return 2;
	}
	else
	{
		// Default downsample
		downsample=1;
	}


	// Handle stereo
	STM_DFSMD_TYPE buf[2][STM_DFSMD_BUFFER_SIZE],c_buf[STM_DFSMD_BUFFER_SIZE];
	memset(buf,0,sizeof(buf));

	unsigned long timems,timestart=0;
	unsigned long ctr=0;
	unsigned long err=0;
	unsigned long c_pkt=0,p_pkt=0;		// Current and previous packet counter - used to commit frames to log
	unsigned char c_lr;

	// Clear buffers
	stm_dfsdm_data_clear();

	fprintf(file_pri,"Binary streaming DMA-sampled data. Press return to stop\n");


	// Print in two column format when using the stereo mode

	while(1)
	{
		// Check if interruption
		if(fischar(file_pri))		// Faster than fgetc (fgets is ~30uS/call)
			break;
		// Get data with "current packet" and "current lr" in "current buf"
		if(stm_dfsdm_data_getnext(c_buf,&timems,&c_pkt,&c_lr))
		{
			continue;
		}

		// Initialise the timing/counters upon reception of the first data
		if(timestart==0)
		{
			timestart = timems;
			ctr=0;
			p_pkt = c_pkt;
		}
		else
			ctr++;

		//fprintf(file_pri,"%d %d %d\n",c_lr,c_pkt,p_pkt);


		// Sanity check on L/R
		if(c_lr<0 || c_lr>1)
		{
			fprintf(file_pri,"Unexpected L/R channels\n");
			break;
		}



		// Move the data to the appropriate buffer
		memcpy(buf[c_lr],c_buf,sizeof(c_buf));

		// Current packet identical to previous: nothing to commit yet.
		if(c_pkt==p_pkt)
			continue;

		if((c_pkt%100)==0)
		{
			fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
			fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
		}

		for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i+=downsample)
		{
			unsigned char packet[32];
			unsigned short d;
			int dl;



			packet[0]='D'; // (ascii=156d, msb=1)

			int o=1;

			// Binary format has only 30-bit resolution to have a unique frame header and hack the LSB to avoid the header in the data stream.
			dl = buf[0][i];
			dl+=32768;				// Shift to positive only range
			dl>>=2;					// Clear two MSB bits, decrease resolution by 2 bit
			d=(unsigned short)dl;	// Convert to unsigned
			packet[o+0]=d>>8;
			packet[o+1]=d&0xff;
			if(packet[o+1] == 'D')
				packet[o+1]--;

			dl = buf[1][i];
			dl+=32768;				// Shift to positive only range
			dl>>=2;					// Clear two MSB bits, decrease resolution by 2 bit
			d=(unsigned short)dl;	// Convert to unsigned
			packet[o+2]=d>>8;
			packet[o+3]=d&0xff;
			if(packet[o+3] == 'D')
				packet[o+3]--;

			err += fputbuf(file_pri,(char*)packet,5);


		}

		// Update the previous packet counter
		p_pkt = c_pkt;
		// Clear buffers
		memset(buf,0,sizeof(buf));

	}

	fprintf(file_pri,"level: %u. %lu ms for %lu buffers.\n",(unsigned)stm_dfsdm_data_level(),timems-timestart,ctr);
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
	fprintf(file_pri,"Samples not transmitted: %lu\n",err);
	unsigned long long sps = (unsigned long long)(ctr*STM_DFSMD_BUFFER_SIZE)*1000/(timems-timestart);
	fprintf(file_pri,"\tSample rate: %lu\n",(unsigned long)sps);

	return 0;
}

unsigned char CommandParserAudInitStartDMA(char *buffer,unsigned char size)
{
	(void)size;

	int mode=STM_DFSMD_INIT_16K;
	int left_right;
	unsigned char np = ParseCommaGetNumParam(buffer);


	if( !(np==0 || np==2) )		// 0 or 2 parameters are ok - nothing else.
		return 2;


	if(ParseCommaGetInt(buffer,2,&mode,&left_right))
	{
		stm_dfsdm_printmodes(file_pri);
		return 2;
	}

	if(mode<0 || mode>STM_DFSMD_INIT_MAXMODES)
		return 2;
	if(left_right<0 || left_right>2)
		return 2;

	// High-level initialisation, zeroing, and start acquisition
	stm_dfsdm_init(mode,left_right);

	return 0;

}
/******************************************************************************
	function: CommandParserAudInitOnly
*******************************************************************************
	Initialises the data acquisition with one of the predefined acquisiiton modes.

	Does not start polling or DMA acquisition.

	Parameters:
		-
	Returns:
		-
******************************************************************************/
unsigned char CommandParserAudInitOnly(char *buffer,unsigned char size)
{
	(void)size;

	int mode=STM_DFSMD_INIT_16K;
	int left_right;
	unsigned char np = ParseCommaGetNumParam(buffer);


	if( !(np==0 || np==2) )		// 0 or 2 parameters are ok - nothing else.
		return 2;


	if(ParseCommaGetInt(buffer,2,&mode,&left_right))
	{
		stm_dfsdm_printmodes(file_pri);
		return 2;
	}

	if(mode<0 || mode>STM_DFSMD_INIT_MAXMODES)
		return 2;
	if(left_right<0 || left_right>2)
		return 2;

	// Low-level initialisation only
	_stm_dfsdm_init_predef(mode,left_right);

	return 0;

}

unsigned char CommandParserAudInitOnlyDetailed(char *buffer,unsigned char size)
{
	(void)size;

	unsigned order,fosr,iosr,div,rightshift,left_right;

	if(ParseCommaGetInt(buffer,6,&order,&fosr,&iosr,&div,&rightshift,&left_right))
		return 2;
	if(order<1 || order>5)
	{
		fprintf(file_pri,"Order must be e[1;5]\n");
		return 2;
	}
	if(left_right<0 || left_right>2)
		return 2;

	fprintf(file_pri,"Init with sinc order %d filter oversampling %d, integrator oversampling %d, clock divider %d, rightshift %d left_right: %d\n",order,fosr,iosr,div,rightshift,left_right);
	stm_dfsdm_initsampling(left_right,order,fosr,iosr,div);
	_stm_dfsdm_rightshift=rightshift;

	stm_dfsdm_rightshift_set(left_right,_stm_dfsdm_rightshift);



	return 0;

}
unsigned char CommandParserAudBench(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	long int perf,refperf;
	//unsigned long int mintime=10;
	unsigned long int mintime=1;

	stm_dfsdm_init(STM_DFSMD_INIT_OFF,0);
	unsigned long p = stm_dfsmd_perfbench_withreadout(mintime);
	refperf=p;
	fprintf(file_pri,"\n\n=== Reference performance: %lu ===\n\n\n",p);

	for(int channel=0;channel<3;channel+=2)		// Loops 0 (left) and 2 (stereo)
	{
		for(int mode = 0; mode<=STM_DFSMD_INIT_MAXMODES; mode++)
		{
			stm_dfsdm_init(mode,channel);

			perf = stm_dfsmd_perfbench_withreadout(mintime);

			stm_dfsdm_init(STM_DFSMD_INIT_OFF,0);

			long load = 100-(perf*100/refperf);
			if(load<0)
				load=0;

			fprintf(file_pri,"\n\t === Mode %d channel %s: perf: %lu (instead of %lu). CPU load %lu %% ===\n\n",mode,channel==0?"left":"stereo",perf,refperf,load);
		}
	}







	return 0;
}
#if 0
unsigned char CommandParserAudFFT(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

//	arm_rfft_fast_init_f32(&S, FFT_SampleNum );

	return 0;
}
#endif

unsigned char CommandParserAudNothing(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	fprintf(file_pri,"Not implemented\n");

	return 0;
}
unsigned char CommandParserAudStatPrint(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	stm_dfsdm_stat_print();

	return 0;
}
unsigned char CommandParserAudStatClear(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	stm_dfsdm_stat_clear();

	return 0;
}

unsigned char CommandParserAudStatus(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	stm_dfsdm_state_print();

	return 0;
}



unsigned char CommandParserAudRightshift(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	int l,r;

	if(ParseCommaGetInt(buffer,2,&l,&r))
		return 2;

	stm_dfsdm_rightshift_set(STM_DFSDM_LEFT,l);
	stm_dfsdm_rightshift_set(STM_DFSDM_RIGHT,r);

	return 0;

}
unsigned char CommandParserAudSync(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	_stm_dfsdm_sampling_sync();

	return 0;
}
