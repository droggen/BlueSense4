/*
 * mode_rtcext.c
 *
 *  Created on: 29 août 2020
 *      Author: droggen
 */

#include "global.h"
#include "command.h"
#include "commandset.h"
#include "main.h"
#include "helper.h"
#include "wait.h"
#include "command_datetime.h"
#include "mode_main.h"
#include "mode_rtcext.h"
#include "max31343.h"
#include "stmrtc.h"
#include "rtcwrapper.h"



const COMMANDPARSER CommandParsersRTCExt[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'I', CommandParserRTCExtInit, "Init external RTC with default settings"},
	{'R',CommandParserRTCExtReadAllRegisters,"Read all registers"},
	{'r', CommandParserRTCExtReadRegister,"r,<reg>: read register <reg>"},
	{'w', CommandParserRTCExtWriteRegister,"w,<reg>,<val>: write <val> to register <reg>"},
	{'Z', CommandParserRTCExtReadDateTime,"Read date and time"},
	{'z', CommandParserRTCExtWriteDateTime,"z,<hh>,<mm>,<ss>,<dd>,<mm>,<yy>: sets the external RTC to hhmmss ddmmyy"},
	{'X', CommandParserRTCExtSwRst,"X,<rst>: Software reset when rst=1; clear reset when rst=0"},
	{'P', CommandParserRTCExtPollDateTime,"Poll date and time and SQW signal"},
	{'A', CommandParserRTCExtWriteAlarmIn,"A,<sec>: sets the external RTC alarm 1 in <sec> seconds"},
	{'a', CommandParserRTCExtWriteAlarm,"a,<hh>,<mm>,<ss>,<dd>,<mm>,<yy>: sets the external RTC alarm 1 to hhmmss ddmmyy"},
	{'S', CommandParserRTCExtBgdReadStatus,"Read status register in background (interrupt driven)"},
	{'T', CommandParserRTCExtTemp,"Read temperature"},
	{'B', CommandParserRTCExtBootStatus,"Status byte on boot"},
	{0,0,"---- Development ----"},

	{'1', CommandParserRTCExtTest1,"test 1"},
	{'2', CommandParserRTCExtTest2,"test 2"},

};
const unsigned char CommandParsersRTCExtNum=sizeof(CommandParsersRTCExt)/sizeof(COMMANDPARSER);



void mode_rtcext(void)
{
	mode_run("RTCEXT",CommandParsersRTCExt,CommandParsersRTCExtNum);
}
unsigned char CommandParserRTCExtInit(char *buffer,unsigned char size)
{
	max31343_init();
	return 0;
}

unsigned char CommandParserRTCExtReadAllRegisters(char *buffer,unsigned char size)
{
	max31343_printregs(file_pri);
	return 0;
}

unsigned char CommandParserRTCExtWriteRegister(char *buffer,unsigned char size)
{
	unsigned addr,word;


	if(ParseCommaGetUnsigned(buffer,2,&addr,&word))
		return 2;

	fprintf(file_pri,"External RTC write %02X at address %02X\n",word,addr);


	if(max31343_writereg(addr,word))
		return 1;

	return 0;

}

unsigned char CommandParserRTCExtReadRegister(char *buffer,unsigned char size)
{
	unsigned addr;


	if(ParseCommaGetUnsigned(buffer,1,&addr))
		return 2;

	unsigned char val;
	if(max31343_readreg(addr,&val))
		return 1;

	fprintf(file_pri,"External RTC read register %02X: %02X\n",addr,(unsigned)val);


	return 0;

}
unsigned char CommandParserRTCExtReadDateTime(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned char hour,min,sec,weekday,day,month,year;
	rv = max31343_readdatetime_conv_int(1, &hour,&min,&sec,&weekday,&day,&month,&year);
	int sqw = max31341_sqw();

	if(rv)
		return 1;

	char buf[64];
	stmrtc_formatdatetime(buf,weekday,day,month,year,hour,min,sec);
	fprintf(file_pri,"SQW: %d Date/Time: %s\n",sqw,buf);


	return 0;
}
unsigned char CommandParserRTCExtPollDateTime(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned char hour,min,sec,weekday,day,month,year;
	char buf[64];


	fprintf(file_pri,"Reading until keypress\n");
	while(1)
	{
		if(fgetc(file_pri)!=-1)
			break;

		rv = max31343_readdatetime_conv_int(0, &hour,&min,&sec,&weekday,&day,&month,&year);
		int sqw = max31341_sqw();

		if(rv)
			return 1;



		stmrtc_formatdatetime(buf,weekday,day,month,year,hour,min,sec);
		fprintf(file_pri,"SQW: %d Date/Time: %s\n",sqw,buf);

		HAL_Delay(50);
	}


	return 0;
}
/*
	Some test dates:

	z,23,59,50,31,08,20
	z,23,59,50,31,12,20

*/
unsigned char CommandParserRTCExtWriteDateTime(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned int hour,min,sec,day,month,year;

	if(ParseCommaGetInt(buffer,6,&hour,&min,&sec,&day,&month,&year))
		return 2;

	char buf[64];
	stmrtc_formatdatetime(buf,1,day,month,year,hour,min,sec);
	fprintf(file_pri,"Setting date/time to: %s\n",buf);

	rv = max31343_writedatetime(1,day,month,year,hour,min,sec);
	if(rv)
		return 1;




	return 0;
}
unsigned char CommandParserRTCExtWriteAlarm(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned int hour,min,sec,day,month,year;

	if(ParseCommaGetInt(buffer,6,&hour,&min,&sec,&day,&month,&year))
		return 2;

	char buf[64];
	stmrtc_formatdatetime(buf,1,day,month,year,hour,min,sec);
	buf[10]=buf[11]=buf[12]='X'; // Overwrite the weekday
	fprintf(file_pri,"Setting alarm date/time to: %s\n",buf);

	rv = max31343_alarm_at(day,month,year,hour,min,sec);
	if(rv)
		return 1;




	return 0;
}
unsigned char CommandParserRTCExtWriteAlarmIn(char *buffer,unsigned char size)
{
	unsigned char rv;
	unsigned int sec;

	if(ParseCommaGetInt(buffer,1,&sec))
		return 2;

	max31343_alarm_in(sec);

	return 0;
}

unsigned char CommandParserRTCExtSwRst(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;
	int reset;

	if(ParseCommaGetInt(buffer,1,&reset))
		return 2;
	if(reset<0 || reset>1)
		return 2;

	max31343_swrst(reset);

	return 0;
}
unsigned char CommandParserRTCExtBgdReadStatus(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;

	max31343_background_read_status();

	return 0;
}
unsigned char CommandParserRTCExtTest1(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;

	int h,m,s;
	if(ParseCommaGetInt(buffer,3,&h,&m,&s))
		return 2;

	fprintf(file_pri,"Setting time to %d %d %d\n",h,m,s);
	rtc_writetime(h,m,s);
	for(int i=0;i<250;i++)
	{
		unsigned char hour,min,sec,weekday,day,month,year;
		rtc_readdatetime_conv_int(0, &hour,&min,&sec,&weekday,&day,&month,&year);
		int sqw = max31341_sqw();
		fprintf(file_pri,"%d: SQW %d  %d %d %d\n",i,sqw,hour,min,sec);
		HAL_Delay(10);
	}


	return 0;
}
/*
	Used to check when the time is updated after a write
*/
unsigned char CommandParserRTCExtTest2(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;


	unsigned long tstart = timer_ms_get_intclk();
	unsigned long treset = tstart;
	int h,m,s;

	while(1)
	{
		if(fgetc(file_pri)!=-1)
		{
			fprintf(file_pri,"Resetting time\n");
			rtc_writedatetime(1,1,1,20,0,0,0);
			treset = timer_ms_get_intclk();
		}

		unsigned char hour,min,sec,weekday,day,month,year;
		rtc_readdatetime_conv_int(0, &hour,&min,&sec,&weekday,&day,&month,&year);
		unsigned long t = timer_ms_get_intclk();
		int sqw = max31341_sqw();

		fprintf(file_pri,"Time since start: %ld. Time since reset: %ld. SQW %d. Time: %d %d %d\n",t-tstart,t-treset,sqw,hour,min,sec);
		HAL_Delay(25);

	}


	return 0;
}
unsigned char CommandParserRTCExtTemp(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;

	fprintf(file_pri,"Temperature (x100): %d\n",max31343_get_temp());

	return 0;
}
unsigned char CommandParserRTCExtBootStatus(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;

	fprintf(file_pri,"\tBoot status: %02X\n",max31341_get_boot_status());

	return 0;

}
