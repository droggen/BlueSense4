#ifndef __STMCIC_H
#define __STMCIC_H

// Public functions
unsigned stm_hardcic_init(unsigned cicchannel,unsigned winsize,unsigned order,unsigned interleaved,unsigned dma,unsigned msbonly);
void stm_hardcic_process_dma_setup(unsigned cicchannel,unsigned *dataout,unsigned outlen);
void stm_hardcic_process_dma_waitcomplete(unsigned cicchannel);
unsigned stm_hardcic_process_dma_remaining(unsigned cicchannel);
void stm_hardcic_offset(int cicchannel,int offset);

// Used internally
unsigned _stm_hardcic_fill_dma(unsigned cicchannel);
unsigned _stm_hardcic_fill_nodma(unsigned cicchannel);

// Debug
void printdmastatus();

/******************************************************************************
	HARDCIC - HIGH-LEVEL - HARDCIC - HIGH-LEVEL - HARDCIC - HIGH-LEVEL - HARDCIC - HIGH-LEVEL -
********************************************************************************/
unsigned stm_hardcic_process_stream_16_32_cpu_init(unsigned cicchannel,unsigned downsampling,unsigned order);
unsigned  stm_hardcic_process_stream_16_32_cpu_process(unsigned cicchannel,short * restrict input,int *restrict output,unsigned inlen);

#if 0
/******************************************************************************
	SOFTWARE CIC - INCOMPLETE - SOFTWARE CIC - INCOMPLETE - SOFTWARE CIC - INCOMPLETE
*******************************************************************************
*/
void cic_4_2_stream_print(double *w,int n,int d,int size);
void cic_4_2_short_stream_print(short *w,int n,int d,int size);
void cic_4_2_stream(double *x,int size,double *xf);
void cic_4_2_short_stream_init(short *w);
void cic_4_2_short_stream_process(short * restrict x,short * restrict w,int size,short * restrict xf);
void cic_4_2_short_stream_process2(short * restrict w,short * restrict xf);
double *cic_4_2(double *x,int size,double *xf);
#endif

/******************************************************************************
	HARDCIC - TEST FUNCTIONS - HARDCIC - TEST FUNCTIONS - HARDCIC - TEST FUNCTIONS -
********************************************************************************/

void hardcic_test_gendata1(short *in,short *in2,int len);
void hardcic_test_gendata2(short *in,short *in2,int len);
void hardcic_test_functional();
void hardcic_test_functional_wsingle_rcpu(unsigned downsampling,unsigned order,short *in1,short *in2,int len);
void hardcic_test_functional_winterleaved_rcpu(unsigned downsampling,unsigned order,short *in1,short *in2,int len);
void hardcic_test_functional_wsingle_rdma(unsigned downsampling,unsigned order,short *in1,short *in2,int len);
void hardcic_test_functional_winterleaved_rdma(unsigned downsampling,unsigned order,short *in1,short *in2,int len);

/******************************************************************************
	HARDCIC - BENCHMARK - HARDCIC - BENCHMARK - HARDCIC - BENCHMARK - HARDCIC -
********************************************************************************/
void hardcic_bench();
void hardcic_bench_wsingle_rcpu();



#endif
