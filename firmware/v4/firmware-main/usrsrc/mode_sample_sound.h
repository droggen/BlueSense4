/*
 * mode_sample_sound.h
 *
 *  Created on: 8 janv. 2020
 *      Author: droggen
 */

#ifndef MODE_SAMPLE_SOUND_H_
#define MODE_SAMPLE_SOUND_H_

#include <stdio.h>
#include "stmdfsdm.h"
#include "pkt.h"

/* Stream existing data - up to a maximum of N=maxctr samples/frames
	maxctr provides an exit to process other things (e.g. user input) if the sample rate uses all CPU resources

Relies on:
	putbufrv: must be of type int, as sound streaming can occur by sample or frames; and putbufrv is normalised to return the number of successful samples
*/
#define _MODE_SAMPLE_SOUND_GET_AND_SEND		{	\
												unsigned maxctr=0;	\
												while(!stm_dfsdm_data_getnext(audbuf,&audbufms,&audbufpkt,&audleftright) && (maxctr++<100))	\
												{	\
													putbufrv = audio_stream_sample(audbuf,audbufms,audbufpkt,file_stream);	\
													if(putbufrv)	\
														stat_snd_samplesendfailed+=putbufrv;			\
													else	\
														stat_snd_samplesendok++;	\
												}	\
											}


extern int mode_sample_sound_param_framebased;
extern unsigned long stat_snd_samplesendfailed;
extern unsigned long stat_snd_samplesendok;

unsigned char CommandParserSampleSound(char *buffer,unsigned char size);
unsigned char CommandParserSampleSoundStatus(char *buffer,unsigned char size);

void mode_sample_sound(void);

void mode_sample_sound_setparam(unsigned char mode,unsigned char framebased, int logfile, int duration);
char *audio_stream_format_metadata_text(char *strptr,unsigned long time,unsigned long pkt);
void audio_stream_format_metadata_bin(PACKET *p,unsigned long time,unsigned long pkt);
int audio_stream_sample(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f);
int audio_stream_sample_text_frame(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f);
int audio_stream_sample_text_sample(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f);
int audio_stream_sample_bin_frame(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f);
int audio_stream_sample_bin_sample(STM_DFSMD_TYPE *audbuf,unsigned long audbufms,unsigned long pkt,FILE *f);
void stream_sound_status(FILE *f,unsigned char bin);





#endif /* MODE_SAMPLE_SOUND_H_ */
