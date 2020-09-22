/*
	file: mode_adc
	
	ADC sampling and streaming/logging mode.
	
	This function contains the 'A' mode of the sensor which allows to acquire multiple ADC channels and stream or log them. 
	
	*TODO*
	
	* Statistics when logging could display log-only information (samples acquired, samples lost, samples per second)
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
#include "mode.h"
#include "mode_global.h"
#include "mode_sample.h"
#include "mode_sample_adc.h"
#include "commandset.h"
#include "ltc2942.h"

/*
	File: mode_sample_adc

	Sample/log ADC.

	Status: complete.

	Notes: the sample time is common for the entire DMA acquired framed, but the sample rate is highly constant

	TODO: none

*/

unsigned mode_adc_period;
unsigned mode_adc_mask,mode_adc_fastbin;
unsigned long stat_adc_samplesendfailed;
unsigned long stat_adc_samplesendok;
PACKET adcpacket;

//const char help_adcpullup[]="P,<on>: if on=1, activates pullups on user ADC inputs (channels 0-3), otherwise deactivate. In principle pullups should be deactivated.";

const COMMANDPARSER CommandParsersADC[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'N', CommandParserAnnotation,help_annotation},
	//{'F', CommandParserStreamFormat,help_f},
//	{'W', CommandParserSwap,help_w},
	{'i',CommandParserInfo,help_info},
	{'!', CommandParserQuit,help_quit}
};
const unsigned char CommandParsersADCNum=sizeof(CommandParsersADC)/sizeof(COMMANDPARSER); 



/*
	Parsers must return:
		0:	Message execution ok (message valid)
		1:	Message execution error (message valid)
		2:	Message invalid 		
*/

/******************************************************************************
	function: CommandParserADCPullup
*******************************************************************************	
	Parses a user command to enable/disable the pull-ups on the ADC inputs.

	Command format: P,<on>
	
	Activates or desactivates the pull-up.
	
	Parameters:
		buffer			-			Buffer containing the command
		size			-			Length of the buffer containing the command
			
******************************************************************************/
#if 0
unsigned char CommandParserADCPullup(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned on;
	rv = ParseCommaGetInt((char*)buffer,1,&on);
	if(rv)
		return 2;
	if(!(on==0 || on==1))
		return 2;
#warning fix
	/*if(on==0)
		system_adcpu_off();
	else
		system_adcpu_on();*/
	
	return 0;
}
#endif

/******************************************************************************
	function: mode_sample_adc_checkmask
*******************************************************************************
	Check if mask is valid

	Parameters:
		-

	Returns:
		0				-			Ok
		1				-			Error

******************************************************************************/
unsigned char mode_sample_adc_checkmask()
{
	if(mode_adc_mask>0b11111111 || mode_adc_mask==0)
	{
		fprintf(file_pri,"Invalid mask\n");
		return 1;
	}
	return 0;
}
/******************************************************************************
	function: mode_sample_adc_period
*******************************************************************************
	Check if mask is valid

	Parameters:
		-

	Returns:
		0				-			Ok
		1				-			Error

******************************************************************************/
unsigned char mode_sample_adc_period()
{
	if(mode_adc_period<50 || mode_adc_period>500000)
	{
		fprintf(file_pri,"Invalid period: should be in range [50uS;0.5s]\n");
		return 2;
	}
	// Round period to multiple of 10uS
	if(mode_adc_period%10)
	{
		mode_adc_period=(mode_adc_period/10)*10;
		fprintf(file_pri,"Period rounded to %u\n",mode_adc_period);
	}
	return 0;
}

/******************************************************************************
	function: CommandParserADC
*******************************************************************************	
	Parses a user command to enter the ADC mode.
	
	Command format: A,<mask>,<period>,[fastbin,[<logfile>[,<duration>]]]
	
	Stores the mask and period in eeprom.
	
	Parameters:
		buffer			-			Buffer containing the command
		size			-			Length of the buffer containing the command
			
******************************************************************************/
unsigned char CommandParserADC(char *buffer,unsigned char size)
{
	mode_adc_fastbin=0;
	mode_sample_param_duration = 0;
	mode_sample_param_logfile = -1;

	int np = ParseCommaGetNumParam(buffer);

	if(np<2 || np>5)
		return 2;
	switch(np)
	{
		case 2:
			if(ParseCommaGetInt(buffer,2,&mode_adc_mask,&mode_adc_period))
				return 2;
			break;
		case 3:
			if(ParseCommaGetInt(buffer,3,&mode_adc_mask,&mode_adc_period,&mode_adc_fastbin))
				return 2;
			break;
		case 4:
			if(ParseCommaGetInt(buffer,4,&mode_adc_mask,&mode_adc_period,&mode_adc_fastbin,&mode_sample_param_logfile))
				return 2;
			break;
		case 5:
		default:
			if(ParseCommaGetInt(buffer,5,&mode_adc_mask,&mode_adc_period,&mode_adc_fastbin,&mode_sample_param_logfile,&mode_sample_param_duration))
				return 2;
			break;
	}

	if(mode_sample_adc_checkmask())
		return 2;

	if(mode_sample_adc_period())
		return 2;

	if(mode_adc_fastbin>2) mode_adc_fastbin=2;		// fastbin is 0, 1 or 2.
	// Round period to multiple of 10uS
	if(mode_adc_period%10)
	{
		mode_adc_period=(mode_adc_period/10)*10;
		fprintf(file_pri,"Period rounded to %u\n",mode_adc_period);
	}
	
	fprintf(file_pri,"Sampling ADC: channel mask: %02X; period: %u; fastbin: %u; logfile: %d; duration: %d\n",mode_adc_mask,mode_adc_period,mode_adc_fastbin,mode_sample_param_logfile,mode_sample_param_duration);
		
	
	CommandChangeMode(APP_MODE_ADC);

		
	return 0;
}




/******************************************************************************
	function: mode_adc
*******************************************************************************	
	ADC mode loop.
	
	This function initialises the board for ADC acquisition and enters a continuous 
	sample/stream loop.
	
******************************************************************************/
void mode_sample_adc(void)
{
	unsigned short data[STM_ADC_MAXCHANNEL];		// Buffer to hold a copy of ADC data

	unsigned long stat_timems_end=0;
	int putbufrv;
	
	
	fprintf(file_pri,"SMPLADC>\n");

	// Packet init
	packet_init(&adcpacket,"DAA",3);
	// Init log file
	mode_sample_file_log=0;										// Initialise log to null
	if(mode_sample_startlog(mode_sample_param_logfile))			// Initialise log will be initiated if needed here
		goto mode_sample_adc_end;



	// Load mode configuration
	stream_load_persistent_frame_settings();
	
	// Some info
	fprintf(file_pri,"ADC mode: period: %u mask: %02X binary: %d timestamp: %d battery: %d label: %d\n",mode_adc_period,mode_adc_mask,mode_stream_format_bin,mode_stream_format_ts,mode_stream_format_bat,mode_stream_format_label);
	
	// Update the current annotation
	CurrentAnnotation=0;
	
	// Initialise ADC with channels and time; round time to multiple of 10uS
	stm_adc_acquire_start_us(mode_adc_mask&0b11111,(mode_adc_mask>>5)&1,(mode_adc_mask>>6)&1,(mode_adc_mask>>7)&1,mode_adc_period);


	// Clear statistics and initialise timers.
	mode_sample_clearstat();

	while(!CommandShouldQuit())
	{
		// Depending on the "fastbin" mode either go through the fast "keypress" exit, or the slower command parser.
		if(mode_adc_fastbin)
		//if(1)
		{
			// Keypress to quit - faster than parsing user commands.
			if(fischar(file_pri))		// Faster than fgetc
				break;
		}
		else
		{
			// Process all user commands
			while(CommandProcess(CommandParsersADC,CommandParsersADCNum));
		}

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
				stream_adc_status(file_pri,mode_stream_format_bin);
				stat_time_laststatus+=MODE_SAMPLE_INFOEVERY_MS;
				stat_wakeup=0;
			}
		}
		
		// Acquire and send data if available
		_MODE_SAMPLE_ADC_GET_AND_SEND;
		/*{
			unsigned level = stm_adc_data_level();
			unsigned numchannels;
			unsigned long pktsample,timesample;
			for(unsigned li=0;li<level;li++)
			{
				if(!stm_adc_data_getnext(data,&numchannels,&timesample,&pktsample))
				{
					switch(mode_adc_fastbin)
					{
						case 1:
							putbufrv = mode_sample_adc_streamfastbin(file_stream,numchannels,data);
							break;
						case 2:
							putbufrv = mode_sample_adc_streamfastbin16(file_stream,numchannels,data);
							break;
						case 0:
						default:
							putbufrv = mode_sample_adc_stream(file_stream,pktsample,timesample,numchannels,data);
					}
					if(putbufrv)
						stat_adc_samplesendfailed++;
					else
						stat_adc_samplesendok++;
				}
				else
					break;
			}
		}*/


		// Sleep
		_MODE_SAMPLE_SLEEP;
	} // End sample loop
	stat_timems_end = timer_ms_get();
	
	// Stop sampling
	stm_adc_acquire_stop();

	// Break to let terminal recover
	HAL_Delay(100);

	// End the logging, if logging was ongoing
	mode_sample_logend();
	
	// Clear LED
	system_led_off(LED_GREEN);

	// Print statistics
	stream_adc_status(file_pri,0);

	unsigned long sps = (stat_adc_samplesendok+stat_adc_samplesendfailed)*1000/(stat_timems_end-stat_timems_start);
	fprintf(file_pri,"%lu samples/sec\n",sps);


	mode_sample_adc_end:
	fprintf(file_pri,"<SMPLADC\n");
}



unsigned char mode_sample_adc_streamtext(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data)
{
	char buffer[128];			// Should be at least 128
	// Plain text encoding
	char *bufferptr=buffer;
	if(mode_stream_format_pktctr)
	{
		bufferptr=format1u32(bufferptr,pktsample);
	}
	// Format us timestamp
	if(mode_stream_format_ts)
	{
		bufferptr = format1u32(bufferptr,timesample);
	}
	// Format ms timestamp
	/*if(mode_stream_format_ts)
	{
		bufferptr = format1u32(bufferptr,timer_ms_get());
	}*/
	if(mode_stream_format_bat)
	{
		bufferptr=format1u16(bufferptr,system_getbattery());
	}
	if(mode_stream_format_label)
	{
		bufferptr=format1u16(bufferptr,CurrentAnnotation);
	}
	for(unsigned i=0;i<numchannels;i++)
	{
		bufferptr=format1u16(bufferptr,data[i]);
	}
	/*bufferptr[0]='A'+(stat_totsample%26);
	bufferptr++;*/
	*bufferptr='\n';
	bufferptr++;
	//*bufferptr=0;
	//bufferptr++;
	//fputs(buffer,file_stream);
	unsigned char putbufrv = fputbuf(file_stream,buffer,bufferptr-buffer);
	//putbufrv = fputbuf(file_stream,buffer,1);
	//bufferptr[0]='.';
	//putbufrv = fputbuf(file_stream,buffer,1);
	//fputc('.',file_stream);
	//putbufrv = 0;
	return putbufrv;
}

unsigned char mode_sample_adc_streambin(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data)
{
	// Packet encoding
	packet_reset(&adcpacket);
	if(mode_stream_format_pktctr)
	{
		packet_add32_little(&adcpacket,pktsample);
	}
	if(mode_stream_format_ts)
	{
		packet_add32_little(&adcpacket,timesample);
	}
	if(mode_stream_format_bat)
	{
		packet_add16_little(&adcpacket,system_getbattery());
	}
	if(mode_stream_format_label)
	{
		packet_add16_little(&adcpacket,CurrentAnnotation);
	}
	for(unsigned i=0;i<numchannels;i++)
	{
		packet_add16_little(&adcpacket,data[i]);
	}
	packet_end(&adcpacket);
	packet_addchecksum_fletcher16_little(&adcpacket);
	int s = packet_size(&adcpacket);
	unsigned char putbufrv = fputbuf(file_stream,(char*)adcpacket.data,s);
	return putbufrv;
}
unsigned char mode_sample_adc_streamfastbin(FILE *file_stream,unsigned numchannels,unsigned short *data)
{
	// Write data in 8-bit
	unsigned char buf[16];
	for(int i=0;i<numchannels;i++)
	{
		buf[i] = (unsigned char)(data[i]>>4);
	}
	unsigned char putbufrv = fputbuf(file_stream,(char*)buf,numchannels);
	return putbufrv;
}
unsigned char mode_sample_adc_streamfastbin16(FILE *file_stream,unsigned numchannels,unsigned short *data)
{
	// Write data in 16-bit with a single D header ("D;s..." format string), no checksum
	// ensuring no "D" arises in the data.
	unsigned char buf[32];
	unsigned char header='D';
	unsigned o=0;
	buf[o++]=header;
	for(int i=0;i<numchannels;i++)
	{
		unsigned short d = data[i];
		unsigned short dh = d>>8;
		unsigned short dl = d&0xff;
		// Check for presence of 'D'"
		if(dh==header)	// Normally dh cannot be equal to header as only lower 4 bits can be set, i.e. maximum value is 0x0F while 'D' is 0x44.
			dh--;
		if(dl==header)
			dl--;

		// Send in little endian
		buf[o++] = (unsigned char)dl;
		buf[o++] = (unsigned char)dh;
	}
	unsigned char putbufrv = fputbuf(file_stream,(char*)buf,numchannels*2+1);
	return putbufrv;
}
/******************************************************************************
	function: stream_adc_status
*******************************************************************************
	Sends to a specified file information about the current streaming/logging state,
	including battery informationa and log status.

	Information to send:
	- Tot sample sent; Tot error (Acquire Error; Send Error); Log file size



	In binary streaming mode an information packet with header DII is created.
	The packet definition string is: DII;is-s-siiiiic;f

	In text streaming mode an easy to parse string is sent prefixed by '#'.



	Paramters:
		f		-	File to which to send the status
		bin		-	Indicate status in binary or text

	Returns:
		Nothing
*******************************************************************************/
void stream_adc_status(FILE *f,unsigned char bin)
{
	unsigned long wps = stat_wakeup*1000l/(stat_t_cur-stat_time_laststatus);
	//unsigned long cnt_int, cnt_sample_tot, cnt_sample_succcess, cnt_sample_errbusy, cnt_sample_errfull;

	unsigned long totframes = stm_adc_stat_totframes();
	unsigned long lostframes = stm_adc_stat_lostframes();
	unsigned long sr = stat_adc_samplesendok*1000/(stat_t_cur-stat_timems_start);

	//if(bin==0)
	{
		// Information text
		fprintf(f,"# ADC t=%05lu ms; %s",stat_t_cur-stat_timems_start,ltc2942_last_strstatus());
		fprintf(f,"; wps=%lu",wps);
		fprintf(f,"; sampletot=%09lu; sampleok=%09lu; samplerr=%09lu (overrun=%09lu; errsend=%09lu); sr=%04lu",totframes,stat_adc_samplesendok,stat_adc_samplesendfailed+lostframes,lostframes,stat_adc_samplesendfailed,sr);
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


unsigned char mode_sample_adc_stream(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data)
{
	if(!mode_stream_format_bin)
	{
		// Plain text encoding
		return mode_sample_adc_streamtext(file_stream,pktsample,timesample,numchannels,data);
	}

	return mode_sample_adc_streambin(file_stream,pktsample,timesample,numchannels,data);
}


