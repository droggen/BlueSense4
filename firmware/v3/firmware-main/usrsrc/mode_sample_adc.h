#ifndef __MODE_ADC_H
#define __MODE_ADC_H

#include "command.h"
#include "pkt.h"

extern unsigned mode_adc_period;
extern unsigned mode_adc_mask;
extern unsigned long stat_adc_samplesendfailed;
extern unsigned long stat_adc_samplesendok;

extern PACKET adcpacket;

unsigned char mode_sample_adc_checkmask();
unsigned char mode_sample_adc_period();


void mode_sample_adc(void);

unsigned char CommandParserADCPullup(char *buffer,unsigned char size);
unsigned char CommandParserADC(char *buffer,unsigned char size);
unsigned char mode_sample_adc_streamtext(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data);
unsigned char mode_sample_adc_streambin(FILE *file_stream,unsigned long pktsample,unsigned long timesample,unsigned numchannels,unsigned short *data);
unsigned char mode_sample_adc_streamfastbin(FILE *file_stream,unsigned numchannels,unsigned short *data);
void stream_adc_status(FILE *f,unsigned char bin);

#endif
