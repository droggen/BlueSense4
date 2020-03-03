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
