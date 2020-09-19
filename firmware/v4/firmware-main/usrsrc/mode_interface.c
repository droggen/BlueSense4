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
#include "serial.h"
#include "mode_main.h"
#include "ymodem.h"

const COMMANDPARSER CommandParsersInterface[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'Y', CommandParserIntfYModemSend,"Send via YModem"},


};
const unsigned char CommandParsersInterfaceNum=sizeof(CommandParsersInterface)/sizeof(COMMANDPARSER);


void mode_interface(void)
{
	mode_run("INTF",CommandParsersInterface,CommandParsersInterfaceNum);
}


unsigned char CommandParserIntfYModemSend(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;

	/*unsigned n=2560;
	unsigned char data[n+1024];			// The buffer must be rounded to 1024


	for(int i=0;i<n;i++)
		data[i] = i&0xff;

	fprintf(file_dbg,"Sending in 5 seconds\n");

	for(int i=0;i<100;i++)
		fprintf(file_dbg,"%02X ",data[i]);
	fprintf(file_dbg,"\n");


	for(int i=0;i<5;i++)
	{
		fprintf(file_dbg,"%d\n",i);
		HAL_Delay(1000);
	}

	fprintf(file_dbg,"Sending now\n");

	ymodem_send("toto.tot",data,n,file_pri);
	*/

/*
	char *filenames[3] = {"file1.dat","file2.dat","file3.dat"};
	unsigned lens[3] = {100,1024,2560};
	unsigned char data1[4096];
	unsigned char data2[4096];
	unsigned char data3[4096];
	unsigned char *datas[3] = {data1,data2,data3};

	memset(data1,'1',4096);
	memset(data2,'2',4096);
	memset(data3,'3',4096);

	sprintf(data1,"D1 - D1");
	sprintf(data2,"D2 - D2");
	sprintf(data3,"D3 - D3");

	ymodem_send_multi(3,filenames,datas,lens,file_pri);*/

	char *filenames[3] = {"LOG-0000.000","LOG-0000.001","LOG-0000.002"};
	ymodem_send_multi_file(3,filenames,file_pri);
	return 0;
}
