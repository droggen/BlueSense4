/*
	file: mode_motionstream
	
	Sampling and streaming/logging of the motion sensor.
	
	This function contains the 'M' mode of the sensor which allows to acquire the data from the motion sensor.
	
	This mode contains conditional codepath depending on the #defines FIXEDPOINTQUATERNION, FIXEDPOINTQUATERNIONSHIFT and ENABLEQUATERNION.
	
	*TODO*
	
	* Statistics when logging could display log-only information (samples acquired, samples lost, samples per second)
*/


#include <stdio.h>
#include <string.h>

#include "mpu_geometry.h"
#include "global.h"
#include "mpu.h"
#include "pkt.h"
#include "wait.h"
#include "system.h"
#include "system-extra.h"
#include "helper.h"
#include "serial.h"
#include "mode_sample.h"
#include "mode_sample_motion.h"
#include "mode_global.h"
#include "mpu_config.h"
#include "commandset.h"
#include "uiconfig.h"
#include "ufat.h"
//#include "sd.h"
#include "mode.h"
#include "ltc2942.h"
//#include "a3d.h"
#include "stmdfsdm.h"

// Volatile parameter of the mode 
MODE_SAMPLE_MOTION_PARAM mode_sample_motion_param;
float yaw,pitch,roll;

// If quaternions are enabled: conversion factors
#if ENABLEQUATERNION==1
#if FIXEDPOINTQUATERNION==1
FIXEDPOINTTYPE atog=1.0k/16384.0k;
#else
float atog=1.0/16384.0;
#endif
#endif




MPUMOTIONDATA mpumotiondata;
MPUMOTIONGEOMETRY mpumotiongeometry;
unsigned long stat_mpu_samplesendfailed;
unsigned long stat_mpu_samplesendok;


const char help_batbench[] = "Battery benchmark";

const COMMANDPARSER CommandParsersMotionStream[] =
{ 
	{'H', CommandParserHelp,help_h},
	//{'W', CommandParserSwap,help_w},
	{'F', CommandParserStreamFormat,help_f},
	{'L', CommandParserSampleLogMPU,help_samplelog},
	{'Z',CommandParserSync,help_z},
	//{'i',CommandParserInfo,help_info},
	{'N', CommandParserAnnotation,help_annotation},
	{'Q', CommandParserBatteryInfoLong,help_batterylong},
	{'q', CommandParserBatteryInfo,help_battery},
	{'s', CommandParserSampleStatus,help_samplestatus},
	{'x', CommandParserBatBench,help_batbench},
	{'!', CommandParserQuit,help_quit}
};
const unsigned char CommandParsersMotionStreamNum=sizeof(CommandParsersMotionStream)/sizeof(COMMANDPARSER); 

void mode_sample_motion_setparam(unsigned char mode, int logfile, int duration)
{
	mode_sample_motion_param.mode=mode;
	mode_sample_motion_param.logfile=logfile;
	mode_sample_motion_param.duration=duration*1000l;		// Store the duration in milliseconds
	
	//printf("duration: %lu\n",mode_sample_motion_param.duration);
	//printf("mode_sample_motion_setparam: %d %d %d\n",mode_sample_motion_param.mode,mode_sample_motion_param.logfile,mode_sample_motion_param.duration);
}

void clearstat(void)
{
	mode_sample_clearstat();
	
	// If there was a successful change in logging (start, stop, etc) then reset the statistics.
	// Clear MPU data and stat
	mpu_data_clear();
	// Clear sound statistics
	stm_dfsdm_data_clear();
}

unsigned char CommandParserSampleLogMPU(char *buffer,unsigned char size)
{
	// MPU specific code to start/stop the log
	unsigned char rv=CommandParserSampleLog(buffer,size);
	if(!rv)
	{		
		clearstat();		// Clear statistics related to streaming/logging
	}
	return rv;
}

unsigned char CommandParserSampleStatus(char *buffer,unsigned char size)
{
	stream_motion_status(file_pri,mode_stream_format_bin);
	return 0;
}

unsigned char CommandParserBatBench(char *buffer,unsigned char size)
{
	ltc2942_print_longbatstat(file_pri);
	return 0;
}

// Builds the text string
unsigned char stream_sample_text(FILE *f)
{
	char motionstream[192];		// Buffer to build the string of motion data
	char *strptr = motionstream;
	
	// Format packet counter
	if(mode_stream_format_pktctr)
	{
		strptr=format1u32(strptr,mpumotiondata.packetctr);
	}
	// Format timestamp
	if(mode_stream_format_ts)
	{
		strptr = format1u32(strptr,mpumotiondata.time);
	}
	// Format battery
	if(mode_stream_format_bat)
	{
		strptr = format1u16(strptr,system_getbattery());
	}
	// Format label
	if(mode_stream_format_label)
	{
		strptr=format1u16(strptr,CurrentAnnotation);
	}
	// Formats acceleration if selected
	if(sample_mode & MPU_MODE_BM_A)
		strptr = format3s16(strptr,mpumotiondata.ax,mpumotiondata.ay,mpumotiondata.az);
	// Formats gyro if selected
	if(sample_mode & MPU_MODE_BM_G)
		strptr = format3s16(strptr,mpumotiondata.gx,mpumotiondata.gy,mpumotiondata.gz);
	// Formats magnetic if selected
	if(sample_mode & MPU_MODE_BM_M)
		strptr = format3s16(strptr,mpumotiondata.mx,mpumotiondata.my,mpumotiondata.mz);
	// Formats quaternions if selected
	
	if(sample_mode & MPU_MODE_BM_Q)
	{
		#if ENABLEQUATERNION==1
			#if FIXEDPOINTQUATERNION==1
				strptr = format4fract16(strptr,mpumotiongeometry.q0,mpumotiongeometry.q1,mpumotiongeometry.q2,mpumotiongeometry.q3);
			#else
				strptr = format4qfloat(strptr,mpumotiongeometry.q0,mpumotiongeometry.q1,mpumotiongeometry.q2,mpumotiongeometry.q3);
			#endif
		#endif
	}
	if(sample_mode & MPU_MODE_BM_E)
	{
		strptr = format3float(strptr,mpumotiongeometry.yaw,mpumotiongeometry.pitch,mpumotiongeometry.roll);
	}
	if(sample_mode & MPU_MODE_QDBG)
	{
		floattoa(mpumotiongeometry.alpha,strptr);
		strptr+=7;
		*strptr=' ';
		strptr++;
		floatqtoa(mpumotiongeometry.x,strptr);
		strptr+=6;
		*strptr=' ';
		strptr++;
		floatqtoa(mpumotiongeometry.y,strptr);
		strptr+=6;
		*strptr=' ';
		strptr++;
		floatqtoa(mpumotiongeometry.z,strptr);
		strptr+=6;
		*strptr=' ';
		strptr++;
	}
	*strptr='\n';		
	strptr++;

	if(fputbuf(f,motionstream,strptr-motionstream))
		return 1;
	return 0;	
}
unsigned char stream_sample_bin(FILE *f)
{
	PACKET p;
	packet_init(&p,"DXX",3);
	
	
	// Format packet counter
	if(mode_stream_format_pktctr)
	{
		packet_add32_little(&p,mpumotiondata.packetctr);
	}
	// Format timestamp
	if(mode_stream_format_ts)
	{
		packet_add16_little(&p,mpumotiondata.time&0xffff);
		packet_add16_little(&p,(mpumotiondata.time>>16)&0xffff);
	}
	// Format battery
	if(mode_stream_format_bat)
		packet_add16_little(&p,system_getbattery());
	if(mode_stream_format_label)
		packet_add16_little(&p,CurrentAnnotation);
		
	// Formats acceleration if selected
	if(sample_mode & MPU_MODE_BM_A)
	{
		packet_add16_little(&p,mpumotiondata.ax);
		packet_add16_little(&p,mpumotiondata.ay);
		packet_add16_little(&p,mpumotiondata.az);
	}
	// Formats gyro if selected
	if(sample_mode & MPU_MODE_BM_G)
	{
		packet_add16_little(&p,mpumotiondata.gx);
		packet_add16_little(&p,mpumotiondata.gy);
		packet_add16_little(&p,mpumotiondata.gz);
	}
	// Formats magnetic if selected
	if(sample_mode & MPU_MODE_BM_M)
	{
		packet_add16_little(&p,mpumotiondata.mx);
		packet_add16_little(&p,mpumotiondata.my);
		packet_add16_little(&p,mpumotiondata.mz);
	}
	// Formats quaternions if selected
	if(sample_mode & MPU_MODE_BM_Q)
	{	
		#if ENABLEQUATERNION==1
			#if FIXEDPOINTQUATERNION==1
				_Accum k;
				signed short v;
				k = q0*10000k; v = k;
				packet_add16_little(&p,v);
				k = q1*10000k; v = k;
				packet_add16_little(&p,v);
				k = q2*10000k; v = k;
				packet_add16_little(&p,v);
				k = q3*10000k; v = k;
				packet_add16_little(&p,v);	
			#else
				float k;
				signed short v;
				k = mpumotiongeometry.q0*10000.0; v = k;
				packet_add16_little(&p,v);
				k = mpumotiongeometry.q1*10000.0; v = k;
				packet_add16_little(&p,v);
				k = mpumotiongeometry.q2*10000.0; v = k;
				packet_add16_little(&p,v);
				k = mpumotiongeometry.q3*10000.0; v = k;
				packet_add16_little(&p,v);	
			#endif
		#else
		packet_add16_little(&p,1);
		packet_add16_little(&p,0);
		packet_add16_little(&p,0);
		packet_add16_little(&p,0);
		#endif
	}
	
	packet_end(&p);
	packet_addchecksum_fletcher16_little(&p);
	int s = packet_size(&p);
	if(fputbuf(f,(char*)p.data,s))
		return 1;

	return 0;
}



unsigned char stream_sample(FILE *f)
{
	if(mode_stream_format_bin==0)
		return stream_sample_text(f);
	else
		return stream_sample_bin(f);
	return 0;
}

/******************************************************************************
	function: stream_motion_status
*******************************************************************************	
	Sends to a specified file information about the current streaming/logging state,
	including battery informationa and log status.
	
	In binary streaming mode an information packet with header DII is created.
	The packet definition string is: DII;is-s-siiiiic;f

	In text streaming mode an easy to parse string is sent prefixed by '#'.
	
	
	
	Paramters:
		f		-	File to which to send the status
		bin		-	Indicate status in binary or text

	Returns:
		Nothing
*******************************************************************************/
void stream_motion_status(FILE *f,unsigned char bin)
{
	unsigned long wps = stat_wakeup*1000l/(stat_t_cur-stat_time_laststatus);
	//unsigned long cnt_int, cnt_sample_tot, cnt_sample_succcess, cnt_sample_errbusy, cnt_sample_errfull;
	unsigned long cnt_sample_errbusy, cnt_sample_errfull;
	
	//mpu_getstat(&cnt_int, &cnt_sample_tot, &cnt_sample_succcess, &cnt_sample_errbusy, &cnt_sample_errfull);
	//mpu_getstat(0, 0, 0, &cnt_sample_errbusy, &cnt_sample_errfull);

	unsigned long totframes = mpu_stat_totframes();
	unsigned long lostframes = mpu_stat_lostframes();
	
	//if(bin==0)
	{
		// Information text
		fprintf(f,"# MPU t=%lu ms; %s",stat_t_cur-stat_timems_start,ltc2942_last_strstatus());
		fprintf(f,"; wps=%lu; errbsy=%lu; errfull=%lu; errsend=%lu; spl=%lu",wps,cnt_sample_errbusy,cnt_sample_errfull,stat_mpu_samplesendfailed,totframes);
		fprintf(f,"; log=%u KB; logmax=%u KB; logfull=%u %%\n",(unsigned)(ufat_log_getsize()>>10),(unsigned)(ufat_log_getmaxsize()>>10),(unsigned)(ufat_log_getsize()/(ufat_log_getmaxsize()/100l)));
#warning Add sample per second (either from last status info or from start)
	}
	// Binary not implemented
	/*else
	{
		// Information packet
		PACKET p;
		packet_init(&p,"DII",3);
		packet_add32_little(&p,stat_t_cur-stat_timems_start);		
		packet_add16_little(&p,ltc2942_last_mV());
		packet_add16_little(&p,ltc2942_last_mA());
		packet_add16_little(&p,ltc2942_last_mW());
		packet_add32_little(&p,wps);
		packet_add32_little(&p,stat_totsample);
		packet_add32_little(&p,stat_samplesendfailed);
		packet_add32_little(&p,ufat_log_getsize()>>10);
		packet_add32_little(&p,ufat_log_getmaxsize()>>10);
		packet_add8(&p,ufat_log_getsize()/(ufat_log_getmaxsize()/100l));
		packet_end(&p);
		packet_addchecksum_fletcher16_little(&p);
		int s = packet_size(&p);
		fputbuf(f,(char*)p.data,s);
	}*/
}



void stream_start(void)
{
	stream_load_persistent_frame_settings();
	
	fprintf(file_pri,"Acc scale: %d\n",mpu_getaccscale());
	fprintf(file_pri,"Gyro scale: %d\n",mpu_getgyroscale());
	
	mpu_config_motionmode(mode_sample_motion_param.mode,1);	

	// Clear statistics
	clearstat();		// Clear statistics related to mpu/streaming/logging
}
void stream_stop(void)
{
	mpu_config_motionmode(MPU_MODE_OFF,0);
}

/******************************************************************************
	function: CommandParserMotion
*******************************************************************************	
	Parses the Motion command: M[,mode[,logfile[,duration]]
	
	If no parameter is passed, it prints the available motion modes.
	Single parameter: motion mode
	Two parameters: motion mode and logfile on which to store data
	Three parameters: motion mode, logfile, and duration in seconds to run this mode before exiting.
	
	
	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid 
******************************************************************************/
unsigned char CommandParserMotion(char *buffer,unsigned char size)
{
	unsigned char rv;
	int mode,lognum,duration;
	
	// Parse from the smallest number of arguments to the largest
	rv = ParseCommaGetInt((char*)buffer,1,&mode);
	if(rv)
	{
		// No argument - display available modes and returns successfully
		mpu_printmotionmode(file_pri);
		return 0;
	}
	
	// One argument - check validity
	if(mode<0 || mode>MOTIONCONFIG_NUM)
		return 2;	// Invalid

	//printf("Mode: %d\n",mode);
	
	// Check if two arguments
	rv = ParseCommaGetInt((char*)buffer,2,&mode,&lognum);
	if(rv==0)
	{
		// Two arguments were parsed
		//printf("lognum: %d\n",lognum);
		
		// Check if three arguments
		rv = ParseCommaGetInt((char*)buffer,3,&mode,&lognum,&duration);
		if(rv==0)
		{
			//printf("Duration: %d\n",duration);
			mode_sample_motion_setparam(mode,lognum,duration);
		}
		else
			mode_sample_motion_setparam(mode,lognum,0);
	}
	else
		mode_sample_motion_setparam(mode,-1,0);
		
	CommandChangeMode(APP_MODE_MOTIONSTREAM);
	
	return 0;
}

/******************************************************************************
	function: mode_motionstream
*******************************************************************************	
	Streaming mode loop	
******************************************************************************/
// TODO: each time that a logging starts, stops, or change reset all the statistics and clear the buffers, including resetting the time since running
void mode_sample_motion(void)
{
	unsigned char putbufrv;
	
	fprintf(file_pri,"SMPLMOTION>\n");


	// Initialise the sleep mode
	//set_sleep_mode(SLEEP_MODE_IDLE);
	//sleep_enable();

	mode_sample_file_log=0;										// Initialise log to null 
	mode_sample_startlog(mode_sample_motion_param.logfile);		// Initialise log will be initiated if needed here

	stream_start();
	
	fprintf(file_pri,"Sample rate: %u\n",_mpu_samplerate);

	
	clearstat();
	
	//printf("atog: %f\n",atog);
	//printf("mpu_gtorps: %f\n",mpu_gtorps);
	//printf("beta: %f\n",beta);
	
	
	while(1)
	{
	
		
		// Process user commands only if we do not run for a specified duration
		if(mode_sample_motion_param.duration==0)
		{
			while(CommandProcess(CommandParsersMotionStream,CommandParsersMotionStreamNum));		
			if(CommandShouldQuit())
				break;
		}
		stat_t_cur=timer_ms_get();		// Current time
		if(mode_sample_motion_param.duration)					// Check if maximum mode time is reached
		{
			if(stat_t_cur-stat_timems_start>=mode_sample_motion_param.duration)
				break;			
		}
		if(ltc2942_last_mV()<BATTERY_VERYVERYLOW)				// Stop if batter too low
		{
			fprintf(file_pri,"Low battery, interrupting\n");
			break;
		}
		if(stat_t_cur-time_lastblink>1000)						// Blink
		{		
			system_led_toggle(0b100);
			time_lastblink=stat_t_cur;
		}		
		// Display info if enabled
		if(mode_stream_format_enableinfo)
		{
			if(stat_t_cur-stat_time_laststatus>MODE_SAMPLE_INFOEVERY_MS)
			{
				stream_motion_status(file_pri,mode_stream_format_bin);
				stat_time_laststatus+=MODE_SAMPLE_INFOEVERY_MS;
				stat_wakeup=0;
			}
		}
		

		// Stream existing data
		unsigned char l = mpu_data_level();
		if(!l)
		{
			//sleep_cpu();
			stat_wakeup++;
		}
		else
		{
			for(unsigned char i=0;i<l;i++)
			{
				// Get the data from the auto read buffer; if no data available break
				if(mpu_data_getnext(&mpumotiondata,&mpumotiongeometry))
					break;
				
				//fprintf(file_pri,"%lu\n",mpu_compute_geometry_time());
			
			
				// Send data to primary stream or to log if available
				FILE *file_stream;
				if(mode_sample_file_log)
					file_stream=mode_sample_file_log;
				else
					file_stream=file_pri;

				// Send the samples and check for error
				putbufrv = stream_sample(file_stream);
				
				// Attempt to send and update the statistics in case of success and error
				if(putbufrv)
					stat_mpu_samplesendfailed++;	// There was an error in fputbuf: increment the number of samples failed to send.
				else
					stat_mpu_samplesendok++;		// Increment success counter

			} // End iterating sample buffer
		}
		
		
		

	} // End sample loop
	
	// Stop acquiring data
	stream_stop();	
	
	// Stop the logging, if logging was ongoing
	mode_sample_logend();
	

	stream_motion_status(file_pri,0);
	mpu_printstat(file_pri);

	fprintf(file_pri,"MPU Geometry time: %lu us\n",mpu_compute_geometry_time());

	// Total errors
#warning Use the stream_mpu_status function instead
	unsigned long cnt_sample_errbusy, cnt_sample_errfull,toterr;
	unsigned long stat_totsample = mpu_stat_totframes();
	mpu_getstat(0, 0, 0, &cnt_sample_errbusy, &cnt_sample_errfull);
	toterr = stat_mpu_samplesendfailed+mpu_stat_lostframes();
	fprintf(file_pri,"Total errors: %lu/%lu samples (%lu ppm). Err stream/log: %lu, err MPU busy: %lu, err buffer full: %lu\n",toterr,stat_totsample,toterr*1000000l/stat_totsample,stat_mpu_samplesendfailed,cnt_sample_errbusy,cnt_sample_errfull);
	if(toterr*1000000l/stat_totsample>10)
		fprintf(file_pri,"WARNING: HIGH SAMPLING ERRORS\n");

#ifdef MSM_LOGBAT
	mode_sample_storebatinfo();
#endif

	// Clear LED
	system_led_off(LED_GREEN);

	fprintf(file_pri,"<SMPLMOTION\n");

}








