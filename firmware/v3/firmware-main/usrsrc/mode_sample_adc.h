#ifndef __MODE_ADC_H
#define __MODE_ADC_H

#include "command.h"
#include "pkt.h"

/*
 *
*/
#define _MODE_SAMPLE_ADC_GET_AND_SEND		{	\
												unsigned numchannels;	\
												unsigned long pktsample,timesample;	\
												if(!stm_adc_data_getnext(data,&numchannels,&timesample,&pktsample))	\
												{	\
													if(mode_adc_fastbin)	\
													{	\
														putbufrv = mode_sample_adc_streamfastbin(file_stream,numchannels,data);	\
													}	\
													else	\
													{	\
														putbufrv = mode_sample_adc_stream(file_stream,pktsample,timesample,numchannels,data);	\
													}	\
													if(putbufrv)	\
														stat_adc_samplesendfailed++;	\
													else	\
														stat_adc_samplesendok++;	\
												} 	\
											}

extern unsigned mode_adc_period;
extern unsigned mode_adc_mask,mode_adc_fastbin;
extern unsigned long stat_adc_samplesendfailed;
extern unsigned long stat_adc_samplesendok;

extern PACKET adcpacket;

unsigned char mode_sample_adc_checkmask();
unsigned char mode_sample_adc_period();


void mode_sample_adc(void);

unsigned char CommandParserADCPullup(char *buffer,unsigned char size);
unsigned char CommandParserADC(char *buffer,unsigned char size);
unsigned char mode_sample_adc_stream(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data);
unsigned char mode_sample_adc_streamtext(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data);
unsigned char mode_sample_adc_streambin(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data);
unsigned char mode_sample_adc_streamfastbin(FILE *file_stream,unsigned numchannels,unsigned short *data);
unsigned char mode_sample_adc_streamfastbin16(FILE *file_stream,unsigned numchannels,unsigned short *data);
void stream_adc_status(FILE *f,unsigned char bin);

#endif
