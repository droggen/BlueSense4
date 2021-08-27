#ifndef __MEGALOL_DSP_H
#define __MEGALOL_DSP_H

//#include "stmadcfast.h"

//#define CICBUFSIZ 12
#define CICBUFSIZ 1024







void hardcic_test();

void dsp_crossproduct(int *outi,int *outq,int *cp,int inlen);
void dsp_crossproduct_s(short *outi,short *outq,int *cp,int inlen);
//void dsp_crossproduct_stream(int *outi,int *outq,int *cp,int inlen);
void dsp_crossproduct_stream(int * restrict outi,int * restrict outq,int * restrict cp,int inlen);
void dsp_crossproduct_stream_sr(int * restrict outi,int * restrict outq,int * restrict cp,int inlen,unsigned sr);
void dsp_crossproduct_stream_dsp_iii_sr(int * restrict outi,int * restrict outq,int * restrict cp,int inlen,int sr);
void dsp_crossproduct_stream_dsp(short * restrict outi,short * restrict outq,int * restrict cp,int inlen);

void dsp_crossproduct_stream_h16(int * restrict outi,int * restrict outq,int * restrict cp,int inlen);
void dsp_crossproduct_stream_h16_2(int * restrict outi,int * restrict outq,int * restrict cp,int inlen);
void dsp_crossproduct_stream_h16_simd(int * restrict outi,int * restrict outq,int * restrict cp,int inlen);

void dsp_crossproductsign_stream_in16(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr);
void dsp_crossproductsign_stream_in16_simd(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr);
void dsp_crossproductsign_stream_in16_simd_2(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr);
void dsp_crossproductsign_stream_in16_simd_3(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr);

void dsp_test_real_to_iq_nofilt(short *data,short *dati,short *datq);

signed int dsp_mean(signed short* restrict data,unsigned len);
signed int dsp_mean_simd_13s_16s(signed short* restrict data,unsigned len);
signed int dsp_mean_simd_12u_16u(signed short* restrict data,unsigned len);

signed int dsp_std(signed short* restrict data,unsigned len,int mean);
signed int dsp_min(signed short* restrict data,unsigned len);
signed int dsp_max(signed short* restrict data,unsigned len);

void dsp_crossproductsign_stream_sr(int * restrict outi,int * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr);

uint32_t simd_s16_to_s16s16(signed short s);




void dsp_addcoswave(signed short *target,int len,int sr,int f,float amp,float phase);


signed int dsp_meansub_inplace(signed short* restrict data);
void dsp_meansub_simd_inplace(signed short* restrict data);


#endif
