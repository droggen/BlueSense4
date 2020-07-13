/*
	file: mode_sample
	
	Contains common functionalities between mode_sample_adc and mode_sample_motion.
	
	Offers:
	
	* mode_sample_logend: 			to terminate logs
	* help_samplelog				help string
	* mode_sample_file_log			FILE* for logging
	
	*TODO*
	
	* Statistics when logging could display log-only information (samples acquired, samples lost, samples per second)
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
#include "serial.h"
//#include "pkt.h"
#include "wait.h"
#include "uiconfig.h"
#include "helper.h"
#include "system.h"
#include "ufat.h"
#include "mode.h"
#include "mode_sample.h"
#include "mode_global.h"
#include "commandset.h"
#include "mode_sample_motion.h"
#include "mode_sample_adc.h"
#include "mode_sample_sound.h"
#include "mode_sample_multimodal.h"
#include "ltc2942.h"

unsigned long stat_timems_start,stat_t_cur,stat_wakeup,stat_time_laststatus;
unsigned long int time_lastblink;

int	mode_sample_param_duration;
int mode_sample_param_logfile;

// Log file used by the modes mode_adc and mode_motionstream
FILE *mode_sample_file_log;				

const char help_samplestatus[] = "Battery and logging status";
const char help_samplelog[] = "L[,<lognum>]: without parameter logging is stopped, otherwise logging starts on lognum";


/******************************************************************************
	function: CommandParserSampleLog
*******************************************************************************	
	Parses a user command to enable logging to a specified logfile. 
	
	This is used by mode_sample_adc and mode_sample_motion. It relies on the global variable
	mode_file_log.
	
	
	Command format: L[,<lognum>]
	
	Parameters:
		buffer			-			Buffer containing the command
		size			-			Length of the buffer containing the command	

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid 
******************************************************************************/
unsigned char CommandParserSampleLog(char *buffer,unsigned char size)
{
	unsigned int lognum;
	
	if(size==0)
	{
		// L-only was passed: stop logging if logging ongoing
		if(mode_sample_file_log==0)
		{
			fprintf(file_pri,"No ongoing logging\n");
			return 1;
		}
		mode_sample_logend();
		return 0;		
	}
	
	// L,lognum was passed: parse
	unsigned char rv;
	rv = ParseCommaGetInt((char*)buffer,1,&lognum);
	if(rv)
		return 2;		// Message invalid
	
	rv = mode_sample_startlog(lognum);
	return rv;			// Returns 0 (ok) or 1 (execution error but message valid)
}

/******************************************************************************
	function: mode_sample_startlog
*******************************************************************************	
	Starts a log on the specified logfile. 
	If the filesystem is unavailable or a log is ongoing returns an error.
	
	If lognum is negative, does nothing.
	
	Parameters:
		lognum		-			Log to open. If negative, do nothing and return success.

	Returns:
		0			-			Success
		1			-			Error

******************************************************************************/
unsigned char mode_sample_startlog(int lognum)
{
	// Log negative means no log to start
	if(lognum<0)
	{
		return 0;
	}
		
	// Non-negative: start logging
	
	// Check if the filesystem has been initialised successfully
	if(!ufat_available())
	{
		fprintf(file_pri,"No filesystem available\n");
		// Return an error
		return 1;
	}
	// Check if logging ongoing
	if(mode_sample_file_log!=0)
	{
		fprintf(file_pri,"Already logging; stop before restarting\n");
		// Return an error
		return 1;
	}
	
	//printf("not logging therefore start logging\n");
	
	// Not logging therefore start logging
	fprintf(file_pri,"Logging on %u\n",lognum);

	mode_sample_file_log = ufat_log_open(lognum);
	if(!mode_sample_file_log)
	{
		fprintf(file_pri,"Error opening log\n");
		return 1;
	}
	return 0;
}


/******************************************************************************
	function: mode_sample_logend
*******************************************************************************	
	Checks whether logging was in progress, and if yes closes the log

******************************************************************************/
void mode_sample_logend(void)
{
	if(mode_sample_file_log)
	{
		fprintf(file_pri,"Terminating logging\n");
		mode_sample_file_log=0;
		ufat_log_close();
	}
}


/******************************************************************************
	function: mode_sample_clearstat
*******************************************************************************
	Clear sampling statistics

******************************************************************************/
void mode_sample_clearstat(void)
{
	//stat_totsample=0;
	stat_mpu_samplesendfailed=0;
	stat_mpu_samplesendok=0;
	stat_snd_samplesendfailed=0;
	stat_snd_samplesendok=0;
	stat_adc_samplesendfailed=0;
	stat_adc_samplesendok=0;
	stat_wakeup=0;

	stat_t_cur = time_lastblink = stat_time_laststatus = stat_timems_start = timer_ms_get();
}


/******************************************************************************
	function: stream_load_persistent_frame_settings
*******************************************************************************
	Load the stream format common to many log functions.

	Parameters:
		-

	Returns:
		-
******************************************************************************/
void stream_load_persistent_frame_settings(void)
{
	// Load the persistent data
	mode_stream_format_bin=ConfigLoadStreamBinary();
	mode_stream_format_ts=ConfigLoadStreamTimestamp();
	mode_stream_format_bat=ConfigLoadStreamBattery();
	mode_stream_format_pktctr=ConfigLoadStreamPktCtr();
	mode_stream_format_label = ConfigLoadStreamLabel();
	mode_stream_format_enableinfo = ConfigLoadEnableInfo();
}



void mode_sample_storebatinfo(void)
{
	// Store batstat in a logfile
	if(ufat_available())
	{
		int nl = ufat_log_getnumlogs()-1;
		FILE *mode_sample_file_log = ufat_log_open(nl);
		if(!mode_sample_file_log)
		{
			fprintf(file_pri,"Error opening log file %d to store battery info\n",nl);
			return;
		}
		else
		{
			//log_printstatus();
			fprintf(mode_sample_file_log,"Battery log:\n");
			ltc2942_print_longbatstat(mode_sample_file_log);
			fprintf(mode_sample_file_log,"Battery log end\n");

			// Print statistics
			if(mode_sample_multimodal_mode & MULTIMODAL_ADC)
				stream_adc_status(mode_sample_file_log,0);
			if(mode_sample_multimodal_mode & MULTIMODAL_SND)
				stream_sound_status(mode_sample_file_log,0);
			if(mode_sample_multimodal_mode & MULTIMODAL_MPU)
				stream_motion_status(mode_sample_file_log,0);

			mode_sample_file_log=0;
			ufat_log_close();
		}
	}

}



