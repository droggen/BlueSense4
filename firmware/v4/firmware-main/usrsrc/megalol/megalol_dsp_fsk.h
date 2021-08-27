#ifndef __MEGALOL_DSP_FSK_H
#define __MEGALOL_DSP_FSK_H

#define FSKTESTSIGNALLEN 3926

extern short testsignal_recognizablespectrum[];
extern short testsignal_realundersampled_0[];

void fsk_gentestsignal2(signed short *target,int len,int sr,int fc,float bfs, float bff);
void fsk_gentestsignal2b(signed short *target);
void fsk_gentestsignal3(signed short *target);
void fsk_demod_test_1();
void fsk_demod_test_2();
void fsk_demod_test_3();
void fsk_demod_test_4();

void dsp_fsk_demod_decode_test_1();
void dsp_fsk_demod_decode_test_2();

void fsk_testdecode1();

void fsk_demod_init();
unsigned fsk_demod_frame(signed short * restrict data,unsigned char *restrict binaryout,unsigned debug);

void dsp_fsk_adc_demod_decode_loop(unsigned sr,unsigned br,unsigned autoplay);

void fsk_demodout_to_decodein();

void dsp_fsk_adc_demod_decode_help();
void dsp_fsk_adc_demod_decode_printstat(signed short *adcdata,unsigned len);


void fsk_gentestsignal(signed short *target,int len,int sr,int f0,int f1,int br);
void fsk_testpipeline1();
void fsk_demod_bench(signed short * restrict indata,int len);

#endif
