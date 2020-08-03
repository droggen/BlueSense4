/*
 * mode_sample_sound.c
 *
 *  Created on: 8 janv. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include "mode.h"
#include "mode_sample.h"
#include "mode_sample_sound.h"
#include "global.h"
#include "stmdfsdm.h"
#include "ufat.h"
#include "commandset.h"
#include "helper.h"
#include "helper_num.h"
#include "system.h"
#include "mode_global.h"
#include "wait.h"
#include "ltc2942.h"
#include "system-extra.h"
#include "pkt.h"

int mode_sample_sound_param_mode;
int mode_sample_sound_param_framebased;
int mode_sample_sound_param_logfile;
unsigned long stat_snd_samplesendfailed;
unsigned long stat_snd_samplesendok;

#define SAMPLE_SOUND_DEBUG_NUMSTREAM STM_DFSMD_BUFFER_SIZE			// Default
//#define SAMPLE_SOUND_DEBUG_NUMSTREAM 10							// For debugging, instead of sending all samples only the first SAMPLE_SOUND_DEBUG_NUMSTREAM of a frame are sent


//const char help_samplestatus[] = "Battery and logging status";
//const char help_batbench[] = "Battery benchmark";

const COMMANDPARSER CommandParsersSampleSound[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	//{'W', CommandParserSwap,help_w},
	//{'F', CommandParserStreamFormat,help_f},
	//{'L', CommandParserSampleLogMPU,help_samplelog},
	//{'Z',CommandParserSync,help_z},
	//{'i',CommandParserInfo,help_info},
	{'N', CommandParserAnnotation,help_annotation},
	{'Q', CommandParserBatteryInfoLong,help_batterylong},
	{'q', CommandParserBatteryInfo,help_battery},
	{'s', CommandParserSampleSoundStatus,help_samplestatus},
	//{'x', CommandParserBatBench,help_batbench},

};
const unsigned char CommandParsersSampleSoundNum=sizeof(CommandParsersSampleSound)/sizeof(COMMANDPARSER);

/******************************************************************************
	function: mode_motionstream
*******************************************************************************
	Streaming mode loop
******************************************************************************/
// TODO: each time that a logging starts, stops, or change reset all the statistics and clear the buffers, including resetting the time since running
void mode_sample_sound(void)
{
	int putbufrv;
	STM_DFSMD_TYPE audbuf[STM_DFSMD_BUFFER_SIZE];		// Buffer for audio data
	unsigned long audbufms,audbufpkt;				// Audio metadata
	unsigned char audleftright;						// Audio metadata: left or right mic

	fprintf(file_pri,"SMPLSOUND>\n");

	fprintf(file_pri,"mode: %d frame: %d logfile: %d duration: %d\n",mode_sample_sound_param_mode,mode_sample_sound_param_framebased,mode_sample_sound_param_logfile,mode_sample_param_duration);


	mode_sample_file_log=0;										// Initialise log to null
	mode_sample_startlog(mode_sample_sound_param_logfile);		// Initialise log will be initiated if needed here

	// Load
	stream_load_persistent_frame_settings();
	// Set audio mode
	stm_dfsdm_init(mode_sample_sound_param_mode,STM_DFSDM_LEFT);

	// Clear sample statistics
	mode_sample_clearstat();
	stm_dfsdm_data_clear();



	while(1)
	{
		// Process user commands only if we do not run for a specified duration
		if(mode_sample_param_duration==0)
		{
			while(CommandProcess(CommandParsersSampleSound,CommandParsersSampleSoundNum));
			if(CommandShouldQuit())
				break;
		}

		// Send data to primary stream or to log if available
		FILE *file_stream;
		if(mode_sample_file_log)
			file_stream=mode_sample_file_log;
		else
			file_stream=file_pri;

		stat_t_cur=timer_ms_get();		// Current time
		_MODE_SAMPLE_CHECK_DURATION_BREAK;				// Stop after duration, if specified
		_MODE_SAMPLE_CHECK_BATTERY_BREAK;				// Stop if battery too low
		_MODE_SAMPLE_BLINK;								// Blink

		// Display info if enabled
		if(mode_stream_format_enableinfo)
		{
			if(stat_t_cur-stat_time_laststatus>MODE_SAMPLE_INFOEVERY_MS)
			{
				stream_sound_status(file_pri,mode_stream_format_bin);
				stat_time_laststatus+=MODE_SAMPLE_INFOEVERY_MS;
				stat_wakeup=0;
			}
		}

		// Stream existing data
		_MODE_SAMPLE_SOUND_GET_AND_SEND;

		// Sleep
		_MODE_SAMPLE_SLEEP;
	} // End sample loop

	// Stop acquiring data
	stm_dfsdm_init(STM_DFSMD_INIT_OFF,0);

	// Stop the logging, if logging was ongoing
	mode_sample_logend();


	// Print STM statistics
	stm_dfsdm_stat_print();

	stream_sound_status(file_pri,0);

	//if(toterr*1000000l/stat_totsample>10)
		//fprintf(file_pri,"WARNING: HIGH SAMPLING ERRORS\n");

#ifdef MSM_LOGBAT
	mode_sample_storebatinfo();
#endif

	// Clear LED
	system_led_off(LED_GREEN);


	fprintf(file_pri,"<SMPLSOUND\n");

}


/******************************************************************************
	function: CommandParserSampleSound
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
unsigned char CommandParserSampleSound(char *buffer,unsigned char size)
{
	unsigned char rv;
	int mode,framebased,lognum,duration;

	// Parse from the smallest number of arguments to the largest
	rv = ParseCommaGetInt((char*)buffer,1,&mode);
	if(rv)
	{
		// No argument - display available modes and returns successfully
		//mpu_printmotionmode(file_pri);
		stm_dfsdm_printmodes(file_pri);
		return 0;
	}

	// One argument - check validity
	if(mode<0 || mode>STM_DFSMD_INIT_MAXMODES)
		return 2;	// Invalid


	// Check if two arguments - sufficient for motion sampling mode
	rv = ParseCommaGetInt((char*)buffer,2,&mode,&framebased);
	if(rv==0)
	{
		// Success
		if(framebased!=0 && framebased!=1)
			return 2;

		// Check if 3 arguments
		rv = ParseCommaGetInt((char*)buffer,3,&mode,&framebased,&lognum);
		if(rv==0)
		{
			//fprintf(file_pri,"3 arg - lognum: %d\n",lognum);
			/*
			// Don't test if log is valid - to be consistent with motion mode
			int availlog = ufat_log_getnumlogs();
			// Success
			if(lognum<0 || lognum>availlog)
			{
				fprintf(file_pri,"Invalid log file number. Number of log files: %d\n",availlog);
				return 2;
			}*/

			// Check if 4 arguments
			rv = ParseCommaGetInt((char*)buffer,4,&mode,&framebased,&lognum,&duration);
			if(rv==0)
			{
				//fprintf(file_pri,"4 arg\n");
				// Success
				if(duration<0)
					return 2;
				// Three arguments were passed
				// Start with framebased, log file, duration
				mode_sample_sound_setparam(mode,framebased,lognum,duration);
			}
			else
			{
				//fprintf(file_pri,"really 3 arg\n");
				// Three arguments were passed
				// Start with framebased, log file, no duration
				mode_sample_sound_setparam(mode,framebased,lognum,0);
			}

		}
		else
		{
			// Two arguments were passed
			// Start with framebased, no log file, no duration
			mode_sample_sound_setparam(mode,framebased,-1,0);

		}
	}
	else
	{
		// Start with sample based, no log file, no duration
		mode_sample_sound_setparam(mode,0,-1,0);

	}

	CommandChangeMode(APP_MODE_SAMPLESOUND);

	return 0;
}

void mode_sample_sound_setparam(unsigned char mode,unsigned char framebased, int logfile, int duration)
{
	mode_sample_sound_param_mode=mode;
	mode_sample_sound_param_framebased=framebased;
	mode_sample_sound_param_logfile=logfile;
	mode_sample_param_duration=duration;		// Store the duration in seconds

	//printf("duration: %lu\n",mode_sample_motion_param.duration);
	//printf("mode_sample_motion_setparam: %d %d %d\n",mode_sample_motion_param.mode,mode_sample_motion_param.logfile,mode_sample_motion_param.duration);
}
unsigned char CommandParserSampleSoundStatus(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	stream_sound_status(file_pri,mode_stream_format_bin);
	return 0;
}


int audio_stream_sample(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f)
{
	if(mode_stream_format_bin==0)
	{
		if(mode_sample_sound_param_framebased)
			return audio_stream_sample_text_frame(audbuf,audbufms,pkt,f);
		return audio_stream_sample_text_sample(audbuf,audbufms,pkt,f);
	}
	else
	{
		if(mode_sample_sound_param_framebased)
			return audio_stream_sample_bin_frame(audbuf,audbufms,pkt,f);
		return audio_stream_sample_bin_sample(audbuf,audbufms,pkt,f);
	}
	return 0;
}

char *audio_stream_format_metadata_text(char *strptr,unsigned long time,unsigned long pkt)
{
	// Format packet counter
	if(mode_stream_format_pktctr)
	{
		strptr=format1u32(strptr,pkt);
	}
	// Format timestamp
	if(mode_stream_format_ts)
	{
		strptr = format1u32(strptr,time);
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
	return strptr;
}
void audio_stream_format_metadata_bin(PACKET *p,unsigned long time,unsigned long pkt)
{
	// Format packet counter
	if(mode_stream_format_pktctr)
		packet_add32_little(p,pkt);
	// Format timestamp
	if(mode_stream_format_ts)
		packet_add32_little(p,time);
	// Format battery
	if(mode_stream_format_bat)
		packet_add16_little(p,system_getbattery());
	// Format annotation
	if(mode_stream_format_label)
		packet_add16_little(p,CurrentAnnotation);

}

int audio_stream_sample_text_frame(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f)
{
	// Frame version
	// Assume STM_DFSMD_BUFFER_SIZE is short
	// Sample bytes: STM_DFSMD_BUFFER_SIZE * 6 bytes
	char buf[STM_DFSMD_BUFFER_SIZE*6 + 500];

	char *buf2=buf;

	// Format metadata
	buf2 = audio_stream_format_metadata_text(buf2,audbufms,pkt);
	// Format samples
	for(unsigned int i=0;i<SAMPLE_SOUND_DEBUG_NUMSTREAM;i++)
	{
		s16toa(audbuf[i],buf2);
		buf2+=6;
		*buf2=' ';
		buf2++;
	}
	*buf2='\n';
	buf2++;
	*buf2=0;

	return fputbuf(f,buf,buf2-buf);

}
int audio_stream_sample_text_sample(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f)
{
	// Sample version
	// Assume STM_DFSMD_BUFFER_SIZE is short
	// Sample bytes: STM_DFSMD_BUFFER_SIZE * 6 bytes
	char buf[500];
	int putbuferr=0;

	// Format samples
	for(unsigned int i=0;i<SAMPLE_SOUND_DEBUG_NUMSTREAM;i++)
	{
		char *buf2=buf;

		// Format metadata
		buf2 = audio_stream_format_metadata_text(buf2,audbufms,pkt);

		s16toa(audbuf[i],buf2);
		buf2+=6;
		*buf2='\n';
		buf2++;
		putbuferr+=fputbuf(f,buf,buf2-buf);
	}

	return putbuferr;
}
int audio_stream_sample_bin_frame(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f)
{
	PACKET p;
	packet_init(&p,"DSN",3);

	// Metadata
	audio_stream_format_metadata_bin(&p,audbufms,pkt);
	// Audio data
	for(unsigned int i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
	{
		packet_add16_little(&p,audbuf[i]);
	}

	//fprintf(file_pri,"Size before fletcher: %d\n",packet_size(&p));

	packet_addchecksum_fletcher16_little(&p);
	int s = packet_size(&p);
	//fprintf(file_pri,"Size after fletcher: %d\n",s);

	if(fputbuf(f,(char*)p.data,s))
		return 1;

	return 0;
}
int audio_stream_sample_bin_sample(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f)
{
	PACKET p;
	int putbuferr=0;
	for(unsigned int i=0;i<SAMPLE_SOUND_DEBUG_NUMSTREAM;i++)
	{
		packet_init(&p,"DSN",3);

		// Metadata
		audio_stream_format_metadata_bin(&p,audbufms,pkt);
		// Audio data

		packet_add16_little(&p,audbuf[i]);

		//fprintf(file_pri,"Size before fletcher: %d\n",packet_size(&p));

		packet_addchecksum_fletcher16_little(&p);
		int s = packet_size(&p);
		//fprintf(file_pri,"Size after fletcher: %d\n",s);

		putbuferr+=fputbuf(f,(char*)p.data,s);


	}
	return putbuferr;
}




/******************************************************************************
	function: stream_sound_status
*******************************************************************************
	Sends to a specified file information about the current streaming/logging state,
	including battery information and log status.

	In binary streaming mode an information packet with header DII is created.
	The packet definition string is: DII;is-s-siiiiic;f

	In text streaming mode an easy to parse string is sent prefixed by '#'.



	Paramters:
		f		-	File to which to send the status
		bin		-	Indicate status in binary or text

	Returns:
		Nothing
*******************************************************************************/
void stream_sound_status(FILE *f,unsigned char bin)
{
	unsigned long wps = stat_wakeup*1000l/(stat_t_cur-stat_time_laststatus);

	// Statistics in frames or samples
	// Total error is lost frame during acquisition and samples/frames not sent.
	unsigned long toterr,totsample,overrun,sendok,sr;
	if(mode_sample_sound_param_framebased)
	{
		toterr = stm_dfsdm_stat_lostframes() + stat_snd_samplesendfailed;
		totsample = stm_dfsdm_stat_totframes();
		overrun = stm_dfsdm_stat_lostframes();
		sendok = stat_snd_samplesendok;
		sr =  stat_snd_samplesendok*SAMPLE_SOUND_DEBUG_NUMSTREAM*1000/(stat_t_cur-stat_timems_start);
	}
	else
	{
		toterr = stm_dfsdm_stat_lostframes()*STM_DFSMD_BUFFER_SIZE + stat_snd_samplesendfailed;
		totsample = stm_dfsdm_stat_totframes()*STM_DFSMD_BUFFER_SIZE;
		overrun = stm_dfsdm_stat_lostframes()*STM_DFSMD_BUFFER_SIZE;
		sendok = stat_snd_samplesendok*SAMPLE_SOUND_DEBUG_NUMSTREAM;
		sr =  sendok*1000/(stat_t_cur-stat_timems_start);
	}

	unsigned long errppm = (unsigned long long)toterr*1000000/totsample;			// Computation on 64-bit needed


	//fprintf(file_pri,"totframes: %lu lostframes: %lu sendfailed: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes(),stat_samplesendfailed);
	//fprintf(file_pri,"errppm: %lu\n",errppm);

	//if(bin==0)
	{
		fprintf(f,"# SND t=%05lu ms; %s",stat_t_cur-stat_timems_start,ltc2942_last_strstatus());
		fprintf(f,"; wps=%lu",wps);
		fprintf(f,"; sampletot=%09lu; sampleok=%09lu; sampleerr=%09lu (overrun=%09lu; errsend=%09lu); errppm=%lu; Numbers are %s; sr=%05lu",
				totsample,sendok,
				toterr,
				overrun,stat_snd_samplesendfailed,
				errppm,
				mode_sample_sound_param_framebased?"frames":"samples",
				sr);
		_MODE_SAMPLE_STREAM_STATUS_LOGSIZE;
	}
	// Binary mode not implemented
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
		packet_add32_little(&p,toterr);
		packet_add32_little(&p,totsample);
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
