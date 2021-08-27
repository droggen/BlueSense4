#ifndef __MEGALOL_DSP_R2IQ_H
#define __MEGALOL_DSP_R2IQ_H


void dsp_real_to_iq_4x_cic_init_cpu(unsigned downsampling,unsigned order);
void dsp_real_to_iq_4x_cic_init_dma(unsigned downsampling,unsigned order);


void dsp_real_to_iq_4x_cic_init(unsigned downsample,unsigned order);
unsigned dsp_real_to_iq_4x_cic_process_cpu(short * restrict adc,int * restrict outi,int *restrict outq,int inlen,short subtract);
unsigned dsp_real_to_iq_4x_cic_process_cpu_msb(short * restrict adc,short * restrict outi,short *restrict outq,int inlen,short subtract);
unsigned dsp_real_to_iq_4x_cic_process_dma(short * restrict adc,int *restrict outi,int *restrict outq,int inlen,short subtract);
unsigned dsp_real_to_iq_4x_split(short * restrict adc,short *restrict outi,short *restrict outq,int inlen,short subtract);

void dsp_real_to_iq_4x_cic();

void dsp_r2iq_test_cpu_msb(short *adc,int inlen,short *outi, short *outq,unsigned order);
void dsp_r2iq4x_init_dma(unsigned downsampling,unsigned order);
void dsp_r2iq_test();

#endif
