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
#include "atomicop.h"

#include "helper_num.h"
#include "commandset.h"
#include "mode_global.h"
#include "usrmain.h"
#include "serial_uart.h"
#include "serial_usb.h"
#include "main.h"
#include "mode.h"
#include "mode_main.h"
#include "benchmark/noprompt/dhry.h"
#include "benchmark/whetstone.h"
#include "power.h"

const char help_benchmarkcpu1[] = "Dhrystone benchmark";
const char help_benchmarkcpu2[] = "Whetstone benchmark";
const char help_benchusb[] ="Benchmark USB write";
const char help_benchbt[] ="Benchmark BT write";
const char help_benchitm[] ="Benchmark ITM write";


const char help_intf_writebench[] ="U,intf,ns,m Test write speed during ns seconds on intf (0=USB, 1=bluetooth) with method m (0=fputs, 1=putfbuf)";
const char help_intf_buf[] ="B,intf,buf,type sets the buffer size to buf for intf (0=USB, 1=bluetooth). buf=0 is no buffering, otherwise line buffered";
const char help_intf_status[]="Show interface status until keypress";
const char help_intf_events[]="Show UART event counters";

const COMMANDPARSER CommandParsersBenchmarks[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{0,0,"---- CPU benchmarks ----"},
	{'1', CommandParserBenchmarkCPU1,help_benchmarkcpu1},
	{'2', CommandParserBenchmarkCPU2,help_benchmarkcpu2},
	{'M', CommandParserBenchmarkCPUMem,"Memory transfer"},
	{'P',CommandParserBenchmarkPerf,"Perf bench"},
	{0,0,"---- Interface benchmarks ----"},
	{'U', CommandParserBenchUSB,help_benchusb},
	{'B', CommandParserBenchBT,help_benchbt},
	{'I', CommandParserBenchITM,help_benchitm},
	{'W',CommandParserIntfWriteBench,"W,intf,ns,m Test write speed during ns seconds on intf (0=USB, 1=bluetooth) with method m (0=fputs, 1=putfbuf)"},
	{'b',CommandParserIntfBuf,"B,intf,buf,type sets the buffer size to buf for intf (0=USB, 1=bluetooth). buf=0 is no buffering, otherwise line buffered"},
	{'S',CommandParserIntfStatus,"Show interface status until keypress"},
	{'E',CommandParserIntfEvents,"Show UART event counters"},
	{'C',CommandParserIntfEventsClear,"Clear UART event counters"},





	// change the vbuf?
	{0,0,"---- Power ----"},
	{'p',CommandParserBenchmarkPower,"Power consumption"},
	{0,0,"---- various benchmarks ----"},
	{'f',CommandParserFlush,"Flush"},
	{'d',CommandParserDump,"Dump to itm"},
	{'i',CommandParserInt,"Interrupt tests"},
	{'u', CommandParserUSBDirect,"USB Direct write"},

	{'X',CommandParserBenchmarkFlush,"Benchmark fflush"},
	{'x',CommandParserBenchmarkLatency,"Benchmark write to display latency"},
	{'t',CommandParserBenchmarkTimer,"Timer"},
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
	fprintf(file_bt,"UART events before:\n");
	fprintf(file_usb,"UART events before:\n");
	serial_uart_printevents(file_bt);
	serial_uart_printevents(file_usb);
	benchmark_interface(file_bt,file_pri);
	fprintf(file_bt,"UART events after:\n");
	fprintf(file_usb,"UART events after:\n");
	serial_uart_printevents(file_bt);
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
	unsigned nit=0;
	unsigned payloadsize = 256;		// Payload size
	unsigned benchtime = 2;			// benchmark duration in seconds
	char s[payloadsize+1];			// Payload

	char *name[4] = {"fprintf","fwrite","fputs","fputbuf"};
	unsigned result_dts[4];
	unsigned result_dt[4];
	unsigned result_nwritten[4];
	unsigned result_bps[4];
	unsigned result_nit[4];


	// Fill buffer with data. Last character is \n.
	for(unsigned i=0;i<payloadsize-1;i++)
		s[i] = '0'+i%10;
	s[payloadsize-1] = '\n';
	s[payloadsize] = 0;


	fprintf(finfo,"Benchmarking\n");

	// Enable blocking write
	fprintf(finfo,"Enabling blocking writes\n");
	//serial_setblockingwrite(fbench,1);
	serial_setblockingwrite(file_usb,1);
	serial_setblockingwrite(file_bt,1);

	// Benchmark


	unsigned int nwritten;



	fprintf(finfo,"Benchmarking fprintf for %u seconds.\n",benchtime);

	nwritten = 0;
	nit=0;
	t_last=timer_s_wait(); tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<benchtime)
	{
		s[0] = '0' + (nit&0xf);
		fprintf(fbench,"%s",s);
		//fprintf(file_pri,"%d\n",nw);
		//if(nw>0)
		nwritten += payloadsize;		// normally fprintf should return the number of bytes written; here returns -1 but is blocking and successful
		nit++;
	}
	tint2=timer_ms_get_intclk();

	result_dts[0] = (t_cur-t_last)*1000;
	result_dt[0] = tint2-tint1;
	result_nwritten[0] = nwritten;
	//result_bps[0] = nwritten*1000/result_dts[0];
	result_bps[0] = nwritten*125/128/result_dts[0];
	result_nit[0] = nit;
	HAL_Delay(500);


	// fwrite

	fprintf(finfo,"Benchmarking fwrite for %u seconds.\n",benchtime);

	nwritten = 0;
	nit=0;
	t_last=timer_s_wait(); tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<benchtime)
	{
		s[0] = '0' + (nit&0xf);
		int nw = fwrite(s,1,payloadsize,fbench);				// payload elements of size 1
		nwritten+=nw;										// Number of elements successfully written
		nit++;
	}
	tint2=timer_ms_get_intclk();

	result_dt[1] = tint2-tint1;
	result_dts[1] = (t_cur-t_last)*1000;
	result_nwritten[1] = nwritten;
	//result_bps[1] = nwritten*1000/result_dts[1];
	result_bps[1] = nwritten*125/128/result_dts[1];
	result_nit[1] = nit;
	HAL_Delay(500);

	// fputs

	fprintf(finfo,"Benchmarking fputs for %u seconds.\n",benchtime);

	nwritten = 0;
	nit=0;
	t_last=timer_s_wait(); tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<benchtime)
	{
		s[0] = '0' + (nit&0xf);
		int nw = fputs(s,fbench);
		if(nw>=0)
		nwritten+=payloadsize;								// Number of elements successfully written
		nit++;
	}
	tint2=timer_ms_get_intclk();

	result_dt[2] = tint2-tint1;
	result_dts[2] = (t_cur-t_last)*1000;
	result_nwritten[2] = nwritten;
	//result_bps[2] = nwritten*1000/result_dts[2];
	result_bps[2] = nwritten*125/128/result_dts[2];
	result_nit[2] = nit;
	HAL_Delay(500);


	// fputbuf

	fprintf(finfo,"Benchmarking fputbuf for %u seconds.\n",benchtime);

	nwritten = 0;
	nit=0;
	t_last=timer_s_wait(); tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<benchtime)
	{
		s[0] = '0' + (nit&0xf);
		if(fputbuf(fbench,s,payloadsize)==0)
			nwritten += payloadsize;		// Success
		nit++;
	}
	tint2=timer_ms_get_intclk();

	result_dt[3] = tint2-tint1;
	result_dts[3] = (t_cur-t_last)*1000;
	result_nwritten[3] = nwritten;
	//result_bps[3] = nwritten*1000/result_dts[3];		// Use second timer, as millisecond may loose interrupts under heavy load
	result_bps[3] = nwritten*125/128/result_dts[3];		// Use second timer, as millisecond may loose interrupts under heavy load
	result_nit[3] = nit;
	HAL_Delay(500);


	// Disable blocking write
	fprintf(finfo,"Disabling blocking writes\n");
	//serial_setblockingwrite(fbench,0);
	serial_setblockingwrite(file_usb,0);
	serial_setblockingwrite(file_bt,0);

	for(int i=0;i<4;i++)
	{
		fprintf(finfo,"Benchmark of %s\n",name[i]);
		fprintf(finfo,"\tTime: %u ms. (with ms int)\n",result_dt[i]);
		fprintf(finfo,"\tTime: %u ms. (with sec. int)\n",result_dts[i]);
		fprintf(finfo,"\tIterations: %u.\n",result_nit[i]);
		fprintf(finfo,"\tBytes written: %u.\n",result_nwritten[i]);
		fprintf(finfo,"\tBandwidth: %u KB/s.\n",result_bps[i]);
	}





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

/******************************************************************************
	function: CommandParserUSBWriteBench
*******************************************************************************
	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid

******************************************************************************/
unsigned char CommandParserIntfWriteBench(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	FILE *fb = file_usb;
	int n=256;
	int ns,intf,method;
	char buf[n+1];

	if(ParseCommaGetInt(buffer,3,&intf,&ns,&method))
		return 2;
	if(intf!=0 && intf!=1)
	{
		fprintf(file_pri,"Invalid interface\n");
		return 2;
	}
	if(method!=0 && method!=1)
	{
		fprintf(file_pri,"Invalid method\n");
		return 2;
	}
	if(ns<0 || ns > 100)
	{
		fprintf(file_pri,"Invalid duration\n");
		return 0;
	}

	fprintf(file_pri,"Benchmark write on interface %s for %d seconds with method %s\n",intf?"BT":"USB",ns,method?"fputbuf":"fputs");

	// Select interface
	FILE *fi;
	unsigned txbufsize;		// Hold the size of the TX buffer of the interface
	if(intf==0)
	{
		fi = file_usb;
		txbufsize = USB_BUFFERSIZE;
	}
	else
	{
		fi = file_bt;
		txbufsize = SERIAL_UART_TX_BUFFERSIZE;
	}

	for(int i=0;i<n;i++)
	{
		buf[i] = 'A'+(i%26);
	}
	buf[n-1]='\n';			// Carriage return as some terminals have issues with very long lines
	buf[n]=0;

	// Enable blocking write
	fprintf(file_pri,"Enabling blocking writes\n");
	//serial_setblockingwrite(fi,1);
	serial_setblockingwrite(file_usb,1);
	serial_setblockingwrite(file_bt,1);


	unsigned wr=0;

	unsigned t1,t2,ts1,ts2,dt;
	unsigned it=0;
	switch(method)
	{
		case 0:
		{
			ts1 = timer_s_wait();
			t1 = timer_ms_get();
			do
			{
				buf[0]='0'+(it&0xf);			// Counter in 1st byte
				if(fputs(buf,fi)>=0)
					wr++;
				it++;
			}
			while( (ts2 = timer_s_get())-ts1 < ns);
			t2 = timer_ms_get();
			break;
		}
		case 1:
		default:
		{
			ts1 = timer_s_wait();
			t1 = timer_ms_get();
			do
			{
				buf[0]='0'+(it&0xf);			// Counter in 1st byte
				if(fputbuf(fi,buf,n)==0)
					wr++;
				it++;
			}
			while( (ts2 = timer_s_get())-ts1 < ns);
			t2 = timer_ms_get();
			break;
		}

	}
	dt = (ts2-ts1)*1000;
	// Wait for the buffers to empty, as fprintf is non-blocking
	//HAL_Delay(500);
	for(int i=0;i<1;i++)
	{
		HAL_Delay(500);
		fprintf(file_usb,"Completed\n");
	}
	// Disable blocking write
	fprintf(file_pri,"Disabling blocking writes\n");
	//serial_setblockingwrite(fi,0);
	serial_setblockingwrite(file_usb,0);
	serial_setblockingwrite(file_bt,0);


	fprintf(file_pri,"\n");
	fprintf(file_pri,"Sent %u buffers of %d (tot: %d bytes). Successfully wrote %d buffers (tot %d bytes). Time: %d ms (with sec. int) %d (with ms int)\n",it,n,it*n,wr,wr*n,dt,t2-t1);
	fprintf(file_pri,"Speed: %u KB/s\n",wr*n*1000/1024/dt);
	// Account for the TX buffer size, which likely isn't empty
	fprintf(file_pri,"Accounting for TX buffer size (%d) wrote: %d\n",txbufsize,wr*n-txbufsize);
	fprintf(file_pri,"Speed: %u KB/s\n",(wr*n-txbufsize)*125/128/dt);



	return 0;
}
unsigned char CommandParserIntfBuf(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	int intf,bufs;
	if(ParseCommaGetInt(buffer,2,&intf,&bufs))
		return 2;
	if(intf!=0 && intf!=1)
	{
		fprintf(file_pri,"Invalid interface\n");
		return 2;
	}
	if(bufs<0 || bufs>256)
	{
		fprintf(file_pri,"Invalid buffer size\n");
		return 2;
	}
	fprintf(file_pri,"Setting buffer for %s to %d\n",intf?"BT":"USB",bufs);
	FILE *fi;
	if(intf==0)
		fi = file_usb;
	else
		fi = file_bt;
	int rv;
	if(bufs==0)
		rv = setvbuf(fi,0,_IONBF,0);
	else
		rv = setvbuf(fi,0,_IOLBF,bufs);
	if(rv)
	{
		fprintf(file_pri,"Error\n");
		return 1;
	}
	fprintf(file_pri,"Success\n");
	return 0;
}
unsigned char CommandParserIntfStatus(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	while(1)
	{
		fprintf(file_pri,"USB: %d BT: %d\n",system_isusbconnected(),system_isbtconnected());

		if(fgetc(file_pri)!=-1)
			break;

		HAL_Delay(100);

	}
	return 0;
}
unsigned char CommandParserIntfEvents(char *buffer,unsigned char size)
{
	serial_uart_printevents(file_pri);
	return 0;
}
unsigned char CommandParserIntfEventsClear(char *buffer,unsigned char size)
{
	serial_uart_clearevents();
	return 0;
}

unsigned char CommandParserBenchmarkPower(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;





	/*
	fprintf(file_pri,"Running until keypress\n");
	unsigned t1 = timer_ms_get();
	unsigned c=0;
	while(1)
	{
		c++;
		if(fgetc(file_pri)!=-1)
			break;
	}
	unsigned t2 = timer_ms_get();
	fprintf(file_pri,"Count %u in %u ms (cps=%u)\n",c,t2-t1,c*1000/(t2-t1));*/

	static signed int pwr;
	static unsigned long timems;
	fprintf(file_pri,"Data of previous measurement: time %ld ms Power: %d mW\n",timems,pwr);
	power_measure_start();
	for(int i=0;i<60;i++)
	{
		fprintf(file_pri,"Power test: %d\n",i);
		HAL_Delay(1000);
	}

	pwr = power_measure_stop(&timems);
	fprintf(file_pri,"Time elapsed: %ld Power: %d mW\n",timems,pwr);



	return 0;
}
unsigned char CommandParserFlush(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	int rdptr1,wrptr1,lvl1;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		rdptr1=SERIALPARAM_USB.txbuf.rdptr;
		wrptr1=SERIALPARAM_USB.txbuf.wrptr;
		lvl1=buffer_level(&SERIALPARAM_USB.txbuf);
	}


	fflush(file_pri);

	HAL_Delay(100);

	int rdptr2,wrptr2,lvl2;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		rdptr2=SERIALPARAM_USB.txbuf.rdptr;
		wrptr2=SERIALPARAM_USB.txbuf.wrptr;
		lvl2=buffer_level(&SERIALPARAM_USB.txbuf);
	}

	fprintf(file_pri,"Before flush: %d %d %d\n",rdptr1,wrptr1,lvl1);
	fprintf(file_pri,"After flush: %d %d %d\n",rdptr2,wrptr2,lvl2);
	fprintf(file_pri,"\n");


	return 0;
}
extern volatile int usbnwrite;
extern volatile unsigned char usbdatalast[2048];
volatile unsigned usblasttx;
unsigned char CommandParserDump(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	itmprintf("\n-----------Going to dump\n");
	for(int i=0;i<3;i++)
	{
		itmprintf("Wait %d\n",i);
		HAL_Delay(500);
	}

	int rdptr,wrptr,lvl;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		rdptr=SERIALPARAM_USB.txbuf.rdptr;
		wrptr=SERIALPARAM_USB.txbuf.wrptr;
		lvl=buffer_level(&SERIALPARAM_USB.txbuf);
	}

	itmprintf("rdptr %d wrptr %d lvl %d\n",rdptr,wrptr,lvl);
	for(unsigned i=0;i<USB_BUFFERSIZE;i++)
	{
		itmprintf("Buf %d: %d (%c)\n",i,USB_TX_DataBuffer[i],USB_TX_DataBuffer[i]);
		HAL_Delay(1);
	}

	itmprintf("\n--------Printing starting at rdptr %d\n",rdptr);
	unsigned i,j;
	for(i=0,j=rdptr;i<USB_BUFFERSIZE;i++)
	{
		itmprintf("%c",USB_TX_DataBuffer[j]);
		j=(j+1)&(USB_BUFFERSIZE-1);
		HAL_Delay(1);
	}
/*	itmprintf("\n--------Last USB write\n");
	itmprintf("nwrite: %d\n",usbnwrite);
	itmprintf("usblasttx: %d\n",usblasttx);

	for(int i=0;i<usbnwrite;i++)
	{
		itmprintf("usb buf %d: %d (%c)\n",i,usbdatalast[i],usbdatalast[i]);
		HAL_Delay(1);
	}*/
	itmprintf("\n-----------END\n");
	for(int i=0;i<3;i++)
	{
		itmprintf("Wait %d\n",i);
		HAL_Delay(500);
	}
	itmprintf("\n");
	return 0;
}
unsigned char CommandParserAtomic(char *buffer,unsigned char size)
{
//	itmprintf("Printing starting at rdptr %d\n",rdptr);
	return 0;
}

unsigned char CommandParserInt(char *buffer,unsigned char size)
{
	unsigned i0,i1,i2,i3,i4,i5,i6;

	i0=__get_PRIMASK();

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		i1=__get_PRIMASK();
		__disable_irq();
		i2=__get_PRIMASK();
		__enable_irq();
		i3=__get_PRIMASK();
		__disable_irq();
		i4=__get_PRIMASK();
	}
	i5=__get_PRIMASK();

	HAL_Delay(100);
	fprintf(file_pri,"%08X %08X %08X %08X %08X %08X\n",i0,i1,i2,i3,i4,i5);
	HAL_Delay(100);

	i0=__get_PRIMASK();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		i1=__get_PRIMASK();
		__disable_irq();
		i2=__get_PRIMASK();
		__enable_irq();
		i3=__get_PRIMASK();
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			i4=__get_PRIMASK();
		}
		i5=__get_PRIMASK();

	}
	i6=__get_PRIMASK();

	HAL_Delay(100);
	fprintf(file_pri,"%08X %08X %08X %08X %08X %08X %08X\n",i0,i1,i2,i3,i4,i5,i6);
	HAL_Delay(100);

	i0=__get_PRIMASK();
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		i1=__get_PRIMASK();
		__disable_irq();
		i2=__get_PRIMASK();
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			i3=__get_PRIMASK();
			__enable_irq();
			i4=__get_PRIMASK();
		}
		i5=__get_PRIMASK();

	}
	i6=__get_PRIMASK();

	HAL_Delay(100);
	fprintf(file_pri,"%08X %08X %08X %08X %08X %08X %08X\n",i0,i1,i2,i3,i4,i5,i6);
	HAL_Delay(100);

	return 0;
}
unsigned char CommandParserSerInfo(char *buffer,unsigned char size)
{
	itmprintf("Buffer level: %d\n",buffer_level(&SERIALPARAM_USB.txbuf));
	return 0;
}


unsigned char CommandParserUSBDirect(char *buffer,unsigned char size)
{
	return 0;
}

unsigned char CommandParserBenchmarkFlush(char *buffer,unsigned char size)
{
	unsigned n=10000;
	unsigned long t1,t2;
	char buf[32];
	memset(buf,'@',32);


	fprintf(file_pri,"Benchmarking fflush(file_pri) without data in buffer\n");


	t1=timer_ms_get();
	for(unsigned i=0;i<n;i++)
		fflush(file_pri);
	t2=timer_ms_get();
	fprintf(file_pri,"Time: %ld ms for %u iterations\n",t2-t1,n);
	fprintf(file_pri,"Time per call: %ld us\n",(t2-t1)*1000L/n);

	fprintf(file_pri,"Benchmarking fflush(file_pri) with fwrite of 32-byte (silent writes)\n");
	_serial_usb_enable_write(0);

	t1=timer_ms_get();
	for(unsigned i=0;i<n;i++)
	{
		fwrite(buf,32,1,file_pri);
		fflush(file_pri);
	}
	t2=timer_ms_get();

	_serial_usb_enable_write(1);
	fprintf(file_pri,"Time: %ld ms for %u iterations\n",t2-t1,n);
	fprintf(file_pri,"Time per call: %ld us\n",(t2-t1)*1000L/n);



	return 0;
}


unsigned char CommandParserBenchmarkLatency(char *buffer,unsigned char size)
{
	fprintf(file_pri,"Testing writing latency when writing text not immediately flushed\n");
	for(int i=0;i<5;i++)
	{
		fprintf(file_pri,"Writing string\n");
		//HAL_Delay(1000);
		fprintf(file_pri,"<test string %d>",i);
		HAL_Delay(2000);
		fprintf(file_pri,"Write done\n\n");
	}

}

unsigned char CommandParserBenchmarkPerf(char *buffer,unsigned char size)
{
	unsigned long refperf = system_perfbench(2);
	fprintf(file_pri,"Reference performance: %lu\n",refperf);
	return 0;
}
unsigned char CommandParserBenchmarkTimer(char *buffer,unsigned char size)
{
	for(int i=0;i<10;i++)
	{
		fprintf(file_pri,"HAL_GetTick: %d\n",HAL_GetTick());
		HAL_Delay(50);
	}
	return 0;
}
