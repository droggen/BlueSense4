#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "stm32l496xx.h"
#include "megalol_dsp.h"
#include "stmcic.h"
#include "global.h"
#include "dfsdm.h"
#include "main.h"
#include "wait.h"
#include "stmadcfast.h"
#include "stm32l4xx_hal.h"

/******************************************************************************
	file: megalol_dsp_r2iq.c
*******************************************************************************

	Performs a real-to-iq conversion assuming a signal sampled at fs=4xfc.

	Procedure:
	1. Input data is 16-bit signed.
	2. Data is mixed with a complex exponential at fc, bringing the carrier to 0.
	3. Data is low-pass filtered with a CIC filter (configurable order) and downsampling ratio of 2.
	4. Output data is 32-bit, with only the upper 24-bit representing the signed result

	Two families of functions are provided: with CPU readout, with DMA readout. The results are numerically different (sample delay in the calibration of the pipeline) but
	identical from a signal processing point of view.

	DMA readout is almost 2x faster than CPU readout.

 	* Implementation details *

	In order to maximise speed the code does not wait for the CIC conversion ready flag (REOCF) and instead inserts NOPs.
	In CPU readout that increases speed by 50%+.

 	* Main functions*

		*Core functions*

		* dsp_real_to_iq_4x_cic_init_cpu					Initialise with CPU readout
		* dsp_real_to_iq_4x_cic_init_dma					Initialise with DMA readout
		* dsp_real_to_iq_4x_cic_process_cpu					Must be initialized with dsp_real_to_iq_4x_cic_init_cpu. Interleaved processing with CPU readout and offset
		* dsp_real_to_iq_4x_cic_process_dma					Must be initialized with dsp_real_to_iq_4x_cic_init_dma. Interleaved processing with DMA readout and offset (faster)

*/

// Set DSP_R2IQ_USENOP_INSTEAD_OF_WAIT to 1 to insert "NOP" instructions instead of busy-waiting the conversion complete flag.
// This speeds up the conversion, but may lead to issues if the compiler where to further optimise the code.
// This applies to the CPU read-out version of the filter.
// DMA readout is triggered by the conversion complete; however if the bus is busy
#define DSP_R2IQ_USENOP_INSTEAD_OF_WAIT		0
#define DSP_R2IQ_USEFASTERWAIT		1

/******************************************************************************
	function: dsp_real_to_iq_4x_cic_init
*******************************************************************************
	Initialise the streaming real-to-IQ conversion.


	Input:
		downsampling:	Downsampling ratio. For example: downsampling=2 for summing 2 samples, and downsampling by 2.
		order:			filter order. 1=sinc1, 2=sinc2, ... 5=sinc5

	Returns:
		-

******************************************************************************/
// CPU readout
void dsp_real_to_iq_4x_cic_init_cpu(unsigned downsampling,unsigned order)
{
	unsigned interleaved = 1;
	//unsigned interleaved = 0;
	unsigned dma=0;
	stm_hardcic_init(0,downsampling-1,order,interleaved,dma,0);			// Channel 0
	stm_hardcic_init(1,downsampling-1,order,interleaved,dma,0);			// Channel 1
}
// DMA readout
void dsp_real_to_iq_4x_cic_init_dma(unsigned downsampling,unsigned order)
{
	unsigned interleaved = 1;
	unsigned dma=1;
	stm_hardcic_init(0,downsampling-1,order,interleaved,dma,0);			// Channel 0
	stm_hardcic_init(1,downsampling-1,order,interleaved,dma,0);			// Channel 1
}

/******************************************************************************
	function: dsp_real_to_iq_4x_cic_process_cpu
*******************************************************************************
	Convert real samples to IQ, assuming the sample rate is 4x the carrier.

	The processs is:
	1. Mixing with complex exponential at 4xfc
	2. CIC low-pass filtering at 2xfc
	3. Downsampling by 2

	The multiplication sequence is:
	I: [0 1 0 -1]
	Q: [1 0 -1 0]

	CIC channel 0: I
	CIC channel 1: Q

Benchmark:
	At 80MHz
		6205/s (with 3 nop; extra nop reduces to 6084/s);

TODO: benchmark multiple options.
	Non-interleaved write: 2x32-bit write, DMA 32-bit read
	Non-interleaved write: 2x16-bit write, DMA 32-bit read
	Interleaved write: 1x32-bit write, DMA 32-bit read
	Same with DMA 16-bit read (only higher 16-bits)

******************************************************************************/
unsigned dsp_real_to_iq_4x_cic_process_cpu(short * restrict adc,int * restrict outi,int *restrict outq,int inlen,short subtract)
{
	// Status
	unsigned wait0=0,wait1=0,wait2=0,wait3=0;


	uint32_t subtractsimd = simd_s16_to_s16s16(subtract);		// | mean | mean |
	uint32_t z = 0;				// | zero | zero |
	uint32_t sa,sb;
	uint32_t *restrict adcsimd=(uint32_t*restrict)adc;

	// 16-bit write pointers
	//volatile short * cic0wr = (short*)&DFSDM1_Channel0->CHDATINR;
	//volatile short * cic1wr = (short*)&DFSDM1_Channel1->CHDATINR;
	//volatile short *cic0rd = (short*)&DFSDM1_Filter0->FLTRDATAR;
	//volatile short *cic1rd = (short*)&DFSDM1_Filter1->FLTRDATAR;

	int *inptr = (int*)adc;

	//for(int i=0;i<inlen;i+=4)
	for(int i=0;i<inlen/2;i+=2)
	{
		// Read 4 samples
		// Multiplication is:
		// I: 0 1 0 -1
		// Q: 1 0 -1 0

		// Sample 0, 1
		sa = *adcsimd++;
		// Subtract the mean
		sa = __SSUB16(sa,subtractsimd);

		/*if(i<5)
		{
			fprintf(file_pri,"Sample %d-%d: %d %d %d %d\n",i,i+3,adc[i+0],adc[i+1],adc[i+2],adc[i+3]);
			fprintf(file_pri,"Mean: %d. meansimd: %08X\n",subtract,subtractsimd);
			fprintf(file_pri,"sa: %08x\n",sa);
			fprintf(file_pri,"ch0 write: %08X\n",sa&0xffff0000);
			fprintf(file_pri,"ch1 write: %08X\n",sa&0x0000ffff);

		}*/

		// Write first 2 samples: mask and write
		DFSDM1_Channel0->CHDATINR = sa&0xffff0000;			// 0, I
		DFSDM1_Channel1->CHDATINR = sa&0x0000ffff;			// Q, 0




		// **CAUTION**: this read must be prior to fetching the result to give time to CIC to complete the operation
		// Next 2 samples: must change sign.
		uint32_t sb = *adcsimd++;	// sample 2, 3
		// Subtract the mean and change sign: -(sb-meansimd) = meansimd-sb
		sb = __SSUB16(subtractsimd,sb);

//#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
		//while(!(DFSDM1_Filter1->FLTISR&2));	// Faster alternative: wait only on 2nd filter and no wait counter
//#endif


		// Get I
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
		while(!(DFSDM1_Filter0->FLTISR&2)) wait0++;
#endif
		//outi[i>>1] =  (signed int)DFSDM1_Filter0->FLTRDATAR;
		outi[i] =  (signed int)DFSDM1_Filter0->FLTRDATAR;





		// Get Q
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
		while(!(DFSDM1_Filter1->FLTISR&2)) wait1++;
#endif
		//outq[i>>1] =  (signed int)DFSDM1_Filter1->FLTRDATAR;
		outq[i] =  (signed int)DFSDM1_Filter1->FLTRDATAR;



		//HAL_Delay(1);
		/*if(i<5)
		{
			fprintf(file_pri,"-sb: %08x\n",sb);
			fprintf(file_pri,"ch0 write: %08X\n",sb&0xffff0000);
			fprintf(file_pri,"ch1 write: %08X\n",sb&0x0000ffff);
		}*/


		// Write next 2 samples: mask and write
		DFSDM1_Channel0->CHDATINR = sb&0xffff0000;

#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT==1
		// **CAUTION**: the *1* NOPs are mandatory.
		__NOP();
#endif

		DFSDM1_Channel1->CHDATINR = sb&0x0000ffff;

#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT==1
		// **CAUTION**: the *3* NOPs are mandatory.
		__NOP();
		__NOP();
		__NOP();
#endif

		//while(!(DFSDM1_Filter1->FLTISR&2));	// Faster alternative: wait only on 2nd filter and no wait counter

		// Get I
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
		while(!(DFSDM1_Filter0->FLTISR&2)) wait2++;
#endif
		//outi[(i>>1)+1] =  (signed int)DFSDM1_Filter0->FLTRDATAR;
		outi[i+1] =  (signed int)DFSDM1_Filter0->FLTRDATAR;
		// Get Q
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
		while(!(DFSDM1_Filter1->FLTISR&2)) wait3++;
#endif
		//outq[(i>>1)+1] =  (signed int)DFSDM1_Filter1->FLTRDATAR;
		outq[i+1] =  (signed int)DFSDM1_Filter1->FLTRDATAR;

	}
	//fprintf(file_pri,"Waits: %d %d %d %d\n",wait0,wait1,wait2,wait3);
	return wait0+wait1+wait2+wait3;
}
/******************************************************************************
	function: dsp_real_to_iq_4x_cic_process_cpu_msb
*******************************************************************************
	Same as dsp_real_to_iq_4x_cic_process_cpu but only stores the upper 16-bits
	of the CIC result to memory.
******************************************************************************/
unsigned dsp_real_to_iq_4x_cic_process_cpu_msb(short * restrict adc,short * restrict outi,short *restrict outq,int inlen,short subtract)
{
	// Status
	unsigned wait0=0,wait1=0,wait2=0,wait3=0;


	uint32_t subtractsimd = simd_s16_to_s16s16(subtract);		// | mean | mean |
	uint32_t z = 0;				// | zero | zero |
	uint32_t sa,sb;
	uint32_t *restrict adcsimd=(uint32_t*restrict)adc;

	// 16-bit write pointers
	//volatile short * cic0wr = (short*)&DFSDM1_Channel0->CHDATINR;
	//volatile short * cic1wr = (short*)&DFSDM1_Channel1->CHDATINR;
	volatile short *cic0rd = ((short*)&DFSDM1_Filter0->FLTRDATAR)+1;		// Point to upper 16-bits of register
	volatile short *cic1rd = ((short*)&DFSDM1_Filter1->FLTRDATAR)+1;

	int *inptr = (int*)adc;

	//for(int i=0;i<inlen;i+=4)
	for(int i=0;i<inlen/2;i+=2)
	{
		// Read 4 samples
		// Multiplication is:
		// I: 0 1 0 -1
		// Q: 1 0 -1 0

		// Sample 0, 1
		sa = *adcsimd++;
		// Subtract the mean
		sa = __SSUB16(sa,subtractsimd);

		/*if(i<5)
		{
			fprintf(file_pri,"Sample %d-%d: %d %d %d %d\n",i,i+3,adc[i+0],adc[i+1],adc[i+2],adc[i+3]);
			fprintf(file_pri,"Mean: %d. meansimd: %08X\n",subtract,subtractsimd);
			fprintf(file_pri,"sa: %08x\n",sa);
			fprintf(file_pri,"ch0 write: %08X\n",sa&0xffff0000);
			fprintf(file_pri,"ch1 write: %08X\n",sa&0x0000ffff);

		}*/

		// Write first 2 samples: mask and write
		DFSDM1_Channel0->CHDATINR = sa&0xffff0000;			// 0, I
		DFSDM1_Channel1->CHDATINR = sa&0x0000ffff;			// Q, 0


		// **CAUTION**: this read must be prior to fetching the result to give time to CIC to complete the operation
		// Next 2 samples: must change sign.
		uint32_t sb = *adcsimd++;	// sample 2, 3
		// Subtract the mean and change sign: -(sb-meansimd) = meansimd-sb
		sb = __SSUB16(subtractsimd,sb);


		// Get I
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
#if DSP_R2IQ_USEFASTERWAIT==0
		while(!(DFSDM1_Filter0->FLTISR&2)) wait0++;
#else
		//while(!(DFSDM1_Filter1->FLTISR&2)) wait1++;	// Wait only on the 2nd filter - the 1st filter should be ready when the second is.
		//while(!(DFSDM1_Filter0->FLTISR&2)) wait0++;
		while(!(DFSDM1_Filter0->FLTISR&2));
#endif
#endif
		//outi[i>>1] =  (signed int)DFSDM1_Filter0->FLTRDATAR;
		//outi[i>>1] =  *cic0rd;
		outi[i] =  *cic0rd;
		//outi[i] =  (short)((signed int)DFSDM1_Filter0->FLTRDATAR)>>16;
		// Get Q
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
#if DSP_R2IQ_USEFASTERWAIT==0
		while(!(DFSDM1_Filter1->FLTISR&2)) wait1++;
#endif
#endif
		//outq[i>>1] =  (signed int)DFSDM1_Filter1->FLTRDATAR;
		//outq[i>>1] =  *cic1rd;
		outq[i] =  *cic1rd;
		//outq[i] =  (short)((signed int)DFSDM1_Filter1->FLTRDATAR)>>16;



		//HAL_Delay(1);
		/*if(i<5)
		{
			fprintf(file_pri,"-sb: %08x\n",sb);
			fprintf(file_pri,"ch0 write: %08X\n",sb&0xffff0000);
			fprintf(file_pri,"ch1 write: %08X\n",sb&0x0000ffff);
		}*/

/*
 236 us nop nowait
 358 us normalwait
 342 us fasterwait on 2nd filter with counter
 323 us fasterwait on 1st filter with counter
 291 us fasterwait on 1st filter no counter
*/


		// Write next 2 samples: mask and write
		DFSDM1_Channel0->CHDATINR = sb&0xffff0000;
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT==1
		// **CAUTION**: the *1* NOPs are mandatory.
		__NOP();
#endif
		DFSDM1_Channel1->CHDATINR = sb&0x0000ffff;

#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT==1
		// **CAUTION**: the *5* NOPs are mandatory.
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
#endif


		// Without wait: 1 nop between write, 2 nop after; or 3 nop after.


		// Get I
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
#if DSP_R2IQ_USEFASTERWAIT==0
		while(!(DFSDM1_Filter0->FLTISR&2)) wait2++;
#else
		//while(!(DFSDM1_Filter1->FLTISR&2)) wait3++;	// Wait only on the 2nd filter - the 1st filter should be ready when the second is.
		//while(!(DFSDM1_Filter0->FLTISR&2)) wait2++;
		while(!(DFSDM1_Filter0->FLTISR&2));
#endif
#endif
		//outi[(i>>1)+1] =  (signed int)DFSDM1_Filter0->FLTRDATAR;
		//outi[(i>>1)+1] =  *cic0rd;
		outi[i+1] =  *cic0rd;
		//outi[i+1] =  (short)((signed int)DFSDM1_Filter0->FLTRDATAR)>>16;
		// Get Q
#if DSP_R2IQ_USENOP_INSTEAD_OF_WAIT!=1
#if DSP_R2IQ_USEFASTERWAIT==0
		while(!(DFSDM1_Filter1->FLTISR&2)) wait3++;
#endif
#endif
		//outq[(i>>1)+1] =  (signed int)DFSDM1_Filter1->FLTRDATAR;
		//outq[(i>>1)+1] =  *cic1rd;
		outq[i+1] =  *cic1rd;
		//outq[i+1] =  (short)((signed int)DFSDM1_Filter1->FLTRDATAR)>>16;

	}
	//fprintf(file_pri,"Waits: %d %d %d %d\n",wait0,wait1,wait2,wait3);
	return wait0+wait1+wait2+wait3;
}
/*
Benchmark:
	At 80MHz
		11992/s

 */
unsigned dsp_real_to_iq_4x_cic_process_dma(short * restrict adc,int *restrict outi,int *restrict outq,int inlen,short subtract)
{
	// Status
	unsigned wait0=0,wait1=0,wait2=0,wait3=0;
	uint32_t subtractsimd = simd_s16_to_s16s16(subtract);		// | mean | mean |
	uint32_t z = 0;				// | zero | zero |
	uint32_t sa,sb;



	// Initialise the dma
	stm_hardcic_process_dma_setup(0,outi,inlen/2);
	stm_hardcic_process_dma_setup(1,outq,inlen/2);

	// 16-bit write pointers
	//volatile short *cic0wr = (short*)&DFSDM1_Channel0->CHDATINR;
	//volatile short *cic1wr = (short*)&DFSDM1_Channel1->CHDATINR;

	uint32_t *restrict adcsimd=(uint32_t*restrict)adc;



	for(int i=0;i<inlen;i+=4)
	{
	#if 1
		// Read 4 samples
		// Multiplication is:
		// I: 0 1 0 -1
		// Q: 1 0 -1 0

		// Sample 0, 1
		sa = *adcsimd++;
		// Subtract the mean
		sa = __SSUB16(sa,subtractsimd);

		/*if(i<5)
		{
			fprintf(file_pri,"Sample %d-%d: %d %d %d %d\n",i,i+3,adc[i+0],adc[i+1],adc[i+2],adc[i+3]);
			fprintf(file_pri,"Mean: %d. meansimd: %08X\n",subtract,subtractsimd);
			fprintf(file_pri,"sa: %08x\n",sa);
			fprintf(file_pri,"ch0 write: %08X\n",sa&0xffff0000);
			fprintf(file_pri,"ch1 write: %08X\n",sa&0x0000ffff);

		}*/

		// First 2 samples: mask and write
		DFSDM1_Channel0->CHDATINR = sa&0xffff0000;			// 0, I
		DFSDM1_Channel1->CHDATINR = sa&0x0000ffff;			// Q, 0

		// **CAUTION**: the NOPs are mandatory.
		// Only way this code works is with 3 NOP after first and 2nd write
		__NOP();
		__NOP();
		__NOP();


		//while(!(DFSDM1_Filter0->FLTISR&2)) wait0++;
		//while(!(DFSDM1_Filter1->FLTISR&2)) wait1++;

		// Next 2 samples: must change sign.
		uint32_t sb = *adcsimd++;	// sample 2, 3

		// Subtract the mean and change sign: -(sb-meansimd) = meansimd-sb
		sb = __SSUB16(subtractsimd,sb);

		//HAL_Delay(1);
		/*if(i<5)
		{
			fprintf(file_pri,"-sb: %08x\n",sb);
			fprintf(file_pri,"ch0 write: %08X\n",sb&0xffff0000);
			fprintf(file_pri,"ch1 write: %08X\n",sb&0x0000ffff);
		}*/

		//HAL_Delay(1);


		// **CAUTION**: the NOPs are mandatory.
		// Only way this code works is with NOP after channel0 and after channel1 write

		// Mask and write
		DFSDM1_Channel0->CHDATINR = sb&0xffff0000;
		__NOP();
		DFSDM1_Channel1->CHDATINR = sb&0x0000ffff;
		__NOP();

		//HAL_Delay(1);


	#endif


	}
	return 0;
}
// Split data into two I, Q lanes for subsequent CIC filtering
// No filtering is done
// I lane will be 0 I 0 -I
// Q lane will be Q 0 -Q 0
unsigned dsp_real_to_iq_4x_split(short * restrict adc,short *restrict outi,short *restrict outq,int inlen,short subtract)
{
	// Status
	unsigned wait0=0,wait1=0,wait2=0,wait3=0;
	uint32_t subtractsimd = simd_s16_to_s16s16(subtract);		// | mean | mean |
	uint32_t z = 0;				// | zero | zero |
	uint32_t sa,sb;




	// 16-bit write pointers
	//volatile short *cic0wr = (short*)&DFSDM1_Channel0->CHDATINR;
	//volatile short *cic1wr = (short*)&DFSDM1_Channel1->CHDATINR;

	uint32_t *restrict adcsimd=(uint32_t*restrict)adc;
	uint32_t *restrict outisimd=(uint32_t*restrict)outi;
	uint32_t *restrict outqsimd=(uint32_t*restrict)outq;



	//for(int i=0;i<inlen;i+=4)
	for(unsigned i=0;i<inlen/2;i+=2)
	{
		// Read 4 samples
		// Multiplication is:
		// I: 0 1 0 -1
		// Q: 1 0 -1 0

		// Sample 0, 1
		//sa = *adcsimd++;
		sa=adcsimd[i];
		// Subtract the mean
		sa = __SSUB16(sa,subtractsimd);

		// First 2 samples: mask and write
		outisimd[i] = sa&0xffff0000;			// 0, I
		outqsimd[i] = sa&0x0000ffff;			// Q, 0

		// Next 2 samples: must change sign.
		//uint32_t sb = *adcsimd++;	// sample 2, 3
		uint32_t sb = adcsimd[i+1];	// sample 2, 3

		// Subtract the mean and change sign: -(sb-meansimd) = meansimd-sb
		sb = __SSUB16(subtractsimd,sb);


		// First 2 samples: mask and write
		outisimd[i+1] = sb&0xffff0000;
		outqsimd[i+1] = sb&0x0000ffff;


	}
	return 0;
}
void dsp_r2iq_test_cpu(short *adc,int inlen,int *outi, int *outq,unsigned order)
{
	// Test the CPU version
	fprintf(file_pri,"========== Benchmarking real to IQ: CPU readout. Order=%d ==========\n",order);

	// Initialise the buffers
	for(int i=0;i<CICBUFSIZ;i++)
	{
		outi[i]=0xFFFFF;
		outq[i]=0xFFFFF;
	}

	// Initialise the CIC
	dsp_real_to_iq_4x_cic_init_cpu(2,order);
	// Feed data
	unsigned long int t_last=timer_s_wait(),t_cur;
	unsigned long int tint1=timer_ms_get_intclk();
	unsigned long int nsample=0;
	unsigned mintime=1;
	unsigned wait=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		wait+=dsp_real_to_iq_4x_cic_process_cpu(adc,outi,outq,CICBUFSIZ,0);
		nsample++;
		//break;
	}
	unsigned long int tint2=timer_ms_get_intclk();


	fprintf(file_pri,"Time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"Total wait: %u\n",wait);

	fprintf(file_pri,"OUTI: "); for(int i=0;i<CICBUFSIZ/2;i++) fprintf(file_pri,"%+05d ",outi[i]>>8); fprintf(file_pri,"\n");
	fprintf(file_pri,"OUTQ: "); for(int i=0;i<CICBUFSIZ/2;i++) fprintf(file_pri,"%+05d ",outq[i]>>8); fprintf(file_pri,"\n");
	fprintf(file_pri,"==========  ==========\n");
}
void dsp_r2iq_test_cpu_msb(short *adc,int inlen,short *outi, short *outq,unsigned order)
{
	// Test the CPU version
	fprintf(file_pri,"========== Benchmarking real to IQ: CPU readout. MSB only. Order=%d ==========\n",order);

	// Initialise the buffers
	for(int i=0;i<CICBUFSIZ;i++)
	{
		outi[i]=0xFFFFF;
		outq[i]=0xFFFFF;
	}

	// Initialise the CIC
	dsp_real_to_iq_4x_cic_init_cpu(2,order);
	// Feed data
	unsigned long int t_last=timer_s_wait(),t_cur;
	unsigned long int tint1=timer_ms_get_intclk();
	unsigned long int nsample=0;
	unsigned mintime=1;
	unsigned wait=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		wait+=dsp_real_to_iq_4x_cic_process_cpu_msb(adc,outi,outq,CICBUFSIZ,0);
		nsample++;
		//break;
	}
	unsigned long int tint2=timer_ms_get_intclk();


	fprintf(file_pri,"Time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"Total wait: %u\n",wait);

	fprintf(file_pri,"OUTI: "); for(int i=0;i<CICBUFSIZ/2;i++) fprintf(file_pri,"%+05d ",outi[i]); fprintf(file_pri,"\n");		// Print without shifting by 8
	fprintf(file_pri,"OUTQ: "); for(int i=0;i<CICBUFSIZ/2;i++) fprintf(file_pri,"%+05d ",outq[i]); fprintf(file_pri,"\n");
	fprintf(file_pri,"==========  ==========\n");
}
void dsp_r2iq_test_dma(short *adc,int inlen,int *outi, int *outq,unsigned order)
{
	// Test the CPU version
	fprintf(file_pri,"========== Benchmarking real to IQ: DMA readout. Order=%d ==========\n",order);

	// Initialise the buffers
	for(int i=0;i<CICBUFSIZ;i++)
	{
		outi[i]=0xFFFFF;
		outq[i]=0xFFFFF;
	}

	// Initialise the CIC
	dsp_real_to_iq_4x_cic_init_dma(2,order);
	// Feed data
	unsigned long int t_last=timer_s_wait(),t_cur;
	unsigned long int tint1=timer_ms_get_intclk();
	unsigned long int nsample=0;
	unsigned mintime=1;
	unsigned wait=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		wait+=dsp_real_to_iq_4x_cic_process_dma(adc,outi,outq,CICBUFSIZ,0);
		nsample++;
		//break;
	}
	unsigned long int tint2=timer_ms_get_intclk();


	fprintf(file_pri,"Time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"Total wait: %u\n",wait);

	fprintf(file_pri,"OUTI: "); for(int i=0;i<CICBUFSIZ/2;i++) fprintf(file_pri,"%+05d ",outi[i]>>8); fprintf(file_pri,"\n");
	fprintf(file_pri,"OUTQ: "); for(int i=0;i<CICBUFSIZ/2;i++) fprintf(file_pri,"%+05d ",outq[i]>>8); fprintf(file_pri,"\n");
	fprintf(file_pri,"==========  ==========\n");
}
void dsp_r2iq_test()
{
	short adc[CICBUFSIZ];		// Input data
	int outi[CICBUFSIZ];		// Output I
	int outq[CICBUFSIZ];		// Output Q
	// Other work variables
	unsigned long int t_last,t_cur;
	unsigned long int ctr,cps,nsample;
	const unsigned long mintime=1;
	unsigned long tint1,tint2;
	unsigned wait=0;

	// Initialise the buffers
	for(int i=0;i<CICBUFSIZ;i++)
	{
		adc[i]=(i&0x7f);
		//adc[i] = 16;
	}
	fprintf(file_pri,"Input: "); for(int i=0;i<CICBUFSIZ;i++) fprintf(file_pri,"%05d ",adc[i]); fprintf(file_pri,"\n");

	unsigned order = 5;

	dsp_r2iq_test_cpu(adc,CICBUFSIZ,outi,outq,order);
	dsp_r2iq_test_dma(adc,CICBUFSIZ,outi,outq,order);

	// The MSB only functions discard the lower 8 bits of data. Bit growth is 4-bit.
	// Value of 1 becomes 16. To make this appear in MSB it must be 256, but to give identical result it should be 256*16 -> multiply by 256
	for(int i=0;i<CICBUFSIZ;i++)
	{
		//adc[i]=adc[i]*16;
		adc[i]=adc[i]*256;
	}
	dsp_r2iq_test_cpu_msb(adc,CICBUFSIZ,(short*)outi,(short*)outq,order);	// Cast to short


#if 0
	// Crossproduct
	int cp[CICBUFSIZ/2];
	t_last=timer_s_wait();
	tint1=timer_ms_get_intclk();
	nsample=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		dsp_crossproduct(outi,outq,cp,CICBUFSIZ/2-1);
		nsample++;
	}
	tint2=timer_ms_get_intclk();
	fprintf(file_pri,"Crossproduct 32x32->32 time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));

	// Crossproduct
	t_last=timer_s_wait();
	tint1=timer_ms_get_intclk();
	nsample=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		dsp_crossproduct_s((short*)outi,(short*)outq,cp,CICBUFSIZ/2-1);
		nsample++;
	}
	tint2=timer_ms_get_intclk();
	fprintf(file_pri,"Crossproduct 16x16->32 time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));
#endif

}



