/*
 * mode_eeprom.c
 *
 *  Created on: 16 Sep 2019
 *      Author: droggen
 */

#include "global.h"
#include "command.h"
#include "commandset.h"
#include "main.h"
#include "mode_eeprom.h"
#include "m24xx.h"
#include "helper.h"
#include "mode_interface.h"
#include "wait.h"

//const char help_eepromread[] ="R[,<addrfrom>[,addrto]]: Read all the EEPROM, or at/from <addrfrom> until <addrto>";
const char help_intf_writebench[] ="U,intf,ns,m Test write speed during ns seconds on intf (0=USB, 1=bluetooth) with method m (0=fputs, 1=putfbuf)";
const char help_intf_buf[] ="B,intf,buf,type sets the buffer size to buf for intf (0=USB, 1=bluetooth). buf=0 is no buffering, otherwise line buffered";
const char help_intf_status[]="Show interface status until keypress";
const char help_intf_events[]="Show UART event counters";

const COMMANDPARSER CommandParsersInterface[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'W',CommandParserIntfWriteBench,help_intf_writebench},
	// change the vbuf?
	{'B',CommandParserIntfBuf,help_intf_buf},
	{'S',CommandParserIntfStatus,help_intf_status},
	{'E',CommandParserIntfEvents,help_intf_events},

};
const unsigned char CommandParsersInterfaceNum=sizeof(CommandParsersInterface)/sizeof(COMMANDPARSER);

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
	if(ns<0 || ns > 10)
	{
		fprintf(file_pri,"Invalid duration\n");
		return 0;
	}

	fprintf(file_pri,"Benchmark write on interface %s for %d seconds with method %s\n",intf?"BT":"USB",ns,method?"fputbuf":"fputs");

	// Convert seconds in milliseconds
	ns*=1000;

	// Select interface
	FILE *fi;
	if(intf==0)
		fi = file_usb;
	else
		fi = file_bt;

	for(int i=0;i<n;i++)
	{
		buf[i] = 'A'+(i%26);
	}
	buf[n]=0;

	// Benchmark with fwrite

	unsigned wr=0;


	unsigned t1,t2;
	unsigned it=0;
	switch(method)
	{
		case 0:
		{
			t1 = timer_ms_get();
			do
			{
				//wr += fwrite(buf,n,1,fb);
				if(fputs(buf,fi)>=0)
					wr++;
				it++;
			}
			while( (t2 = timer_ms_get())-t1 < ns);
			break;
		}
		case 1:
		default:
		{
			t1 = timer_ms_get();
			do
			{
				//wr += fwrite(buf,n,1,fb);
				if(fputbuf(fi,buf,n)==0)
					wr++;
				it++;
			}
			while( (t2 = timer_ms_get())-t1 < ns);
			break;
		}

	}
	fprintf(file_pri,"\nSent %u buffers of %d (tot: %d). Successfully wrote %d buffers. Time: %d\n",it,n,it*n,wr,t2-t1);
	fprintf(file_pri,"Speed: %u KB/s\n",wr*n*1000/1024/(t2-t1));

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
}
unsigned char CommandParserIntfEvents(char *buffer,unsigned char size)
{
	serial_uart_printevents(file_pri);
	return 0;
}
void mode_interface(void)
{
	mode_run("INTF",CommandParsersInterface,CommandParsersInterfaceNum);
}


