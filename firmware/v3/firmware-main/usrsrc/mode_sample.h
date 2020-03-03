#ifndef __MODE_SAMPLE_H
#define __MODE_SAMPLE_H

#define MODE_SAMPLE_INFOEVERY_MS	2000


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
