/*
 * mode_sample_multimodal.c
 *
 *  Created on: 5 Feb 2020
 *      Author: droggen
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
#include "stmadc.h"
#include "serial.h"
#include "pkt.h"
#include "wait.h"
#include "uiconfig.h"
#include "helper.h"
#include "system.h"
#include "system-extra.h"
#include "pkt.h"
#include "ufat.h"
#include "mpu.h"
#include "mpu_config.h"
#include "stmdfsdm.h"
#include "stmadc.h"
#include "mode.h"
#include "mode_global.h"
#include "mode_sample.h"
#include "mode_sample_adc.h"
#include "mode_sample_sound.h"
#include "mode_sample_motion.h"
#include "mode_sample_multimodal.h"
#include "commandset.h"
#include "ltc2942.h"

unsigned mode_sample_multimodal_mode;

const COMMANDPARSER CommandParsersMultimodal[] =
{
	{'H', CommandParserHelp,help_h},
	{'N', CommandParserAnnotation,help_annotation},
	{'F', CommandParserStreamFormat,help_f},
	{'i',CommandParserInfo,help_info},
	{'!', CommandParserQuit,help_quit}
};
const unsigned char CommandParsersMultimodalNum=sizeof(CommandParsersMultimodal)/sizeof(COMMANDPARSER);


/******************************************************************************
	function: CommandParserSampleMultimodal
*******************************************************************************
	Parses a user command to enter the multimodal mode.

	Command format: U,[<mode>,<adcmask>,<adcperiod>[[,<logfile>[,<duration>]]]


	Parameters:
		buffer			-			Buffer containing the command
		size			-			Length of the buffer containing the command

******************************************************************************/
unsigned char CommandParserSampleMultimodal(char *buffer,unsigned char size)
{
	mode_sample_param_duration = 0;
	mode_sample_param_logfile = -1;
	mode_adc_fastbin=0;

	unsigned modemap[4] = {MULTIMODAL_MPU|MULTIMODAL_SND|MULTIMODAL_ADC,MULTIMODAL_MPU|MULTIMODAL_SND,MULTIMODAL_MPU|MULTIMODAL_ADC,MULTIMODAL_SND|MULTIMODAL_ADC};	// Bitmap indicating which modality to include: MPU | SND | ADC


	int np = ParseCommaGetNumParam(buffer);
	if(np==0)
	{
		fprintf(file_pri,"Available multimodal modes:\n");
		fprintf(file_pri,"\t0: MPU-200Hz(Acc + Gyro + Mag + Quaternion) + Sound-16KHz + ADC(mask)\n");
		fprintf(file_pri,"\t1: MPU-200Hz(Acc + Gyro + Mag + Quaternion) + Sound-16KHz\n");
		fprintf(file_pri,"\t2: MPU-200Hz(Acc + Gyro + Mag + Quaternion) + ADC(mask)\n");
		fprintf(file_pri,"\t3: Sound-16KHz + ADC(mask)\n");
		return 0;
	}

	if(np>0 && np<3)
	{
		fprintf(file_pri,"Invalid number of parameters\n");
		return 2;
	}
	switch(np)
	{
		case 3:
			if(ParseCommaGetInt(buffer,3,&mode_sample_multimodal_mode,&mode_adc_mask,&mode_adc_period))
				return 2;
			break;
		case 4:
			if(ParseCommaGetInt(buffer,4,&mode_sample_multimodal_mode,&mode_adc_mask,&mode_adc_period,&mode_sample_param_logfile))
				return 2;
			break;
		case 5:
		default:
			if(ParseCommaGetInt(buffer,5,&mode_sample_multimodal_mode,&mode_adc_mask,&mode_adc_period,&mode_sample_param_logfile,&mode_sample_param_duration))
				return 2;
			break;
	}

	if(mode_sample_multimodal_mode>3)
		return 2;

	// Convert the mode to a modemap
	mode_sample_multimodal_mode = modemap[mode_sample_multimodal_mode];

	if(mode_sample_multimodal_mode & MULTIMODAL_ADC)
	{
		// Check ADC parameters
		if(mode_sample_adc_checkmask())
			return 2;

		if(mode_sample_adc_period())
			return 2;
	}

	CommandChangeMode(APP_MODE_SAMPLEMULTIMODAL);


	return 0;
}



/******************************************************************************
	function: mode_sample_multimodal
*******************************************************************************
	Multimodal mode loop.

	This function initialises the board for ADC acquisition and enters a continuous
	sample/stream loop.

******************************************************************************/
void mode_sample_multimodal(void)
{
	unsigned short data[STM_ADC_MAXCHANNEL];		// Buffer to hold a copy of ADC data
	STM_DFSMD_TYPE audbuf[STM_DFSMD_BUFFER_SIZE];	// Buffer for audio data
	unsigned long audbufms,audbufpkt;				// Audio metadata

	//unsigned long stat_timems_end=0;
	int putbufrv;

	mode_sample_sound_param_framebased=1;			// Sound must be frame-based always


	fprintf(file_pri,"SMPMULTIMODAL>\n");

	// Packet init
	packet_init(&adcpacket,"DAA",3);
	PACKET mpupacket;
	packet_init(&mpupacket,"DXX",3);


	// Init log file
	mode_sample_file_log=0;										// Initialise log to null
	if(mode_sample_startlog(mode_sample_param_logfile))			// Initialise log will be initiated if needed here
		goto mode_sample_multimodal_end;



	// Load mode configuration
	stream_load_persistent_frame_settings();

	// Some info
	//fprintf(file_pri,"Sampling ADC: channel mask: %02X; period: %u\n",mode_adc_mask,mode_adc_period);
	//fastbin: %u; logfile: %d; duration: %d\n",mode_adc_mask,mode_adc_period,mode_adc_fastbin,mode_sample_param_logfile,mode_sample_param_duration);


	// Update the current annotation
	CurrentAnnotation=0;

	if(mode_sample_multimodal_mode & MULTIMODAL_ADC)
	{
		// Initialise ADC with channels and time; round time to multiple of 10uS
		fprintf(file_pri,"ADC sampling: period: %u mask: %02X\n",mode_adc_period,mode_adc_mask);
		stm_adc_acquire_start(mode_adc_mask&0b11111,(mode_adc_mask>>5)&1,(mode_adc_mask>>6)&1,(mode_adc_mask>>7)&1,199,(mode_adc_period/10)-1);		// 20MHz/(199+1)=10uS and period-1 to ensure microseconds
	}
	if(mode_sample_multimodal_mode & MULTIMODAL_MPU)
	{
		fprintf(file_pri,"MPU sampling\n");
		//mpu_config_motionmode(MPU_MODE_225HZ_ACCGYROMAG100QUAT,1);
		mpu_config_motionmode(MPU_MODE_102HZ_ACCGYROMAG100QUAT,1);
	}
	if(mode_sample_multimodal_mode & MULTIMODAL_SND)
	{
		fprintf(file_pri,"SND sampling\n");
		// Set audio mode
		//stm_dfsdm_init(STM_DFSMD_INIT_16K);
		stm_dfsdm_init(STM_DFSMD_INIT_8K);
	}


	// Clear statistics and initialise timers.
	mode_sample_clearstat();
	stm_dfsdm_data_clear();
	stm_adc_data_clear();
	mpu_data_clear();

	while(!CommandShouldQuit())
	{
		// Depending on the "fastbin" mode either go through the fast "keypress" exit, or the slower command parser.
		/*if(mode_adc_fastbin)
		{
			// Keypress to quit - faster than parsing user commands.
			if(fischar(file_pri))		// Faster than fgetc
				break;
		}
		else
		{*/
			// Process all user commands
			while(CommandProcess(CommandParsersMultimodal,CommandParsersMultimodalNum));
		//}

		// Send data to primary stream or to log if available
		FILE *file_stream;
		if(mode_sample_file_log)
			file_stream=mode_sample_file_log;
		else
			file_stream=file_pri;

		stat_t_cur=timer_ms_get();						// Current time
		_MODE_SAMPLE_CHECK_DURATION_BREAK;				// Stop after duration, if specified
		_MODE_SAMPLE_CHECK_BATTERY_BREAK;				// Stop if battery too low
		_MODE_SAMPLE_BLINK;								// Blink

		if(mode_stream_format_enableinfo)						// Display info if enabled
		{
			if(stat_t_cur-stat_time_laststatus>MODE_SAMPLE_INFOEVERY_MS)
			{
				stream_adc_status(file_pri,0);
				stream_sound_status(file_pri,0);
				stream_motion_status(file_pri,0);
				stat_time_laststatus+=MODE_SAMPLE_INFOEVERY_MS;
				stat_wakeup=0;
			}
		}

		// ADC data
		if(mode_sample_multimodal_mode & MULTIMODAL_ADC)
		{
			// Stream existing data
			_MODE_SAMPLE_ADC_GET_AND_SEND;
		}	// ADC
		// SND data
		if(mode_sample_multimodal_mode & MULTIMODAL_SND)
		{
			// Stream existing data
			_MODE_SAMPLE_SOUND_GET_AND_SEND;
		} // SND
		// MPU data
		if(mode_sample_multimodal_mode & MULTIMODAL_MPU)
		{
			// Stream existing data
			_MODE_SAMPLE_MPU_GET_AND_SEND;
		} // MPU

		// Sleep
		_MODE_SAMPLE_SLEEP;
	}	// Continuous loop
	//stat_timems_end = timer_ms_get();

	// Stop sampling
	if(mode_sample_multimodal_mode & MULTIMODAL_ADC)
		stm_adc_acquire_stop();
	if(mode_sample_multimodal_mode & MULTIMODAL_MPU)
		mpu_config_motionmode(MPU_MODE_OFF,0);
	if(mode_sample_multimodal_mode & MULTIMODAL_SND)
		stm_dfsdm_init(STM_DFSMD_INIT_OFF);

	// End the logging, if logging was ongoing
	mode_sample_logend();

	// Clear LED
	system_led_off(LED_GREEN);

	// Print statistics
	stream_adc_status(file_pri,0);
	stream_sound_status(file_pri,0);
	stream_motion_status(file_pri,0);


mode_sample_multimodal_end:
	fprintf(file_pri,"<SMPMULTIMODAL\n");
}
