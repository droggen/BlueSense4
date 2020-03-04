#ifndef __MODE_MOTIONSTREAM_H
#define __MODE_MOTIONSTREAM_H

#include "command.h"
#include "mpu.h"

// MSM_LOGBAT: if defined, logs the battery level in the last log file, if the filesystem is available.

#define _MODE_SAMPLE_MPU_GET_AND_SEND		{	\
												unsigned char l = mpu_data_level();	\
												for(unsigned char i=0;i<l;i++)	\
												{	\
													if(mpu_data_getnext(&mpumotiondata,&mpumotiongeometry))	\
														break;	\
													putbufrv = mode_sample_mpu_stream(file_stream);	\
													if(putbufrv)	\
														stat_mpu_samplesendfailed++;	\
													else	\
														stat_mpu_samplesendok++;	\
												} 	\
											}


// #define MSM_LOGBAT

extern const char help_streamlog[];

unsigned char mode_sample_mpu_stream(FILE *f);

// Structure to hold the volatile parameters of this mode
typedef struct {
	unsigned char mode;
	int logfile;
} MODE_SAMPLE_MOTION_PARAM;

extern MPUMOTIONDATA mpumotiondata;
extern MPUMOTIONGEOMETRY mpumotiongeometry;
extern unsigned long stat_mpu_samplesendfailed;
extern unsigned long stat_mpu_samplesendok;

void stream(void);
void stream_stop(void);

void mode_sample_motion_setparam(unsigned char mode, int logfile, int duration);
unsigned char CommandParserSampleLogMPU(char *buffer,unsigned char size);
unsigned char CommandParserSampleStatus(char *buffer,unsigned char size);
unsigned char CommandParserBatBench(char *buffer,unsigned char size);
void stream_motion_status(FILE *f,unsigned char bin);
unsigned char CommandParserMotion(char *buffer,unsigned char size);
void mode_sample_motion(void);


#endif
