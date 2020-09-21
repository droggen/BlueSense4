/*
 * mode_adc.c
 *
 *  Created on: 11 janv. 2020
 *      Author: droggen
 */

#include "commandset.h"
#include "usrmain.h"
#include "global.h"
#include "mode.h"
#include "mode_adc.h"
#include "adc.h"
#include "helper.h"
#include "wait.h"
#include "stmadc.h"
#include "tim.h"
#include "serial_usb.h"

/*
Max application sample frequency: 10KHz
S&H: 2.5 clock
Conversion: 12.5 clock
-> 15 clock.
20MHz/1 -> 1.3Msps
20MHz/4 -> 333Ksps

20MHz/4: S&H 2.5 clocks => 0.5uS for S&H -> 2MHz signal.
20MHz/4: S&H 24.5 clocks => 4.9uS for S&H -> 200KHz signal.


S&H 24.5 clock + conversion 12.5 clock: 37 clock 20MHz/4/37 = 135ksps -> OK


1000 conv: 13279 us 13.2uS/conv nconv=2 eoc=singleconv
1000 conv: 16784 us 16.7uS/conv nconv=2 eoc=seqconv
1000 conv: 13907 us 13.9 us nconv=1 eoc=singleconv
1000 conv: 13483 us 13.4 nconv=1 eoc=seqconvn
9925 nc=4 eoc=single
22236 nc=4 eoc=seq scanconv
19966 nc=3 eoc=seq scanconv
16803 nc=2 eoc=seq scanconv
13483 nc=1 eoc=seq scanconv
2716.5 for 1000 conv -> 2.7uS -> 368ksps

*/
const COMMANDPARSER CommandParsersADCTST[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},

	{0,0,"---- General help: ADC ----"},
	{'K', CommandParserADCBench,"Benchmark ADC acquisition"},
	{0,0,"---- Dev ----"},
	{'1', CommandParserADCTest1,"stuff"},
	{'2', CommandParserADCTest2,"stuff"},
	{'3', CommandParserADCTest3,"stuff"},
	{'d', CommandParserADCTestDMA,"DMA"},
	{'b', CommandParserADCTestB,"bench"},
	{'A', CommandParserADCAcquire,"A,<channelmask>,<vbat>,<vref>,<temp>,<dt_us> Acquire every specified us microsecond."},
	{'R', CommandParserADCReadCont,"read continuously"},
	{'r', CommandParserADCRead,"read"},
	{'I', CommandParserADCTestInit,"I,<channelmask>,<vbat>,<vref>,<temp> Initialisation"},
	{'G', CommandParserADCTestReg,"Print ADC registers"},
	{'i', CommandParserADCTestInit2,"Restore init"},
	{'T', CommandParserADCTestTimerInit,"T,prescaler,reload: Initialise and start timer"},
	{'t', CommandParserADCTestTimer,"Stop timer"},
	{'x', CommandParserADCTestComplete,"Complete test"},

	{'f', CommandParserADCTestFgetc,"fgetc test"},

};
unsigned char CommandParsersADCTSTNum=sizeof(CommandParsersADCTST)/sizeof(COMMANDPARSER);




void mode_adc(void)
{
	mode_run("ADCTST",CommandParsersADCTST,CommandParsersADCTSTNum);
}


/******************************************************************************
	function: CommandParserADC
*******************************************************************************
	Parses a user command to enter the ADC mode.

	Command format: A,<mask>,<period>

	Stores the mask and period in eeprom.

	Parameters:
		buffer			-			Buffer containing the command
		size			-			Length of the buffer containing the command

******************************************************************************/
unsigned char CommandParserADCTest(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	CommandChangeMode(APP_MODE_ADCTEST);


	return 0;
}

unsigned char CommandParserADCTest1(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	HAL_StatusTypeDef s;
	fprintf(file_pri,"test\n");

	while(1)
	{
		s = HAL_ADC_Start(&hadc1);
		fprintf(file_pri,"Start: %d\n",s);

		unsigned long t1 = timer_us_get();
		s = HAL_ADC_PollForConversion(&hadc1,1000);
		unsigned long t2 = timer_us_get();
		fprintf(file_pri,"Poll: %d. %lu us\n",s,t2-t1);
		unsigned v = HAL_ADC_GetValue(&hadc1);

		fprintf(file_pri,"Value: %u\n",v);

		_delay_ms(300);

		if(fgetc(file_pri)!=-1)
			break;
	}
	s = HAL_ADC_Stop(&hadc1);
	fprintf(file_pri,"Stop: %d\n",s);
	return 0;
}
unsigned char CommandParserADCTest2(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	HAL_StatusTypeDef s;

	ADC_ChannelConfTypeDef sConfig = {0};

	fprintf(file_pri,"test\n");


	int channel;

	if(ParseCommaGetInt(buffer,1,&channel))
		return 2;

	fprintf(file_pri,"Configure with channel: %d\n",channel);

	switch(channel)
	{
	case 0:
		sConfig.Channel = ADC_CHANNEL_VREFINT;
		fprintf(file_pri,"vrefint\n");
		break;
	case 1:
		sConfig.Channel = ADC_CHANNEL_VBAT;
		fprintf(file_pri,"vbat\n");
		break;
	case 2:
		sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
		fprintf(file_pri,"temp\n");
		break;
	case 3:
		sConfig.Channel = ADC_CHANNEL_1;
		fprintf(file_pri,"ch 1\n");
		break;
	case 4:
		sConfig.Channel = ADC_CHANNEL_2;
		fprintf(file_pri,"ch 2\n");
		break;
	case 5:
	default:
		sConfig.Channel = ADC_CHANNEL_4;
		fprintf(file_pri,"ch 4\n");
		break;
	}

	//sConfig.Channel = ADC_CHANNEL_VREFINT;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);

	fprintf(file_pri,"Config channel: %d\n",s);
		return 0;
}
unsigned char CommandParserADCTest3(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	HAL_StatusTypeDef s;

	ADC_ChannelConfTypeDef sConfig = {0};

	fprintf(file_pri,"test\n");



	sConfig.Channel = ADC_CHANNEL_VREFINT;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	fprintf(file_pri,"Config channel: %d\n",s);

	sConfig.Channel = ADC_CHANNEL_VBAT;
	sConfig.Channel = ADC_CHANNEL_VREFINT;
	sConfig.Rank = ADC_REGULAR_RANK_2;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	fprintf(file_pri,"Config channel: %d\n",s);


	sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
	sConfig.Rank = ADC_REGULAR_RANK_3;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	fprintf(file_pri,"Config channel: %d\n",s);


	sConfig.Channel = ADC_CHANNEL_1;
	sConfig.Rank = ADC_REGULAR_RANK_4;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	fprintf(file_pri,"Config channel: %d\n",s);


	sConfig.Channel = ADC_CHANNEL_2;
	sConfig.Rank = ADC_REGULAR_RANK_5;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	fprintf(file_pri,"Config channel: %d\n",s);


	sConfig.Channel = ADC_CHANNEL_4;
	sConfig.Rank = ADC_REGULAR_RANK_6;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	s = HAL_ADC_ConfigChannel(&hadc1, &sConfig);
	fprintf(file_pri,"Config channel: %d\n",s);


	return 0;
}
unsigned char CommandParserADCTestDMA(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	//LL_ADC_REG_StartConversion(ADC1);
	//HAL_ADC_Start(&hadc1);

	//s = HAL_ADC_Start_IT(&hadc1);
	//HAL_TIM_Base_Start_IT

	//HAL_StatusTypeDef s = HAL_ADC_Start_IT(&hadc1);
	HAL_StatusTypeDef s = HAL_ADC_Start_DMA(&hadc1,(uint32_t*)_stm_adc_dmabuf,hadc1.Init.NbrOfConversion);
	/*LL_TIM_EnableIT_UPDATE(TIM15);
	LL_TIM_EnableUpdateEvent(TIM15);
	LL_TIM_EnableCounter(TIM15);
	LL_TIM_GenerateEvent_UPDATE(TIM15);*/

	return 0;
#if 0
	HAL_StatusTypeDef s;
	int dma;
	if(ParseCommaGetInt(buffer,1,&dma))
		return 2;


	/*if(dma)
		s = HAL_ADC_Start_IT(&hadc1);
	else
		s = HAL_ADC_Stop_IT(&hadc1);*/

	//memset(_stm_adc_dmabuf,0xff,STM_ADC_DMA_MAXCHANNEL*sizeof(unsigned short));

	// Only 8 bytes transferred
	// half callback after 26 32-bit (26*4 = 100 bytes)
	// full callback after 50 32-bit (50*4 = 200 bytes)
	// Number conv: 4 x 16-bit = 8 bytes

	// Number of DMA transfer must be equal to number of channels

	fprintf(file_pri,"ADC buffer level pre: %d\n",stm_adc_data_level());

	if(dma)
		//s = HAL_ADC_Start_DMA(&hadc1,(uint32_t*)_stm_adc_dmabuf,hadc1.Init.NbrOfConversion);
		s = stm_adc_acquireonce();
	else
		s = HAL_ADC_Stop_DMA(&hadc1);

	fprintf(file_pri,"Start or stop: %d result: %d\n",dma,s);
	//HAL_Delay(100);
	HAL_Delay(500);

	/*for(int i=0;i<STM_ADC_DMA_MAXCHANNEL;i++)
		fprintf(file_pri,"%04X ",(unsigned)_stm_adc_dmabuf[i]);
	fprintf(file_pri,"\n");*/

	fprintf(file_pri,"ADC buffer level post: %d\n",stm_adc_data_level());
#endif
	return 0;

}




unsigned char CommandParserADCTestB(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	HAL_StatusTypeDef s;
	fprintf(file_pri,"test\n");
	unsigned maxit=1000;

	while(1)
	{



		unsigned long t1 = timer_us_get();
		for(unsigned i=0;i<maxit;i++)
		{
			s = HAL_ADC_Start(&hadc1);
			s = HAL_ADC_PollForConversion(&hadc1,1000);
		}

		unsigned long t2 = timer_us_get();
		fprintf(file_pri,"Poll: %d. %lu us for %u\n",s,t2-t1,maxit);
		unsigned v = HAL_ADC_GetValue(&hadc1);

		fprintf(file_pri,"Value: %u\n",v);

		_delay_ms(300);

		if(fgetc(file_pri)!=-1)
			break;
	}
	s = HAL_ADC_Stop(&hadc1);
	fprintf(file_pri,"Stop: %d\n",s);
	return 0;
}
unsigned char CommandParserADCReadCont(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"Press key to stop:\n");

	while(1)
	{
		unsigned long pkt,time;
		unsigned short data[STM_ADC_MAXCHANNEL];
		memset(data,0xff,STM_ADC_MAXCHANNEL*sizeof(unsigned short));

		while(!stm_adc_data_getnext(data,0,&time,&pkt))
		{
			fprintf(file_pri,"Data:\n");
			for(int i=0;i<STM_ADC_MAXCHANNEL;i++)
			{
				fprintf(file_pri,"%04X ",data[i]);
			}
			fprintf(file_pri,"\n");
		}

		if(fgetc(file_pri)!=-1)
			break;
	}
	fprintf(file_pri,"end\n");

	return 0;
}

unsigned char CommandParserADCRead(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"ADC buffer level pre: %d\n",stm_adc_data_level());

	unsigned long pkt,time;
	unsigned nc;
	unsigned short data[STM_ADC_MAXCHANNEL];
	memset(data,0xff,STM_ADC_MAXCHANNEL*sizeof(unsigned short));

	while(!stm_adc_data_getnext(data,&nc,&time,&pkt))
	{
		fprintf(file_pri,"Data:\n");
		for(int i=0;i<STM_ADC_MAXCHANNEL;i++)
		{
			fprintf(file_pri,"%04X ",data[i]);
		}
		fprintf(file_pri,"\n");
	}

	fprintf(file_pri,"ADC buffer level post: %d\n",stm_adc_data_level());

	return 0;
}
unsigned char CommandParserADCTestInit(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	unsigned channels,vbat,vref,temp;

	if(ParseCommaGetInt(buffer,4,&channels,&vbat,&vref,&temp))
		return 2;

	stm_adc_init(channels,vbat,vref,temp);

	return 0;
}
unsigned char CommandParserADCTestReg(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	ADC_TypeDef *adc = hadc1.Instance;

	fprintf(file_pri,"SQR1: %08X SQR2: %08X SQR3: %08X SQR4: %08X\n",(unsigned)adc->SQR1,(unsigned)adc->SQR2,(unsigned)adc->SQR3,(unsigned)adc->SQR4);

	unsigned sq = (unsigned)adc->SQR1;
	fprintf(file_pri,"L: %d sq1: %d sq2: %d sq3: %d sq4: %d\n",sq&0b1111,(sq>>6)&0b11111,(sq>>12)&0b11111,(sq>>18)&0b11111,(sq>>24)&0b11111);
	sq = (unsigned)adc->SQR2;
	fprintf(file_pri,"sq5: %d sq6: %d sq7: %d sq8: %d sq9: %d\n",(sq>>0)&0b11111,(sq>>6)&0b11111,(sq>>12)&0b11111,(sq>>18)&0b11111,(sq>>24)&0b11111);

	fprintf(file_pri,"CR: %08X\n",adc->CR);

	return 0;
}
unsigned char CommandParserADCTestInit2(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	ADC_TypeDef *adc = hadc1.Instance;

	adc->SQR1=0;
	adc->SQR2=0;
	adc->SQR3=0;
	adc->SQR4=0;

	adc->SQR1=0x02012443;		// Restore default




	return 0;
}
unsigned char CommandParserADCTestTimerInit(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	unsigned prescaler,reload;
	if(ParseCommaGetInt(buffer,2,&prescaler,&reload))
		return 2;

	fprintf(file_pri,"Prescaler: %lu Reload: %lu\n",prescaler,reload);
	_stm_adc_init_timer(prescaler,reload);

	return 0;
}

unsigned char CommandParserADCTestTimer(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	_stm_adc_deinit_timer();


	return 0;
}

unsigned char CommandParserADCTestComplete(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	// Initialise the ADC with all the channels for timing reasons
	stm_adc_init(31,1,1,1);

	// Initialise the timer at 1 second
	_stm_adc_init_timer(20000,1000);		// Every second




	stm_adc_acquirecontinuously(1);



	// Start


	return 0;
}
unsigned char CommandParserADCAcquire(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	// Read parameters

	unsigned channels,vbat,vref,temp,micro;

	if(ParseCommaGetInt(buffer,5,&channels,&vbat,&vref,&temp,&micro))
		return 2;

	fprintf(file_pri,"Channels: %u Vbat: %u Vref: %u Temp: %u Micro: %u\n",channels,vbat,vref,temp,micro);


	unsigned prescaler,reload;

	prescaler=400;		// Prescaler to give 20uS resolution
	reload = micro/20;
	if(reload==0)
		reload = 1;
	if(reload>65535)
		reload=65535;


	// Start
	_stm_adc_acquire_start(channels,vbat,vref,temp,prescaler,reload);


	fprintf(file_pri,"Press key to stop.\n");
	unsigned long pkt,time;
	unsigned nc;
	unsigned short data[STM_ADC_MAXCHANNEL];

	while(1)
	{
		while(!stm_adc_data_getnext(data,&nc,&time,&pkt))
		{
			//fprintf(file_pri,"Data [%u]: [%lu %lu] ",nc,time,pkt);
			fprintf(file_pri,"%lu %lu ",time,pkt);
			for(int i=0;i<nc;i++)
			{
				fprintf(file_pri,"%04X ",data[i]);
			}
			fprintf(file_pri,"\n");
		}

		if(fgetc(file_pri)!=-1)
			break;
	}
	fprintf(file_pri,"End\n");
	// Get statistics
	unsigned long tot,lost;
	tot = stm_adc_stat_totframes();
	lost = stm_adc_stat_lostframes();
	fprintf(file_pri,"Tot frames: %lu lost frames: %lu\n",tot,lost);

	// Must stop
	stm_adc_acquire_stop();
	_delay_ms(1000);

	tot = stm_adc_stat_totframes();
	lost = stm_adc_stat_lostframes();
	fprintf(file_pri,"Tot frames: %lu lost frames: %lu\n",tot,lost);



	return 0;
}


unsigned char CommandParserADCBench(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;



	unsigned char channels[3] = {1,31,255};							// 1 external channel; 5 external channels; 5 ext+3 internal channels
	unsigned periodus[6] = {10000, 1000, 500, 250, 100, 50};		// Period in microseconds 100Hz, 1KHz, 2KHz, 4KHZ, 10KHz, 20KHz


	long int perf,refperf;
	unsigned long int mintime=4;


	stm_adc_acquire_stop();
	unsigned long p = stm_adc_perfbench_withreadout(mintime);
	refperf=p;
	fprintf(file_pri,"Reference performance: %lu\n",p);

	for(int rc = 0;rc<6;rc++)
	{
		for(int nc = 0; nc<3; nc++)
		{
			stm_adc_acquire_start_us(channels[nc],0,0,0,periodus[rc]);

			perf = stm_adc_perfbench_withreadout(mintime);

			stm_adc_acquire_stop();

			long load = 100-(perf*100/refperf);
			if(load<0)
				load=0;

			fprintf(file_pri,"\tBenchmark dt=%u us channels=%u: perf: %lu (instead of %lu). CPU load %lu %%\n",periodus[rc],channels[nc],perf,refperf,load);
		}
	}







	return 0;
}
unsigned char CommandParserADCTestFgetc(char *buffer,unsigned char size)
{
	(void)size; (void) buffer;

	unsigned long t1,t2;
	unsigned n=100000;
	char buf;

	t1 = timer_us_get();
	for(unsigned i=0;i<n;i++)
	{
		fgetc(file_pri);
	}
	t2 = timer_us_get();
	fprintf(file_pri,"fgetc time: %lu us for %u calls. %u us/call\n",t2-t1,n,(t2-t1)/n);

	t1 = timer_us_get();
	for(unsigned i=0;i<n;i++)
	{
		fread(&buf,1,1,file_pri);
	}
	t2 = timer_us_get();
	fprintf(file_pri,"fread time: %lu us for %u calls. %u us/call\n",t2-t1,n,(t2-t1)/n);


	t1 = timer_us_get();
	unsigned long nnn=0;
	for(unsigned i=0;i<n;i++)
	{
		nnn+=fischar(file_pri);
	}
	t2 = timer_us_get();
	fprintf(file_pri,"buffer_get time: %lu us for %u calls. %u us/call (%u)\n",t2-t1,n,(t2-t1)/n,nnn);


	return 0;
}
