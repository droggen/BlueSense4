/*
 * stmadc.h
 *
 *  Created on: 12 janv. 2020
 *      Author: droggen
 */

#ifndef MEGALOL_STMADC_H_
#define MEGALOL_STMADC_H_

#define STM_ADC_BUFFER_NUM		128					// Number of entries in the ADC buffers
#define STM_ADC_BUFFER_MASK		(STM_ADC_BUFFER_NUM-1)
#define STM_ADC_MAXCHANNEL		10					// Maximum number of channels in a scan conversion. In BS4: 5 GPIO channels and 3 internal channels -> safety margin
#define STM_ADC_MAXGROUPING		32					// Maximum number of ADC readings grouped together before triggering an interrupt.
//#define STM_ADC_MAXGROUPING		4				// Maximum number of ADC readings grouped together before triggering an interrupt.


extern unsigned short _stm_adc_dmabuf[];

void stm_adc_data_clear(void);

void _stm_adc_acquire_start(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,unsigned prescaler,unsigned reload);
void stm_adc_acquire_start_us(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,unsigned periodus);
void stm_adc_acquire_stop();
unsigned char stm_adc_init(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp);
void stm_adc_deinit();
unsigned char stm_adc_isdata();
unsigned char stm_adc_isfull(void);
unsigned char stm_adc_data_level(void);
unsigned char stm_adc_data_getnext(unsigned short *buffer,unsigned *nc,unsigned long *timems,unsigned long *pktctr);
void _stm_adc_data_storenext(unsigned short *buffer,unsigned long timems);
void stm_adc_clearstat();
unsigned long stm_adc_stat_totframes();
unsigned long stm_adc_stat_lostframes();
unsigned char stm_adc_acquireonce();
unsigned char stm_adc_acquirecontinuously(unsigned grouping);
void stm_adc_init_gpiotoadc(unsigned char channels);
void stm_adc_deinit_gpiotoanalog(unsigned char channels);
void _stm_adc_init_timer(unsigned prescaler,unsigned reload);
void _stm_adc_deinit_timer();
unsigned long stm_adc_perfbench_withreadout(unsigned long mintime);
unsigned _stm_adc_gettimfrq();

#endif /* MEGALOL_STMADC_H_ */
