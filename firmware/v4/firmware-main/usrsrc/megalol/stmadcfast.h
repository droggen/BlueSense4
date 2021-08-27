/*
 * stmadc.h
 *
 *  Created on: 12 janv. 2020
 *      Author: droggen
 */

#ifndef MEGALOL_STMADCFAST_H_
#define MEGALOL_STMADCFAST_H_


//#define STMADCFAST_KEEPOLDEST	0								// Whether the buffering behaviour is to keep old frames if the buffer is full (buffered frames can be old depending on readout); or evict the oldest ones (buffered frames are most recent)

//#define ADCFAST_BUFFERSIZE 32									// Size of frames. Frames contain interleaved data. Frames define DMA buffer size and acquisition buffers size.
#define ADCFAST_BUFFERSIZE 1024									// Size of frames. Frames contain interleaved data. Frames define DMA buffer size and acquisition buffers size.
#define STM_ADCFAST_BUFFER_NUM 16								// Number of frame buffers.
#define STM_ADCFAST_BUFFER_MASK		(STM_ADCFAST_BUFFER_NUM-1)	// Helper for wraparounds.

extern unsigned short _stm_adcfast_dmabuf[];

void stm_adcfast_acquire_start(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,unsigned prescaler,unsigned reload);
void stm_adcfast_acquire_stop();
unsigned char stm_adcfast_init(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,int *numchannel);
unsigned char stm_adcfast_acquirecontinuously(unsigned effframesize);

void stm_adcfast_data_clear(void);
unsigned stm_adcfast_samplerate_to_divider(unsigned samplerate,unsigned prescaler);

void stm_adcfast_clearstat();
unsigned long stm_adcfast_stat_totframes();
unsigned long stm_adcfast_stat_lostframes();
unsigned char stm_adcfast_isdata();
unsigned char stm_adcfast_isfull(void);
unsigned short stm_adcfast_data_level(void);


unsigned char stm_adcfast_data_getnext(unsigned short * restrict buffer,unsigned *effsize,unsigned long *timems,unsigned long *pktctr);
unsigned char stm_adcfast_data_getnext_anyway(unsigned short * restrict buffer,unsigned *effsize,unsigned long *timems,unsigned long *pktctr);
void _stm_adcfast_data_storenext(unsigned short * restrict buffer,unsigned long timems);
//unsigned char stm_adcfast_data_getnext(unsigned short *buffer,unsigned *nc,unsigned long *timems,unsigned long *pktctr);

void _stm_adcfast_callback_conv_cplt(ADC_HandleTypeDef *hadc);
void _stm_adcfast_callback_conv_half_cplt(ADC_HandleTypeDef *hadc);

unsigned long stm_adcfast_perfbench_withreadout(unsigned long mintime);


#endif

