/*
 * mode_benchmarkcpu.c
 *
 *  Created on: 7 déc. 2019
 *      Author: droggen
 */

#include <mode_benchmarks.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "usrmain.h"
#include "global.h"
#include "serial.h"
#include "wait.h"
//#include "init.h"
#include "uiconfig.h"
#include "helper.h"
#include "system.h"

#include "helper_num.h"
#include "commandset.h"
#include "mode_global.h"
#include "usrmain.h"
#include "main.h"
#include "mode.h"
#include "mode_main.h"
#include "benchmark/noprompt/dhry.h"
#include "benchmark/whetstone.h"


const char help_benchmarkcpu1[] = "Dhrystone benchmark";
const char help_benchmarkcpu2[] = "Whetstone benchmark";
const char help_benchusb[] ="Benchmark USB write";
const char help_benchbt[] ="Benchmark BT write";
const char help_benchitm[] ="Benchmark ITM write";


const COMMANDPARSER CommandParsersBenchmarks[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'1', CommandParserBenchmarkCPU1,help_benchmarkcpu1},
	{'2', CommandParserBenchmarkCPU2,help_benchmarkcpu2},
	{'M', CommandParserBenchmarkCPUMem,"Memory transfer"},
	{'U', CommandParserBenchUSB,help_benchusb},
	{'B', CommandParserBenchBT,help_benchbt},
	{'I', CommandParserBenchITM,help_benchitm},
};
unsigned char CommandParsersBenchmarksNum=sizeof(CommandParsersBenchmarks)/sizeof(COMMANDPARSER);



void mode_benchmarkcpu(void)
{
	mode_run("BENCH",CommandParsersBenchmarks,CommandParsersBenchmarksNum);
}

unsigned char CommandParserBenchmarkCPU1(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	unsigned dps = dhry_main();

	fprintf(file_pri,"Benchmark done: %u dhrystones/sec\n",dps);

	fprintf(file_pri,"References: Dhrystones MHz\n");
	fprintf(file_pri,"\t 109749 80\n");
	fprintf(file_pri,"\t 84398 60\n");
	fprintf(file_pri,"\t 57909 40\n");
	fprintf(file_pri,"\t 50608 35\n");
	fprintf(file_pri,"\t 49166 34\n");
	fprintf(file_pri,"\t 47761 33\n");
	fprintf(file_pri,"\t 47700 32\n");
	fprintf(file_pri,"\t 46199 31\n");
	fprintf(file_pri,"\t 44738 30\n");
	fprintf(file_pri,"\t 43222 29\n");
	fprintf(file_pri,"\t 37249 25\n");
	fprintf(file_pri,"\t 29731 20\n");
	fprintf(file_pri,"\t 28235 19\n");
	fprintf(file_pri,"\t 26736 18\n");
	fprintf(file_pri,"\t 25249 17\n");
	fprintf(file_pri,"\t 24515 16\n");
	fprintf(file_pri,"\t 22982 15\n");


	return 0;
}
unsigned char CommandParserBenchmarkCPU2(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;


	fprintf(file_pri,"Whetstone benchmark\n");

	unsigned wps = whetstone(0,100,file_pri);

	fprintf(file_pri,"Whetstone: %d KIPS\n",wps);







	return 0;
}

unsigned char CommandParserBenchmarkCPUMem(char *buffer,unsigned char size)
{
	(void)size; (void)buffer;

	unsigned nk = 8;
	unsigned s = 1024*nk;
	volatile unsigned char b1[s],b2[s];

	fprintf(file_pri,"Memory transfer benchmark. Press return to stop.\n");

	while(1)
	{
		unsigned long t1 = timer_us_get();
		memcpy((void*)b1,(void*)b2,s);
		unsigned long t2 = timer_us_get();

		unsigned long dt = t2-t1;

		unsigned long long mbps = (unsigned long long)s*1000000/dt;
		mbps>>=10;

		fprintf(file_pri,"Copy %u bytes: %lu us. KB/s: %u\n",s,dt,(unsigned)mbps);

		_delay_ms(100);

		if(fgetc(file_pri)!=-1)
			break;
	}


	return 0;
}

unsigned char CommandParserBenchUSB(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;
	benchmark_interface(file_usb,file_pri);
	return 0;
}
unsigned char CommandParserBenchBT(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;
	fprintf(file_pri,"UART events before:\n");
	fprintf(file_usb,"UART events before:\n");
	serial_uart_printevents(file_pri);
	serial_uart_printevents(file_usb);
	benchmark_interface(file_bt,file_pri);
	fprintf(file_pri,"UART events after:\n");
	fprintf(file_usb,"UART events after:\n");
	serial_uart_printevents(file_pri);
	serial_uart_printevents(file_usb);
	return 0;
}
unsigned char CommandParserBenchITM(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;
	benchmark_interface(file_itm,file_pri);
	return 0;
}



/******************************************************************************
	function: benchmark_interface
*******************************************************************************
	Benchmark an interface fbench; prints additional info to finfo

	Parameters:
		fbench				-		Interface to benchmark
		findo				-		Interface where to print information (benchmark results)

	Returns:
		-
******************************************************************************/
void benchmark_interface(FILE *fbench,FILE *finfo)
{	unsigned long int t_last,t_cur;
	unsigned long tint1,tint2;
	unsigned payloadsize = 256;		// Payload size
	unsigned benchtime = 2;			// benchmark duration in seconds
	char s[payloadsize+1];			// Payload



	// Fill buffer with data. Last character is \n.
	for(unsigned i=0;i<payloadsize-1;i++)
		s[i] = '0'+i%10;
	s[payloadsize-1] = '\n';
	s[payloadsize] = 0;


	fprintf(finfo,"Benchmarking\n");

	// Benchmark fwrite, fputs, fputc


	// Transfer 128KB
	unsigned int size = 128*1024l;
	//unsigned int size = 16*1024l;
	unsigned int nwritten;
	unsigned int it = size/256;
	unsigned int t1,t2;
	unsigned int bps;

	fprintf(finfo,"Benchmarking fwrite for %u seconds.\n",benchtime);

	nwritten = 0;
	t_last=timer_s_wait(); tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<benchtime)
	{
		int nw = fwrite(s,1,payloadsize,fbench);				// payload elements of size 1
		nwritten+=nw;										// Number of elements successfully written
	}
	tint2=timer_ms_get_intclk();

	fprintf(finfo,"Time: %lu ms. ",tint2-tint1);
	fprintf(finfo,"Bytes written: %lu. ",nwritten);
	fprintf(finfo,"Bandwidth: %lu bps. \n",nwritten*1000/(tint2-tint1));

	return;
#if 0
	nwritten=0;
	t1 = HAL_GetTick();
	for(unsigned i=0;i<it;i++)
	{
		int nw = fwrite(s,1,256,fbench);
		nwritten+=nw;

		fprintf(finfo,"nw: %d\n",nw);

		if((i&0xf)==0)
			fputc('.',finfo);
	}
	t2 = HAL_GetTick();
	bps = size*1000/(t2-t1);
	fprintf(file_pri,"Transfer of %u bytes. Effective: %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);
	fprintf(finfo,"Transfer of %u bytes. Effective: %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);

	HAL_Delay(1000);

	fprintf(file_pri,"Benchmarking fputs. Tot: %u.\n",size);
	fprintf(finfo,"Benchmarking fputs. Tot: %u.\n",size);
	nwritten=0;
	t1 = HAL_GetTick();
	for(unsigned i=0;i<it;i++)
	{
		int nw = fputs(s,fbench);
		if(nw!=-1)
			nwritten++;

		fprintf(finfo,"nw: %d\n",nw);

		if((i&0xf)==0)
			fputc('.',finfo);
	}
	t2 = HAL_GetTick();
	bps = size*1000/(t2-t1);
	fprintf(file_pri,"Transfer of %u bytes. Effective (blocks of 256): %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);
	fprintf(finfo,"Transfer of %u bytes. Effective (blocks of 256): %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);


	HAL_Delay(1000);

	fprintf(file_pri,"Benchmarking fputbuf. Tot: %u.\n",size);
	fprintf(finfo,"Benchmarking fputbuf. Tot: %u.\n",size);*/
#endif
/*	HAL_Delay(1000);

	fprintf(file_pri,"Benchmarking fputc. Tot: %u.\n",size);
	fprintf(finfo,"Benchmarking fputc. Tot: %u.\n",size);

	nwritten=0;
	t1 = HAL_GetTick();
	for(unsigned i=0;i<it;i++)
	{
		for(unsigned j=0;j<256;j++)
		{
			int nw = fputc(s[j],fbench);
			if(nw!=-1)
				nwritten++;
		}

		if((i&0xf)==0)
			fputc('.',finfo);
	}
	t2 = HAL_GetTick();
	bps = size*1000/(t2-t1);
	fprintf(file_pri,"fputc transfer of %u bytes. Effective: %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);
	fprintf(finfo,"fputc transfer of %u bytes. Effective: %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);
	*/
	/*

	HAL_Delay(1000);

	nwritten=0;
	t1 = HAL_GetTick();
	while(nwritten<size)
	{
		//if(!fputbuf(fbench,s,256))
		//if(!fputbuf(fbench,s,16))
		if(!fputbuf(fbench,s,1))
		{
			//nwritten+=256;
			//nwritten+=16;
			nwritten+=1;
		}

		//if((i&0xf)==0)
			//fputc('.',finfo);
	}
	t2 = HAL_GetTick();
	bps = size*1000/(t2-t1);
	fprintf(file_pri,"fputbuf transfer of %u bytes. Effective: %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);
	fprintf(finfo,"fputbuf transfer of %u bytes. Effective: %u. Time: %u ms. Bandwidth: %u byte/s\n",size,nwritten,t2-t1,bps);
	*/


	fprintf(finfo,"Benchmark end\n");


}

/*void mode_bench(void)
{
	fprintf_P(file_pri,PSTR("bench mode\n"));
	char s[256];
	int c;

	for(unsigned i=0;i<255;i++)
		s[i] = '0'+i%10;
	s[255] = '\n';



	while(1)
	{
		while(CommandProcess(CommandParsersBench,CommandParsersBenchNum));
		if(CommandShouldQuit())
			break;

		FILE *f;
		if(mode_bench_io==0)
			f=file_usb;
		else
			f=file_bt;

		// Transfer 128KB
		unsigned long size = 128*1024l;
		unsigned it = size/256;
		unsigned long t1,t2;
		printf("size: %ld\n",size);
		printf("it: %d\n",it);
		t1 = timer_ms_get();
		for(unsigned i=0;i<it;i++)
		{
			fwrite(s,256,1,f);
			if((i&0xf)==0)
				fputc('.',file_usb);

			// Get feedback if any
			while((c=fgetc(file_bt))!=-1)
				fputc(c,file_usb);
		}
		t2 = timer_ms_get();
		unsigned long bps = size*1000/(t2-t1);
		fprintf_P(file_pri,PSTR("Transfer of %lu bytes in %lu ms. Bandwidth: %lu byte/s\n"),size,t2-t1,bps);
		fprintf_P(file_dbg,PSTR("Transfer of %lu bytes in %lu ms. Bandwidth: %lu byte/s\n"),size,t2-t1,bps);

		_delay_ms(5000);


	}
	fprintf_P(file_pri,PSTR("bench end\n"));

	return 0;
}
*/
