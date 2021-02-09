#ifndef __MODE_SAMPLE_H
#define __MODE_SAMPLE_H

#define MODE_SAMPLE_INFOEVERY_MS	30000


/* Check whether duration of recording overrun to stop streaming/logging
Check if maximum mode time is reached
Relies on:
	mode_sample_param_duration (in seconds)
	stat_timems_start: start time (in milliseconds)
	stat_t_cur: current time (in milliseconds)
*/
#define _MODE_SAMPLE_CHECK_DURATION_BREAK if(mode_sample_param_duration) { if(stat_t_cur-stat_timems_start>=mode_sample_param_duration) break; }

/* Check whether battery is too low to stop streaming/logging
Relies on:
	ltc2942_last_mV: last battery reading in mV
	BATTERY_VERYVERYLOW: thresold below which to stop
*/
#define _MODE_SAMPLE_CHECK_BATTERY_BREAK if(ltc2942_last_mV()<BATTERY_VERYVERYLOW) { fprintf(file_pri,"Low battery, interrupting\n"); break; }

/* Blink at regular intervals
 Relies on:
 	 stat_t_cur: current time (in milliseconds)
 	 time_lastblink: time of last blink (in milliseconds)
*/
#define _MODE_SAMPLE_BLINK if(stat_t_cur-time_lastblink>1000) { system_led_toggle(0b100); time_lastblink=stat_t_cur; }

/*
	Sleep and update wakeup statistics
	Relies on:
		stat_wakeup: must be defined
*/
#define _MODE_SAMPLE_SLEEP		{	\
									system_sleep();	\
									stat_wakeup++;	\
								}

/*
	Used to stream log size by the various streaming functions
*/
#define _MODE_SAMPLE_STREAM_STATUS_LOGSIZE fprintf(f,"; log=%u KB; logmax=%u KB; logfull=%u %%\n",(unsigned)(ufat_log_getsize()>>10),(unsigned)(ufat_log_getmaxsize()>>10),(unsigned)(ufat_log_getsize()/(ufat_log_getmaxsize()/100l)));


extern FILE *mode_sample_file_log;

extern const char help_samplestatus[];
extern const char help_samplelog[];

extern unsigned long stat_timems_start,stat_t_cur,stat_wakeup,stat_time_laststatus;
extern unsigned long int time_lastblink;

extern int mode_sample_param_duration;
extern int mode_sample_param_logfile;

unsigned char CommandParserSampleLog(char *buffer,unsigned char size);
unsigned char mode_sample_startlog(int lognum);
void mode_sample_logend(void);
void mode_sample_clearstat(void);
void stream_load_persistent_frame_settings(void);
void mode_sample_storebatinfo(void);





#endif
