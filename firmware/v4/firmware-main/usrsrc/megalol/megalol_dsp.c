#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "stm32l496xx.h"
#include "megalol_dsp.h"
#include "megalol_dsp_r2iq.h"
#include "stmcic.h"
#include "global.h"
#include "dfsdm.h"
#include "megalol_comm_softuart.h"
#include "main.h"
#include "helper.h"
#include "ufat.h"
#include "wait.h"
#include <math.h>
#include "megalol_dsp_fsk.h"
#include "stmadcfast.h"
#include "stm32l4xx_hal.h"

/*
 * Current compiler: -mfpu=fpv4-sp-d16
 *
 * https://embeddedartistry.com/blog/2017/10/11/demystifying-arm-floating-point-compiler-options/
 * helium: https://www.eejournal.com/article/arm-floats-helium-for-cortex-m/
 * https://www.ceva-dsp.com/
 * https://www.dspg.com/
 *
 *
 * Intrinsics: https://developer.arm.com/architectures/instruction-sets/intrinsics
 *
 */
/*

	Functional test:
	V 1) Write single, CPU read
	V 2) Write interleaved, CPU read
	3) Write single, DMA read
	4) Write interleavd, DMA read

	TODO: use CIC shift to correct offet.

	stage 0: m
	stage 1: 2m
	stage 2: 4m
	stage 3: 8m
	stage 4: 16m
	stage 5: 32m: max 131072 ok with 24-bit shift in register

*/








#define MEGALOLDSPDEBUG 0



/*
DFSDM is on APB2
APB2 32MHz: Time: 1s (1000 intclk ms) Frames: 4777. Frames/sec: 4777
APB2 16MHz: Time: 1s (1000 intclk ms) Frames: 3453. Frames/sec: 3453


*/

// end



void dsp_crossproduct(int *outi,int *outq,int *cp,int inlen)
{
	for(int i=0;i<inlen-1;i++)
	{
		cp[i] = outi[i]*outq[i+1]-outi[i+1]*outq[i];
	}
}
/* Streaming version of crossproduct
	This version gets a frame of inlen current samples, and a past sample.
	The past sample is at address outi, outq.
	The "current" sample is at outi[1] and outq[1].

	In total inlen output cross-products are calculated with the first cp being between the past and current sample.
	The output cp is of size inlen.
	The input vectors must be of size inlen+1.

*/
__attribute__((optimize("unroll-loops")))
//void dsp_crossproduct_stream(int *outi,int *outq,int *cp,int inlen)
void dsp_crossproduct_stream(int * restrict outi,int * restrict outq,int * restrict cp,int inlen)
{
	for(int i=0;i<inlen;i++)
	{
		cp[i] = outi[i]*outq[i+1]-outi[i+1]*outq[i];
	}
}
__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_sr(int * restrict outi,int * restrict outq,int * restrict cp,int inlen,unsigned sr)
{
	for(int i=0;i<inlen;i++)
	{
		cp[i] = (outi[i]>>sr)*(outq[i+1]>>sr)-(outi[i+1]>>sr)*(outq[i]>>sr);
	}
}
__attribute__((optimize("unroll-loops")))
void dsp_crossproductsign_stream_sr(int * restrict outi,int * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr)
{
	for(int i=0;i<inlen;i++)
	{
		int cp = (outi[i]>>sr)*(outq[i+1]>>sr)-(outi[i+1]>>sr)*(outq[i]>>sr);
		cps[i] = (cp>0)?1:0;
	}
}
// Cross product of only the upper 16-bits of the 32-bit CIC result
__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_h16(int * restrict outi,int * restrict outq,int * restrict cp,int inlen)
{
	for(int i=0;i<inlen;i++)
	{
		cp[i] = (outi[i]>>16)*(outq[i+1]>>16)-(outi[i+1]>>16)*(outq[i]>>16);
		/*if(i<10)
		{
			fprintf(file_pri,"%d. I i: %d I i+1: %d Q i: %d Q i+1: %d\n",i,outi[i],outi[i+1],outq[i],outq[i+1]);
			fprintf(file_pri,"\tI i: %d I i+1: %d Q i: %d Q i+1: %d\n",outi[i]>>16,outi[i+1]>>16,outq[i]>>16,outq[i+1]>>16);
			fprintf(file_pri,"\tcp: %d\n",cp[i]);
		}*/
	}
}
// Cross product of only the upper 16-bits of the 32-bit CIC result
//__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_h16_2(int * restrict outi,int * restrict outq,int * restrict cp,int inlen)
{
	// Read only upper 16-bits
	short *restrict outi16 = ((short*restrict)outi)+1;
	short *restrict outq16 = ((short*restrict)outq)+1;

	for(int i=0;i<inlen;i++)
	{
		//cp[i] = (outi[i]>>16)*(outq[i+1]>>16)-(outi[i+1]>>16)*(outq[i]>>16);
		cp[i] = (outi16[2*i])*(outq16[2*(i+1)])-(outi16[2*(i+1)])*(outq16[2*i]);
		/*if(i<10)
		{
			fprintf(file_pri,"%d. I i: %d I i+1: %d Q i: %d Q i+1: %d\n",i,outi16[2*i],outi16[2*(i+1)],outq16[2*i],outq16[2*(i+1)]);
			fprintf(file_pri,"\tcp: %d\n",cp[i]);
		}*/
	}
}
// GCC defines as bit operation even though the instruction exists.
#define __PKHTB_DAN(ARG1,ARG2,ARG3) \
({                          \
  uint32_t __RES, __ARG1 = (ARG1), __ARG2 = (ARG2); \
  if (ARG3 == 0) \
    __ASM ("pkhtb %0, %1, %2" : "=r" (__RES) :  "r" (__ARG1), "r" (__ARG2)  ); \
  else \
    __ASM ("pkhtb %0, %1, %2, asr %3" : "=r" (__RES) :  "r" (__ARG1), "r" (__ARG2), "I" (ARG3)  ); \
  __RES; \
 })



// Cross product of only the upper 16-bits of the 32-bit CIC result
//__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_h16_simd(int * restrict outi,int * restrict outq,int * restrict cp,int inlen)
{
	uint32_t * restrict is = outi;
	uint32_t * restrict qs = outq;


	// is: IIIIIIXX - keep only upper 16

	for(int i=0;i<inlen;i++)
	{
		uint32_t i1 = is[i];		// 111111xx
		uint32_t i2 = is[i+1];		// 222222xx
		// Combine into 11112222 discarding the lower 16 bits
		//uint32_t ii = (i1&0xffff0000) | (i2>>16);
		uint32_t ii = __PKHTB_DAN(i1,i2,16);


		uint32_t q1 = qs[i];		// 111111xx
		uint32_t q2 = qs[i+1];		// 222222xx
		// Combine into 11112222 discarding the lower 16 bits
		//uint32_t qq = (q1&0xffff0000) | (q2>>16);
		//uint32_t qq = __PKHBT(q2>>16,q1,0);
		uint32_t qq = __PKHTB_DAN(q1,q2,16);

		cp[i] = __SMLSDX(qq,ii,0);

		//cp[i] = (outi[i]>>16)*(outq[i+1]>>16)-(outi[i+1]>>16)*(outq[i]>>16);
		//cp[i] = (outi16[2*i])*(outq16[2*(i+1)])-(outi16[2*(i+1)])*(outq16[2*i]);
		/*if(i<10)
		{
			fprintf(file_pri,"%d. I i: %d I i+1: %d Q i: %d Q i+1: %d\n",i,outi[i]>>16,outi[i+1]>>16,outq[i]>>16,outq[i+1]>>16);
			fprintf(file_pri,"%d. I i: %08X I i+1: %08X Q i: %08X Q i+1: %08X\n",i,outi[i]>>16,outi[i+1]>>16,outq[i]>>16,outq[i+1]>>16);
			fprintf(file_pri,"%d. I %08x Q: %08X\n",i,ii,qq);
			fprintf(file_pri,"\tcp: %d\n",cp[i]);
		}*/
	}
}


/*void dsp_crossproduct_stream_sr_s(int * restrict outi,int * restrict outq,int * restrict cp,int inlen,unsigned sr)
{
	for(int i=0;i<inlen;i++)
	{
		cp[i] = (outi[i]>>sr)*(outq[i+1]>>sr)-(outi[i+1]>>sr)*(outq[i]>>sr);
	}
}*/

void dsp_crossproduct_stream_sr2(int * restrict outi,int * restrict outq,int * restrict cp,int inlen,unsigned sr)
{
	int a,b,c,d;
	a = outi[0]>>sr;
	d = outq[0]>>sr;

	for(int i=0;i<inlen;i+=2)
	{
		//cp[i] = (outi[i]>>sr)*(outq[i+1]>>sr)-(outi[i+1]>>sr)*(outq[i]>>sr);
		b = outq[i+1]>>sr;
		c = outi[i+1]>>sr;

		cp[i] = a*b-c*d;

		//cp[i+1] = (outi[i+1]>>sr)*(outq[i+2]>>sr)-(outi[i+2]>>sr)*(outq[i+1]>>sr);
		a = outi[i+2]>>sr;
		d = outq[i+2]>>sr;
		cp[i+1] = c*d - a*b;

		// shift values



	}
}
__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_iis(int * restrict outi,int * restrict outq,short * restrict cp,int inlen)
{
	for(int i=0;i<inlen;i++)
	{
		cp[i] = (outi[i]*outq[i+1]-outi[i+1]*outq[i])>>16;
	}
}
/*
	DSP implementation
	__SMLSDX(val1,val2,val3):
		p1 = val1[15:0]  * val2[31:16]
		p2 = val1[31:16] * val2[15:0]
		res[31:0] = p1 - p2 + val3[31:0]



*/
__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_dsp(short * restrict outi,short * restrict outq,int * restrict cp,int inlen)
{
	//uint32_t *restrict pi = (uint32_t*restrict)outi;
	//uint32_t *restrict pq = (uint32_t*restrict)outq;

	for(int i=0;i<inlen;i++)
	{
		uint32_t a = *(uint32_t*)&outi[i];
		uint32_t b = *(uint32_t*)&outq[i];

		//cp[i] = __SMLSDX(a,b,0);
		cp[i] = __SMLSDX(a,b,0)>>16;
	}

	/*for(int i=0;i<inlen;i++)
	{
		cp[i] = outi[i]*outq[i+1]-outi[i+1]*outq[i];
	}*/
}
#if 1
__attribute__((optimize("unroll-loops")))
void dsp_crossproduct_stream_dsp_iii_sr(int * restrict outi,int * restrict outq,int * restrict cp,int inlen,int sr)
{
#warning Could certainly be optimised
	//
	//uint32_t *restrict pi = (uint32_t*restrict)outi;
	//uint32_t *restrict pq = (uint32_t*restrict)outq;

	for(int i=0;i<inlen;i++)
	{
		short is[2],qs[2];
		is[0] = outi[i]>>sr;
		is[1] = outi[i+1]>>sr;
		qs[0] = outq[i]>>sr;
		qs[1] = outq[i+1]>>sr;

		//uint32_t a = *(uint32_t*)&outi[i];
		//uint32_t b = *(uint32_t*)&outq[i];
		uint32_t a = *(uint32_t*)is;
		uint32_t b = *(uint32_t*)qs;

		//cp[i] = __SMLSDX(a,b,0);
		cp[i] = __SMLSDX(a,b,0)>>16;
	}

	/*for(int i=0;i<inlen;i++)
	{
		cp[i] = outi[i]*outq[i+1]-outi[i+1]*outq[i];
	}*/
}
#endif
void dsp_crossproduct_s(short *outi,short *outq,int *cp,int inlen)
{
	for(int i=0;i<inlen-1;i++)
	{
		cp[i] = outi[i]*outq[i+1]-outi[i+1]*outq[i];
	}
}

// Sign of crossproduct. Input data is short. Shift not used.
__attribute__((optimize("unroll-loops")))
void dsp_crossproductsign_stream_in16(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr)
{
	for(int i=0;i<inlen;i++)
	{
		//int cp = (outi[i]>>sr)*(outq[i+1]>>sr)-(outi[i+1]>>sr)*(outq[i]>>sr);
		int cp = (outi[i])*(outq[i+1])-(outi[i+1])*(outq[i]);
		cps[i] = (cp>0)?1:0;
	}
}
__attribute__((optimize("unroll-loops")))
void dsp_crossproductsign_stream_in16_simd(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr)
{
	//uint32_t * restrict is = outi;
	//uint32_t * restrict qs = outq;

	for(int i=0;i<inlen;i++)
	{
		//uint32_t ii = is[i];
		//uint32_t qq = qs[i];
		uint32_t ii = *(uint32_t*)(&outi[i]);
		uint32_t qq = *(uint32_t*)(&outq[i]);

		int cp = __SMLSDX(ii,qq,0);

		cps[i] = (cp>0)?1:0;
	}
}
/******************************************************************************
function: dsp_crossproductsign_stream_in16_simd_2
*******************************************************************************

Calculate the sign of the crossproduct of two 16-bit streams (i, q).

Exactly inlen crossproducts are calcultated.

The input arrays (i,q) must have inlen+1 valid samples.
However, due to the SIMD implementation processing 4 samples at a time and accessing
data 2 samples at a time, the input arrays must have a length of inlen+2, with
the last sample accessed, but not effectively used.

The implementation unrolls access to memory and ensures only 32-bit aligned memory accesses.

Parameters:
	outi		-	Array of 16-bit samples, of length equal to inlen+2
	outq		-	Array of 16-bit samples, of length equal to inlen+2
	cps			-	Array holding the resulting cross-product sign of length inlen.
	inlen		-	Number of cross-products to compute. Must be multiple of 4.


Returns:
	-
******************************************************************************/
void dsp_crossproductsign_stream_in16_simd_2(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr)
{
	uint32_t * restrict is = (uint32_t*restrict)outi;
	uint32_t * restrict qs = (uint32_t*restrict)outq;

	uint32_t i1,i2,q1,q2;
	uint32_t i3,q3;

	// Example: data is 01 23 45 67
	// Load first words
	i1 = is[0];	// 01
	q1 = qs[0]; // 01

	// PKHTB: Combines bits[31:16] of Rn with bits[15:0] of the shifted value from Rm.
	unsigned j=0;
	// Loop produces 4 cross-products at a time.
	for(unsigned i=0;i<inlen;i+=4,j+=2)
	{
		//
		i2 = is[j+1]; // 23
		q2 = qs[j+1]; // 23

		// Cross product 01-10
		int cp = __SMLSDX(i1,q1,0);
		cps[i] = (cp>0)?1:0;
		//*cps++ = (cp>0)?1:0;

		// Cross product 12-21
		i3 = __PKHTB_DAN(i1,i2,0);	// 21
		q3 = __PKHTB_DAN(q1,q2,0);	// 21
		cp = __SMLSDX(q3,i3,0);		// cp 21-12. Swapped to sign swap and effectively do 12-21
		cps[i+1] = (cp>0)?1:0;
		//*cps++ = (cp>0)?1:0;



		// Cross product 23-32
		cp = __SMLSDX(i2,q2,0);
		cps[i+2] = (cp>0)?1:0;
		//*cps++ = (cp>0)?1:0;

		// Load next
		i1 = is[j+2]; // 45
		q1 = qs[j+2]; // 45

		// Cross product 34-43
		i3 = __PKHTB_DAN(i2,i1,0);	// 43
		q3 = __PKHTB_DAN(q2,q1,0);	// 43
		cp = __SMLSDX(q3,i3,0);		// cp 43-34. Swapped to sign swap and effectively do 34-43
		cps[i+3] = (cp>0)?1:0;
		//*cps++ = (cp>0)?1:0;
		//cps[j]=9;
	}
}
void dsp_crossproductsign_stream_in16_simd_3(short * restrict outi,short * restrict outq,unsigned char * restrict cps,int inlen,unsigned sr)
{
	uint32_t * restrict is = (uint32_t*restrict)outi;
	uint32_t * restrict qs = (uint32_t*restrict)outq;

	uint32_t i1,i2,q1,q2;
	uint32_t i3,q3;

	// Example: data is 01 23 45 67
	// Load first words
	i1 = is[0];	// 01
	//i1 = *is++;
	q1 = qs[0]; // 01
	//q1 = *qs++;


	// PKHTB: Combines bits[31:16] of Rn with bits[15:0] of the shifted value from Rm.
	unsigned j=0;
	// Loop produces 4 cross-products at a time.
	//for(unsigned i=0;i<inlen;i+=4,j+=2)
	for(unsigned j=0;j<inlen/2;j+=2)
	//for(unsigned i=0;i<inlen;i+=4)
	//for(unsigned i=inlen/4;i>0;i--)
	{
		//
		i2 = is[j+1]; // 23
		//i2 = *is++;
		q2 = qs[j+1]; // 23
		//q2 = *qs++;

		// Cross product 01-10
		int cp = __SMLSDX(i1,q1,0);
		//cps[i] = (cp>0)?1:0;
		*cps++ = (cp>0)?1:0;

		// Cross product 12-21
		i3 = __PKHTB_DAN(i1,i2,0);	// 21
		q3 = __PKHTB_DAN(q1,q2,0);	// 21
		cp = __SMLSDX(q3,i3,0);		// cp 21-12. Swapped to sign swap and effectively do 12-21
		//cps[i+1] = (cp>0)?1:0;
		*cps++ = (cp>0)?1:0;

		// Cross product 23-32
		cp = __SMLSDX(i2,q2,0);
		//cps[i+2] = (cp>0)?1:0;
		*cps++ = (cp>0)?1:0;

		// Load next
		i1 = is[j+2]; // 45
		//i1 = *is++;
		q1 = qs[j+2]; // 45
		//q1 = *qs++;


		// Cross product 34-43
		i3 = __PKHTB_DAN(i2,i1,0);	// 43
		q3 = __PKHTB_DAN(q2,q1,0);	// 43
		cp = __SMLSDX(q3,i3,0);		// cp 43-34. Swapped to sign swap and effectively do 34-43
		//cps[i+3] = (cp>0)?1:0;
		*cps++ = (cp>0)?1:0;

	}
}

/******************************************************************************
	function: dsp_mean
*******************************************************************************
	Computes the mean of 13-bit signed numbers, return a 16-bit signed.

	This function can be used to average ADC data which are 12-bit integer (13 signed).

	The input range (13-bit) is used to unroll and SIMD-optimise the addition.
	Internally the sum is computed on 16-bit and accumulated to a 32-bit accumulator
	before being shifted back to a 16 bit result.

	Data is processed 16 samples at a time, and data must be a multiple of 16.

******************************************************************************/
/*
// Calculate mean
	At 80MHz
		21366/s		Array, inc, no unroll
		20659/s		Pointer, inc, no unroll
		20659/s		Pointer, dec, no unroll
		21366		Array, inc, unroll
		20659		Pointer, inc, unroll
		20658		Pointer, dec, unroll

 */
__attribute__((optimize("unroll-loops")))
signed int dsp_mean(signed short* restrict data,unsigned len)
{
	int mean=0;		// Sum on 32-bits

	//for(unsigned i=0;i<CICBUFSIZ;i++)
	for(unsigned i=0;i<len;i++)
	//for(unsigned i=CICBUFSIZ;i!=0;i--)
		mean+=data[i];
		//mean+=*data++;

	//fprintf(file_pri,"Sum: %d\n",mean);
	mean/=len;

	return mean;
}

/******************************************************************************
	function: dsp_mean_simd_13s_16s
*******************************************************************************
	Computes the mean of 13-bit signed numbers, return a 16-bit signed.

	This function can be used to average ADC data which are 12-bit integer (13 signed).

	The input range (13-bit) is used to unroll and SIMD-optimise the addition.
	Internally the sum is computed on 16-bit and accumulated to a 32-bit accumulator
	before being shifted back to a 16 bit result.

	Data is processed 16 samples at a time, and data must be a multiple of 16.

******************************************************************************/
/*
	42358/s		Array, inc, no unroll
	42313		Array, dec, no unroll
	42358		Ptr, dec, no unroll
	42358		Array, inc, unroll
	42358		Ptr, dec, unroll

	44039
	49100
*/
//__attribute__((optimize("unroll-loops")))
signed int dsp_mean_simd_13s_16s(signed short* restrict data,unsigned len)
{

	signed int mean=0;									// 32-bit sum
	uint32_t *restrict datas=(uint32_t*restrict)data;	// SIMD data
	uint32_t oneone=0x00010001;

	// 2805/s initial
	// 2815/s with SMLAD of t1-t4
	// 2819/s with SMLAD of s
	// 2804/s with SMLAD of s and interleaving t5
	// 2824: SMALD interleaved; unroll, increment

	/*for(int i=0;i<len/2;i++)
	{
		fprintf(file_pri,"In: %d %d -> %08X\n",data[i*2],data[i*2+1],datas[i]);
	}*/

	// Sum 8 at a time: bit growth 13->16 bits
	//for(unsigned i=0;i<len;i+=8)
	for(unsigned i=0;i<len/2;i+=8)
	//for(signed i=len/2-8;i>=0;i-=8)
	{
		//ni++;
		// This implementation is slightly slower (44k/s)
		//uint32_t s = __SADD16(__SADD16(__SADD16(__SADD16(__SADD16(__SADD16(__SADD16(datas[i],datas[i+1]),datas[i+2]),datas[i+3]),datas[i+4]),datas[i+5]),datas[i+6]),datas[i+7]);

		// Slightly faster (49k/s)
		uint32_t t1 = __SADD16(datas[i],datas[i+1]);
		uint32_t t2 = __SADD16(datas[i+2],datas[i+3]);
		//uint32_t t5 = __SADD16(t1,t2);
		//mean=__SMLAD(t1,oneone,mean);
		//mean=__SMLAD(t2,oneone,mean);
		uint32_t t3 = __SADD16(datas[i+4],datas[i+5]);
		uint32_t t4 = __SADD16(datas[i+6],datas[i+7]);

		//mean=__SMLAD(t3,oneone,mean);
		//mean=__SMLAD(t4,oneone,mean);

		uint32_t t5 = __SADD16(t1,t2);
		uint32_t t6 = __SADD16(t3,t4);


		uint32_t s = __SADD16(t5,t6);

		/*mean=__SMLAD(t1,oneone,mean);
		mean=__SMLAD(t2,oneone,mean);
		mean=__SMLAD(t3,oneone,mean);
		mean=__SMLAD(t4,oneone,mean);*/

		// s is | sum1 | sum0 |
		// Need to extract the 16-bit parts and accumulate to 32-bit
		// __SMLAD: signed multiply with 32-bit accumulate


		// Accumulate sum into 32-bit sum
#if 0
		signed m1 = *((short*)&s);
		signed m2 = *(((short*)&s)+1);
		mean+=m1+m2;
#else
		mean=__SMLAD(s,oneone,mean);
#endif

		//fprintf(file_pri,"Sum of elements %d (%08X)->%d (%08X): %08X. Upper: %d. Lower: %d. Adding to sum: %d\n",i,datas[i],i+7,datas[i+7],s,m2,m1,m1+m2);

	}


	//fprintf(file_pri,"Sum: %d. num it: %d\n",mean, ni);
	mean/=len;		// Division is 2-12 clock cycles: negligible.

	return mean;
}
/******************************************************************************
	function: dsp_mean_simd_12u_16u
*******************************************************************************
	Computes the mean of 12-bit unsigned numbers, return a 32-bit unsigned.

	This function can be used to average ADC data which are 12-bit unsigned integer.

	The input range (12-bit) is used to unroll and SIMD-optimise the addition.
	Internally the sum is computed on 16-bit and accumulated to a 32-bit accumulator
	before being shifted back to a 16 bit result.

	Data is processed 32 samples at a time, and data must be a multiple of 32.

******************************************************************************/
signed int dsp_mean_simd_12u_16u(signed short* restrict data,unsigned len)
{

	signed int mean=0;									// 32-bit sum
	uint32_t *restrict datas=(uint32_t*restrict)data;	// SIMD data
	uint32_t oneone=0x00010001;

	// 2824/s

	// Sum 16 at a time: bit growth 12->16 bits
	for(unsigned i=0;i<len/2;i+=16)
	{

		uint32_t t1 = __UADD16(datas[i],datas[i+1]);
		uint32_t t2 = __UADD16(datas[i+2],datas[i+3]);
		uint32_t t5 = __UADD16(t1,t2);
		uint32_t t3 = __UADD16(datas[i+4],datas[i+5]);
		uint32_t t4 = __UADD16(datas[i+6],datas[i+7]);
		uint32_t t6 = __UADD16(t3,t4);

		uint32_t s1 = __UADD16(t5,t6);

		uint32_t t7 = __UADD16(datas[i+8],datas[i+9]);
		uint32_t t8 = __UADD16(datas[i+10],datas[i+11]);
		uint32_t t11 = __UADD16(t7,t8);
		uint32_t t9 = __UADD16(datas[i+12],datas[i+13]);
		uint32_t t10 = __UADD16(datas[i+14],datas[i+15]);
		uint32_t t12 = __UADD16(t9,t10);

		uint32_t s2 = __UADD16(t11,t12);

		uint32_t s = __UADD16(s1,s2);

		// Accumulate upper and lower half of s to mean

		unsigned m1 = *((unsigned short*)&s);
		unsigned m2 = *(((unsigned short*)&s)+1);
		mean+=m1+m2;

		//fprintf(file_pri,"Sum of elements %d (%08X)->%d (%08X): %08X. Upper: %d. Lower: %d. Adding to sum: %d\n",i,datas[i],i+7,datas[i+7],s,m2,m1,m1+m2);

	}


	//fprintf(file_pri,"Sum: %d. num it: %d\n",mean, ni);
	mean/=len;		// Division is 2-12 clock cycles: negligible.

	return mean;
}
/******************************************************************************
	function: dsp_meansub_inplace
*******************************************************************************
	In-place mean subtraction.

	At 80MHz:
		7301/s array, inc, no unroll
		6679/s pointer, dec, no unroll
		9141/s pointer, dec, unroll
		10352/s array, inc, unroll

******************************************************************************/
__attribute__((optimize("unroll-loops")))
signed int dsp_meansub_inplace(signed short* restrict data)
{
	// In-place mean subtraction
	signed int mean=dsp_mean(data,CICBUFSIZ);
	for(unsigned i=0;i<CICBUFSIZ;i++)
		data[i]-=mean;
	// Slower implementation
	//for(unsigned i=CICBUFSIZ;i!=0;i--)
		//*data++-=mean;

	return mean;
}
/******************************************************************************
	function: dsp_meansub_dsp_inplace
*******************************************************************************
	In-place mean subtraction.

	SIMD implementation for the mean computation and subtraction.

	The input data must be 13-bit signed max, as the DSP implementations exploits bit
	growth for optimisation.

	At 80MHz:
		10851/s		array, inc, no unroll, normal mean
		13069/s		array, inc, unroll, normal mean
		12935/s		ptr, dec, unroll, normal mean
		15296/s		array, inc, no unroll, SIMD mean
		19938/s		array, inc, unroll, SIMD mean

******************************************************************************/
// Same but with simd
__attribute__((optimize("unroll-loops")))
void dsp_meansub_simd_inplace(signed short* restrict data)
{
	// In-place mean subtraction
	//signed mean16=dsp_mean(data,CICBUFSIZ);
	signed mean16=dsp_mean_simd_13s_16s(data,CICBUFSIZ);

	uint32_t mean16_16 = simd_s16_to_s16s16(mean16);		// Convert to SIMD |mean|mean|

	//fprintf(file_pri,"Mean: %d (%08x). mean16_16: %d (%08x)\n",mean32,mean32,mean16_16,mean16_16);


	uint32_t * restrict ptr=(uint32_t* restrict)data;

	//for(unsigned i=CICBUFSIZ/2;i!=0;i--)
	for(unsigned i=0;i<CICBUFSIZ/2;i++)
	{
		//*ptr = __SADD16(*ptr, mean16_16);
		//ptr++;
		ptr[i] = __SADD16(ptr[i],mean16_16);
	}
}
/******************************************************************************
	function: dsp_std
*******************************************************************************
	Computes the standard deviation of data.

******************************************************************************/
signed int dsp_std(signed short* restrict data,unsigned len,int mean)
{
	int std=0;

	for(unsigned i=0;i<len;i++)
		std+=(data[i]-mean)*(data[i]-mean);

	std = sqrt(std/(len-1));

	return std;
}
/******************************************************************************
	function: dsp_min
*******************************************************************************
	Computes the minimum of the data

******************************************************************************/
signed int dsp_min(signed short* restrict data,unsigned len)
{
	int min=0;

	min=data[0];

	for(unsigned i=1;i<len;i++)
	{
		if(data[i]<min)
			min=data[i];
	}
	return min;
}
/******************************************************************************
	function: dsp_max
*******************************************************************************
	Computes the maximum of the data

******************************************************************************/
signed int dsp_max(signed short* restrict data,unsigned len)
{
	int max=0;

	max=data[0];

	for(unsigned i=1;i<len;i++)
	{
		if(data[i]>max)
			max=data[i];
	}
	return max;
}
uint32_t simd_s16_to_s16s16(signed short s)
{
	unsigned int s32=*(unsigned short*)(&s);		// Map to 32-bit preventing sign extension
	uint32_t s16_16 = (s32<<16)|s32;
	return s16_16;
}

uint32_t add_halfwords(uint32_t val1, uint32_t val2)
{
  return __SADD16(val1, val2);
}


// Add a cosine wave to a buffer, zero centered.
void dsp_addcoswave(signed short *target,int len,int sr,int f,float amp,float phase)
{
	float dt = 1.0/sr;
	float pi = 3.14159265358979323846;
	float w = 2*pi*f;
	// y = cos( w * dt * i ) with dt time in-between samples, i index of sample. I.e. phase increment is equal to w*dt
	for(unsigned i=0;i<len;i++)
	{
		int t = cos(w*dt*i + phase)*amp;
		target[i] = target[i] + t;
	}
}
