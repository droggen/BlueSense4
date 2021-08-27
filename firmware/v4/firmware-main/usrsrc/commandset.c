/*#include "cpu.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <util/atomic.h>*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "atomicop.h"
#include "global.h"
#include "command.h"
#include "commandset.h"
#include "helper.h"
//#include "mode_eeprom.h"
#include "mode.h"
#include "usrmain.h"
#include "wait.h"
#include "system.h"
#include "system-extra.h"
#include "uiconfig.h"
/*#include "init.h"
#include "dbg.h"*/
#include "interface.h"
#include "mode_global.h"
#include "ltc2942.h"
#include "eeprom-m24.h"
#include "power.h"
#include "serial_itm.h"
#include "max31343.h"

//#include "ufat.h"

#include "i2c/i2c_transaction.h"
#include "i2c/stmi2c.h"

#include "cmsis_gcc.h"

// Command help



const char help_z[]  ="Z[,<hh><mm><ss><dd><mm><yy>]: synchronise time.\n\t\tNo parameters: reset local time/date";
const char help_y[]  ="Test sync";
const char help_demo[]  ="Demo mode";
const char help_c[]  ="Clock mode";
const char help_w[]  ="Swap primary and secondary interfaces";
const char help_i[]  ="I<period>,<#tx>: Sets USB IO parameters. IO at (period+1)/1024Hz.\n\t\tUse #tx slots for transmission before reception.";
#if BUILD_BLUETOOTH==1
const char help_r[]  ="RN-41 terminal";
#endif
const char help_b[]  ="Benchmark IO interfaces";
const char help_l[]  ="L,<en>: en=1 to enable LCD, 0 to disable";
const char help_t[]  ="T[,<hh><mm><ss>] Query or set time";
const char help_timetest[] = "t,<cmd>: cmd=0: print date/time continuously, 1: print backup registers";
const char help_ttest[]  ="Time-related tests";
const char help_d[]  ="t[,<dd><mm><yy>] Query or set date";
const char help_quit[]  ="Exit current mode";
const char help_h[]  ="Help";
const char help_a_test[]  ="Additional ADC functions";
const char help_s[]  ="S,<us>: test streaming/logging mode; us: sample period in microseconds";
const char help_f[]  ="F,<bin>,<pktctr>,<ts>,<bat>,<label>: bin: 1 for binary, 0 for text\n\t\tOther parameters: 1 to stream, 0 to omit.";
const char help_M[]  ="M[,<mode>[,<logfile>[,<duration>]]: MPU sampling/logging.\n\t\tNo parameters to lists modes, otherwise enters the specified mode.\n\t\tLogs to logfile (use -1 not to log) and runs for the specified duration.";
const char help_m[]  ="Additional MPU functions";
const char help_g[]  ="G,<mode> enters motion recognition mode. The parameter is the sample rate/channels to acquire. Use G? to find more about modes";
const char help_O[]  ="O[,<sec>] Power off and no wakeup, or wakeup after <sec> seconds.";
const char help_o[]  ="Display power used in off mode";
const char help_p[]  ="Store data to measure power in off mmodepower used in off mode; if the node was turned off with O";
const char help_coulomb[]  ="Coulomb counter test mode";
const char help_sd[]  ="SD card test mode";
const char help_identify[]  ="Identify device by blinking LEDs";
const char help_annotation[]  ="N,<number>: sets the current annotation";
const char help_bootscript[]  ="b[,run;a boot;script] Prints the current bootscript or sets a new one\n\t\tmultiple commands are delimited by a ;";
const char help_info[]  ="i,<ien> Prints battery and logging information when streaming/logging when ien=1";
const char help_batterylong[] ="Long-term battery info";
const char help_battery[] ="Short-term battery info";
const char help_powertest[] ="Power tests";
const char help_callback[]  ="Lists timer callbacks";
const char help_clearbootctr[]  ="Clear boot counter";
const char help_eeprom[] = "EEPROM functions";
const char help_rtc[] = "Internal RTC functions";
const char help_usbreinit[] = "USB reinit";
const char help_interrupts[] = "Interrupt tryouts";
const char help_i2c[] = "I2C tests";
//const char help_clear[] PROGMEM ="Lists timer callbacks";
const char help_interface[]="Interface functions (benchmark, swap, etc)";
const char help_benchmark[]="CPU and IO benchmarks";
const char help_fat[]="fat test";
const char help_sleep[] = "p[,<0|1>] Query sleep state or enable (1) or disable (0) sleep while waiting user input";
const char help_cpureg[] = "Print CPU registers";
const char help_audio[] = "Additional audio functions";

const char help_1[]  ="Fcn 1";

unsigned CurrentAnnotation=0;

/******************************************************************************
	CommandParser format
*******************************************************************************	
Parser must return 0 for success, 1 for error decoding command
0=OK
1=INVALID
Exec must return 0 for success, 1 for error executing command	
******************************************************************************/
/*
New command format:

	0:	Message execution ok (message valid)
	1:	Message execution error (message valid)
	2:	Message invalid 
*/
const unsigned char CommandParsersDefaultNum=4;
const COMMANDPARSER CommandParsersDefault[4] =
{ 
	//{'T', CommandParserTime},
	//{'D', CommandParserDate},
	{'S', CommandParserSuccess,0},
	{'E', CommandParserError,0},
};

unsigned char __CommandQuit=0;
/*
	Parsers must return:
		0:	Message execution ok (message valid)
		1:	Message execution error (message valid)
		2:	Message invalid 		
*/
/******************************************************************************
	function: CommandParserTime
*******************************************************************************
	Either displays the RTC time if no argument is provided, or sets the
	RTC time to the hhmmss argument.
	The local time is synchronised to the RTC (epoch).

	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid

******************************************************************************/
/*unsigned char CommandParserTime(char *buffer,unsigned char size)
{
	unsigned char h=0,m=0,s=0;

	if(size==0)
	{
		// Query time
		//ds3232_readdatetime_conv_int(1,&h,&m,&s,0,0,0);
		fprintf(file_pri,"%02d:%02d:%02d\n",h,m,s);
		return 0;
	}
	// Set time
	if(size!=7 || buffer[0]!=',')
		return 2;

	buffer++;	// Skip comma


	// Check the digits are in range
	if(checkdigits(buffer,6))
		return 2;

	str2xxyyzz(buffer,&h,&m,&s);

	fprintf(file_pri,"Time: %02d:%02d:%02d\n",h,m,s);

	if(h>23 || m>59 || s>59)
	{
		return 2;
	}

	// Execute
	//unsigned char rv = ds3232_writetime(h,m,s);
	unsigned char rv=0;
	if(rv==0)
		return 0;
	// Synchronise local time to RTC time
	//system_settimefromrtc();
	return 1;
}*/
/******************************************************************************
	function: CommandParserSync
*******************************************************************************	
	Receive hhmmss time, update the RTC and synchronise the local time to the 
	RTC.
	
	This function operates in two steps:
	- 	First, it immediately sets the RTC time - this ensures the RTC is synchronised
		to the provided time.
	-	Then, it synchronises the local time to the RTC using system_settimefromrtc.
		
	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid 

******************************************************************************/
unsigned char CommandParserSync(char *buffer,unsigned char size)
{
	if(size!=0)
	{
		// Parameters are passed: check validity
		if(size!=13 || buffer[0]!=',')
			return 2;
		
		buffer++;	// Skip comma

		// Check the digits are in range
		if(checkdigits(buffer,12))
			return 2;
			
		unsigned char h,m,s,d,month,y;
		str2xxyyzz(buffer,&h,&m,&s);
		str2xxyyzz(buffer+6,&d,&month,&y);	
		
		
		//fprintf_P(file_dbg,PSTR("Time: %02d:%02d:%02d\n"),h,m,s);
		
		if(h>23 || m>59 || s>59 || d>31 || month>12)
		{
			return 2;
		}
		
		
		// Update the RTC date
		if(rtc_writedatetime(1,d,month,y,h,m,s))
			return 1;		
		// Synchronise local time to RTC time
		system_settimefromrtc();
		return 0;
	}
	// No parameter - reset time to 0
	timer_init(0,0,0);
	return 0;
}
/*unsigned char CommandParserDate(char *buffer,unsigned char size)
{
	unsigned char d=0,m=0,y=0;
	if(size==0)
	{
		// Query date
		//ds3232_readdate_conv_int(&d,&m,&y);
		fprintf(file_pri,"%02d.%02d.%02d\n",d,m,y);
		return 0;
	}
	// Set date
	if(size!=7 || buffer[0]!=',')
		return 2;

	buffer++;	// Skip comma

	// Check the digits are in range
	if(checkdigits(buffer,6))
		return 2;

	str2xxyyzz(buffer,&d,&m,&y);

	fprintf(file_pri,"Date: %02d.%02d.%02d\n",d,m,y);

	if(d>31 || d<1 || m>12 || m<1 || y>99)
	{
		return 2;
	}

	//unsigned char rv = ds3232_writedate_int(1,d,m,y);
	unsigned char rv = 0;
	if(rv==0)
		return 0;

	// Synchronise local time to RTC time
	//system_settimefromrtc();
	return 1;
}*/
unsigned char CommandParserTime_Test(char *buffer,unsigned char size)
{
	/*
	// Demonstrate the second transition at the falling edge of the NT#/SQW signal.
	unsigned char h,m,s;
	unsigned long t1,t2,t3;
	t1=timer_ms_get_intclk();
	while(timer_ms_get_intclk()-t1<3000)
	{
		ds3232_readdatetime_conv_int(0,&h,&m,&s,0,0,0);		
		printf("%02d:%02d:%02d %d\n",h,m,s,(PINA>>6)&1);
		_delay_ms(100);
	}
	printf("\n");
	while(1)
	{
		unsigned char p1,p2;
		// Wait for falling edge
		p1=(PINA>>6)&1;
		while(1)
		{
			p2=(PINA>>6)&1;
			if(p1!=p2 && p2==0)
				break;
			p1=p2;
		}
		_delay_ms(750);
		ds3232_writetime(10,11,12);
	}
	
	// Wait for 
	for(unsigned char i=0;i<3;i++)
	{
		unsigned char p1,p2;
		
		// Wait for falling edge
		p1=(PINA>>6)&1;
		while(1)
		{
			p2=(PINA>>6)&1;
			if(p1!=p2 && p2==0)
				break;
			p1=p2;
		}
		t1 = timer_ms_get_intclk();
		// Wait for another falling edge
		p1=(PINA>>6)&1;
		while(1)
		{
			p2=(PINA>>6)&1;
			if(p1!=p2 && p2==0)
				break;
			p1=p2;
		}
		t2 = timer_ms_get_intclk();
		printf("dt1: %ld\n",t2-t1);
		ds3232_readtime_conv(&h,&m,&s);
		printf("%02d:%02d:%02d %d\n",h,m,s,(PINA>>6)&1);
		// Wait 300ms
		_delay_ms(50);
		printf("dt2: %ld\n",timer_ms_get_intclk()-t1);
		ds3232_readtime_conv(&h,&m,&s);
		printf("dt2b: %ld\n",timer_ms_get_intclk()-t1);
		printf("+%02d:%02d:%02d %d\n",h,m,s,(PINA>>6)&1);
		// Set the time
		printf("dt2c: %ld\n",timer_ms_get_intclk()-t1);
		unsigned long tt1,tt2;
		tt1=timer_ms_get_intclk();
		printf("b%d",(PINA>>6)&1);
		ds3232_writetime(10,11,12);
		printf("a%d\n",(PINA>>6)&1);
		tt2=timer_ms_get_intclk();
		printf("dt2d: %ld tt: %ld\n",timer_ms_get_intclk()-t1,tt2-tt1);
		ds3232_readtime_conv(&h,&m,&s);
		printf("dt2e: %ld\n",timer_ms_get_intclk()-t1);
		printf("*%02d:%02d:%02d %d\n",h,m,s,(PINA>>6)&1);
		
		printf("dt3: %ld\n",timer_ms_get_intclk()-t1);
		
		// Wait for another falling edge
		p1=(PINA>>6)&1;
		while(1)
		{
			p2=(PINA>>6)&1;
			if(p1!=p2 && p2==0)
				break;
			p1=p2;
		}
		t3 = timer_ms_get_intclk();
		printf("dt4: %ld\n",t3-t1);
		ds3232_readtime_conv(&h,&m,&s);
		printf("%02d:%02d:%02d %d\n",h,m,s,(PINA>>6)&1);
		
		_delay_ms(2500);
	}
	*/
	/*unsigned long ctr=0;
	while(1)
	{
		ctr++;
		_delay_ms(77);
		unsigned char rv;
		if( (ctr&0xF) ==0)
			rv=ds3232_writetime(10,11,12);
		unsigned long tt1=timer_ms_get();
		unsigned long tt1s,tt2s,tt11000,tt21000;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tt1s = _timer_time_1_in_ms;
			tt11000 = _timer_time_1000;
		}
		printf("%ld %ld.%ld (%d)\n",tt1,tt1s,tt11000,rv);
		rv=0x55;
	}*/
	/*for(int i=0;i<100;i++)
	{
		_delay_ms(rand()%1000);
		unsigned long tt1=timer_ms_get_intclk();
		unsigned long tt1s,tt2s,tt11000,tt21000;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tt1s = _timer_time_1_in_ms;
			tt11000 = _timer_time_1000;
		}
		
		unsigned char p1 = (PINA>>6)&1;
		ds3232_writetime(10,11,12);
		unsigned char p2 = (PINA>>6)&1;
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			tt2s = _timer_time_1_in_ms;
			tt21000 = _timer_time_1000;
		}
		unsigned long tt2=timer_ms_get_intclk();
		printf("dt: %ld (%ld %ld) %ld.%ld -> %ld.%ld %d->%d\n",tt2-tt1,tt1,tt2,tt1s,tt11000,tt2s,tt21000,p1,p2);
	}*/
	
	
	
	return 0;
}

// Always success
unsigned char CommandParserQuit(char *buffer,unsigned char size)
{
	__CommandQuit=1;
	return 0;
}
// Always success
unsigned char CommandParserHelp(char *buffer,unsigned char size)
{
	fprintf(file_pri,"Available commands:\n");
	for(unsigned char i=0;i<CommandParsersCurrentNum;i++)
	{
		if(CommandParsersCurrent[i].cmd)
			fprintf(file_pri,"\t%c\t",CommandParsersCurrent[i].cmd);
		else
			fprintf(file_pri,"\t");
		if(CommandParsersCurrent[i].help)
			fputs(CommandParsersCurrent[i].help,file_pri);
		fputc('\n',file_pri);
	}	
	return 0;
}
unsigned char CommandParserSuccess(char *buffer,unsigned char size)
{
	return 0;
}
unsigned char CommandParserError(char *buffer,unsigned char size)
{
	return 1;
}

/*unsigned char CommandParserLCD(char *buffer,unsigned char size)
{	
	int lcden;
	
	unsigned char rv = ParseCommaGetInt(buffer,1,&lcden);
	if(rv)
		return 2;
	
	// Store 
	system_enable_lcd = lcden;
	ConfigSaveEnableLCD(lcden);	
	fprintf_P(file_pri,PSTR("LCD enabled: %d\n"),lcden);
	if(lcden==1)
		init_lcd();
	else
		deinit_lcd();
	return 0;
}*/
unsigned char CommandParserInfo(char *buffer,unsigned char size)
{	
	int ien2;

	int ien1 = ConfigLoadEnableInfo();
	unsigned char rv = ParseCommaGetInt(buffer,1,&ien2);
	if(!rv)
	{
		// Store
		ConfigSaveEnableInfo(ien2);
	}
	ien2 = ConfigLoadEnableInfo();
	fprintf(file_pri,"Information during streaming/logging enabled: %d (previously was %d)\n",ien2,ien1);
	return 0;
}

unsigned char CommandShouldQuit(void)
{
	unsigned char t=__CommandQuit;
	__CommandQuit=0;
	return t;
}

unsigned char CommandParserIO(char *buffer,unsigned char size)
{
	/*unsigned char rv;
	int period,txslot;
	
	rv = ParseCommaGetInt((char*)buffer,2,&period,&txslot);
	if(rv || period<0 || txslot<=0)
		return 2;

	// Check params, clamp at some reasonable value
	if(period>999)
		period=999;
	
	if(txslot>256)
		txslot=256;
		
	dbg_setioparam(period,txslot);*/
		
	return 0;
}
unsigned char CommandParserSwap(char *buffer,unsigned char size)
{
	interface_swap();
		
	return 0;
}
unsigned char CommandParserOff(char *buffer,unsigned char size)
{
	if(ParseCommaGetNumParam(buffer)==0)
	{
		fprintf(file_pri,"Shutting down, no wakeup\n");
		system_off();
	}
	unsigned nsec;
	if(ParseCommaGetUnsigned(buffer,1,&nsec))
		return 2;

	fprintf(file_pri,"Shutting down, with wakeup in %d seconds\n",nsec);
	max31343_alarm_in(nsec);
	system_off();

	return 0;
}
unsigned char CommandParserOffPower(char *buffer,unsigned char size)
{
	power_print_off_on_info();
	return 0;
}
unsigned char CommandParserOffStore(char *buffer,unsigned char size)
{
	// Power in off mode
	//system_storepoweroffdata();
	return 0;
}
unsigned char CommandParserStreamFormat(char *buffer,unsigned char size)
{
	unsigned char rv;
	int bin,pktctr,ts,bat,label;
	
	//printf("string: '%s'\n",buffer);
	
	rv = ParseCommaGetInt((char*)buffer,5,&bin,&pktctr,&ts,&bat,&label);
	if(rv)
		return 2;
	//printf("%d %d %d %d %d\n",bin,pktctr,ts,bat,label);
		
	
	bin=bin?1:0;
	pktctr=pktctr?1:0;
	ts=ts?1:0;
	bat=bat?1:0;
	label=label?1:0;
				
	
	mode_stream_format_bin = bin;	
	mode_stream_format_ts = ts;		
	mode_stream_format_bat = bat;		
	mode_stream_format_label = label;
	mode_stream_format_pktctr = pktctr;
	
	fprintf(file_pri,"Streaming parameters: binary: %d. packet counter: %d timestamp: %d battery: %d label: %d\n",bin,pktctr,ts,bat,label);
	
	ConfigSaveStreamBinary(bin);
	ConfigSaveStreamPktCtr(pktctr);
	ConfigSaveStreamTimestamp(ts);
	ConfigSaveStreamBattery(bat);
	ConfigSaveStreamLabel(label);
		
	return 0;
}




/*
	Some power tests
	
	Available sleep modes:
	SLEEP_MODE_IDLE				// 20mW reduction
	SLEEP_MODE_PWR_DOWN			// Cannot wake up
	SLEEP_MODE_PWR_SAVE			// Cannot wake up
	SLEEP_MODE_STANDBY			// Cannot wake up
	SLEEP_MODE_EXT_STANDBY		// Cannot wake up
	
	
*/
//unsigned char CommandParserPowerTest(char *buffer,unsigned char size)
//{
	/*unsigned long t1,tlast,tcur,stat_loop;
	fprintf_P(file_pri,PSTR("Power tests\n"));
	
	// Unregister lifesign
	timer_unregister_slowcallback(system_lifesign);
	// Turn off all LEDs
	system_led_set(0b000);*/

	/*fprintf_P(file_pri,PSTR("Busy loop, LED off\n"));	
	t1=tlast=timer_ms_get();
	stat_loop=0;
	while(1)
	{
		tcur=timer_ms_get();
		if(tcur-t1>20000)
			break;
		stat_loop++;
		if(tcur-tlast>=10000)
		{
			fprintf_P(file_pri,PSTR("%lu loops. %s\n"),stat_loop,ltc2942_last_strstatus());
			tlast=tcur;
			stat_loop=0;
		}
	}
	
	system_led_set(0b111);

	fprintf_P(file_pri,PSTR("Busy loop, LED on\n"));	
	t1=tlast=timer_ms_get();
	stat_loop=0;
	while(1)
	{
		tcur=timer_ms_get();
		if(tcur-t1>20000)
			break;
		stat_loop++;
		if(tcur-tlast>=10000)
		{
			fprintf_P(file_pri,PSTR("%lu loops. %s\n"),stat_loop,ltc2942_last_strstatus());
			tlast=tcur;
			stat_loop=0;
		}
	}*/
	
	/*system_led_set(0b000);
	
	fprintf_P(file_pri,PSTR("Busy loop with idle sleep, LED off\n"));
	set_sleep_mode(SLEEP_MODE_IDLE); 		// 20mW reduction

	sleep_enable();
	t1=tlast=timer_ms_get();
	stat_loop=0;
	while(1)
	{
		sleep_cpu();
		tcur=timer_ms_get();
		if(tcur-t1>30000)
			break;
		stat_loop++;
		if(tcur-tlast>=10000)
		{
			fprintf_P(file_pri,PSTR("%lu loops. %s\n"),stat_loop,ltc2942_last_strstatus());
			tlast=tcur;
			stat_loop=0;
		}
	}*/


	/*fprintf_P(file_pri,PSTR("Busy loop with idle sleep, LED off, analog off\n"));
	set_sleep_mode(SLEEP_MODE_IDLE); 		// 20mW reduction
	ACSR=0x10;		// Clear analog compare interrupt (by setting ACI/bit4 to 1) and clear interrupt enable (setting ACIE/bit3 to 0)
	ACSR=0x80;		// Disable analog compare	(setting ACD/bit7 to 1)
	fprintf_P(file_pri,PSTR("ACSR: %02X\n"),ACSR);

	sleep_enable();
	t1=tlast=timer_ms_get();
	stat_loop=0;
	while(1)
	{
		sleep_cpu();
		tcur=timer_ms_get();
		if(tcur-t1>30000)
			break;
		stat_loop++;
		if(tcur-tlast>=10000)
		{
			fprintf_P(file_pri,PSTR("%lu loops. %s\n"),stat_loop,ltc2942_last_strstatus());
			tlast=tcur;
			stat_loop=0;
		}
	}*/

	/*fprintf_P(file_pri,PSTR("Busy loop with idle sleep, LED off, analog off\n"));
	set_sleep_mode(SLEEP_MODE_IDLE); 		// 20mW reduction
	fprintf_P(file_pri,PSTR("ASSR: %02X\n"),ASSR);
	fprintf_P(file_pri,PSTR("PRR0: %02X\n"),PRR0);
	PRR0 = 0b01100001;
	fprintf_P(file_pri,PSTR("PRR0: %02X\n"),PRR0);
	

	sleep_enable();
	t1=tlast=timer_ms_get();
	stat_loop=0;
	while(1)
	{
		sleep_cpu();
		tcur=timer_ms_get();
		if(tcur-t1>30000)
			break;
		stat_loop++;
		if(tcur-tlast>=10000)
		{
			fprintf_P(file_pri,PSTR("%lu loops. %s\n"),stat_loop,ltc2942_last_strstatus());
			tlast=tcur;
			stat_loop=0;
		}
	}
	
	fprintf_P(file_pri,PSTR("Power tests done\n"));
	
	timer_register_slowcallback(system_lifesign,0);
	
	return 0;*/
//}


unsigned char CommandParserSyncFromRTC(char  *buffer,unsigned char size)
{
	system_settimefromrtc();
	return 0;
}
unsigned char CommandParserTestSync(char *buffer,unsigned char size)
{
	/*fprintf_P(file_pri,PSTR("Test sync command\n"));
	CommandChangeMode(APP_MODE_TESTSYNC);*/
	return 0;
}

unsigned char CommandParserIdentify(char *buffer,unsigned char size)
{
	fprintf(file_pri,"My name is %s\n",system_getdevicename());
	system_blink(10,100,0);
	return 0;
}


unsigned char CommandParserAnnotation(char *buffer,unsigned char size)
{
	int label;

	if(ParseCommaGetInt(buffer,1,&label))
		return 2;
	

	CurrentAnnotation=label;
	
	return 0;
}

unsigned char CommandParserMPUTest(char *buffer,unsigned char size)
{
	CommandChangeMode(APP_MODE_MPUTEST);
	return 0;
}
unsigned char CommandParserMPUTestN(char *buffer,unsigned char size)
{
	CommandChangeMode(APP_MODE_MPUTESTN);
	return 0;
}

/******************************************************************************
	function: CommandParserBootScript
*******************************************************************************	
	Parses the boot script command. 
	
	If no parameter is passed it prints the current boot script.
	
	If a parameter is passed, the rest of the string after the comma is stored
	as boot script.
	
	To clear the boot script, pass "b,".

	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid 

******************************************************************************/
unsigned char CommandParserBootScript(char *buffer,unsigned char size)
{
	//printf("size: %d\n",size);
	if(size!=0)
	{
		// If the first character is not a comma return an error.
		if(buffer[0]!=',')
			return 2;
		// Skip the comma, the size decreases accordingly.
		buffer++;
		size--;
		// The maximum scrip length is CONFIG_ADDR_SCRIPTLEN-2, as the last byte must be NULL and the one prior must be a command delimiter
		if(size>CONFIG_ADDR_SCRIPTLEN-2)
		{
			fprintf(file_pri,"Boot script too long\n");
			return 1;
		}
		char tmpbuf[CONFIG_ADDR_SCRIPTLEN];
		strcpy(tmpbuf,buffer);
		// Add a terminating newline (semicolon)
		strcat(tmpbuf,";");
		fprintf(file_pri,"saving %d bytes\n",strlen(tmpbuf)+1);
		// Save the string including the null char
		ConfigSaveScript(tmpbuf,strlen(tmpbuf)+1);
	}
	
	// Read and print script
	char buf[CONFIG_ADDR_SCRIPTLEN];
	ConfigLoadScript(buf);
	fprintf(file_pri,"Boot script: \"");
	prettyprint_hexascii(file_pri,buf,strlen(buf),0);
	fprintf(file_pri,"\"\n");
	
	return 0;
}
unsigned char CommandParserBatteryInfoLong(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	ltc2942_print_longbatstat(file_pri);

	return 0;
}
unsigned char CommandParserBatteryInfo(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"#%s\n",ltc2942_last_strstatus());
	for(unsigned char i=0;i<LTC2942NUMLASTMW;i++)
		fprintf(file_pri,"%d ",ltc2942_last_mWs(i));
	fprintf(file_pri,"\n");

	return 0;
}
unsigned char CommandParserCallback(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	timer_printcallbacks(file_pri);
	return 0;
}
unsigned char CommandParserClearBootCounter(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	eeprom_write_dword(STATUS_ADDR_NUMBOOT0,0);
	fprintf(file_pri,"Cleared\n");
	return 0;
}
void CommandChangeMode(unsigned char newmode)
{
	//if(system_mode!=newmode)
		__CommandQuit=1;
	system_mode = newmode;
}



unsigned char CommandParserEEPROM(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_EEPROM);
	return 0;
}

unsigned char CommandParserBench(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_BENCHIO);
	return 0;
}
unsigned char CommandParserRTC(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_RTC);
	return 0;
}
unsigned char CommandParserRTCExt(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_RTC_EXT);
	return 0;
}
unsigned char CommandParserI2CTest(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_I2CTST);
	return 0;
}
unsigned char CommandParserInterface(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_INTERFACE);
	return 0;
}
unsigned char CommandParserInterrupts(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	fprintf(file_pri,"Interrupt stuff\n");

	//unsigned r = PRIMASK;
	unsigned r;

	r = __get_PRIMASK();
	itmprintf("PRIMASK: %08X\n",r);
	fprintf(file_pri,"PRIMASK: %08X\n",r);


	// disable irq
	__disable_irq(); // Set I-bit in the CPSR

	r = __get_PRIMASK();
	itmprintf("PRIMASK: %08X\n",__get_PRIMASK());
	fprintf(file_pri,"PRIMASK: %08X\n",r);

	__enable_irq();
	r = __get_PRIMASK();
	itmprintf("PRIMASK: %08X\n",__get_PRIMASK());
	fprintf(file_pri,"PRIMASK: %08X\n",r);

	itmprintf("Try atomic: %08X\n",__get_PRIMASK());

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		itmprintf("In atomic: %08X\n",__get_PRIMASK());
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			itmprintf("In atomic of atomic: %08X\n",__get_PRIMASK());
		}
		itmprintf("After atomic of atomic: %08X\n",__get_PRIMASK());

	}
	itmprintf("After atomic: %08X\n",__get_PRIMASK());

	return 0;
}











unsigned char CommandParserSleep(char *buffer,unsigned char size)
{
	(void) size;

	if(ParseCommaGetNumParam(buffer)==0)
	{
		fprintf(file_pri,"Current sleep mode: %d.\n",__mode_sleep);
		return 0;
	}

	int sleep;
	if(ParseCommaGetInt(buffer,1,&sleep))
		return 2;

	unsigned char s = __mode_sleep;

	if(sleep)
		__mode_sleep=1;
	else
		__mode_sleep=0;


	fprintf(file_pri,"Previous sleep mode: %d. New sleep mode: %d\n",s,__mode_sleep);
	return 0;
}

unsigned char CommandParserCPUReg(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

#if 1
	/*if(ParseCommaGetNumParam(buffer))
	{
		fprintf(file_pri,"Changing voltage scaling\n");
		HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE3);
	}*/


	unsigned short pwrcr = PWR->CR1;

	fprintf(file_pri,"PWR CR1: %04X\n",pwrcr);
	fprintf(file_pri,"\tLPR: %d\n",(pwrcr>>14)&0b1);
	fprintf(file_pri,"\tVOS: %d\n",(pwrcr>>9)&0b11);
	fprintf(file_pri,"\tVDBP: %d\n",(pwrcr>>8)&0b1);
	fprintf(file_pri,"\tLPMS: %d\n",(pwrcr>>0)&0b111);
	/*fprintf(file_pri,"\tMRLVDS: %d\n",(pwrcr>>11)&0b1);
	fprintf(file_pri,"\tLPLVDS: %d\n",(pwrcr>>10)&0b1);
	fprintf(file_pri,"\tFPSD: %d\n",(pwrcr>>9)&0b1);
	fprintf(file_pri,"\tPLS: %d\n",(pwrcr>>5)&0b111);
	fprintf(file_pri,"\tPCDE: %d\n",(pwrcr>>4)&0b1);*/

	unsigned short flashacr = FLASH->ACR;
	fprintf(file_pri,"FLASH ACR: %04X\n",flashacr);
	fprintf(file_pri,"\tDCEN: %d\n",(flashacr>>10)&0b1);
	fprintf(file_pri,"\tICEN: %d\n",(flashacr>>9)&0b1);
	fprintf(file_pri,"\tPRFTEN: %d\n",(flashacr>>8)&0b1);
	fprintf(file_pri,"\tLATENCY: %d\n",(flashacr>>0)&0b1111);

	FLASH->ACR |= 1<<8;
#endif
	return 0;
}

unsigned char CommandParserAudio(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_AUDIO);
	return 0;
}
unsigned char CommandParserBenchmark(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_BENCHMARK);
	return 0;
}
unsigned char CommandParserModeDACTest(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_DACTEST);
	return 0;
}
unsigned char CommandParserWait(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	unsigned delay;
	if(ParseCommaGetInt(buffer,1,&delay))
		return 1;
	HAL_Delay(delay);
	return 0;
}
unsigned char CommandParserPeriphPower(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	int en;
	if(ParseCommaGetInt(buffer,1,&en))
		return 2;

	if(en)
	{
		system_periphvcc_enable();
		fprintf(file_pri,"Peripheral power enabled\n");
	}
	else
	{
		system_periphvcc_disable();
#warning IO ports should be put in analog HiZ or input to avoid driving peripherals through inputs.
		fprintf(file_pri,"Peripheral power disabled\n");
	}
	return 0;
}

unsigned char CommandParserRamp(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	int delay,from,lowerthan,step;
	int exit=0;

	//fprintf(file_pri,"Ramp\n");

	if(ParseCommaGetInt(buffer,4,&delay,&from,&lowerthan,&step))
		return 2;

	fprintf(file_pri,"Ramp generation: from %d to %d by %d, every %dms\n",from,lowerthan,step,delay);

	HAL_Delay(100);

	while(!exit)
	{
		for(int i=from;i<lowerthan;i+=step)
		{
			fprintf(file_pri,"%d\n",i);
			HAL_Delay(delay);
			if(fgetc(file_pri)!=-1)
			{
				exit=1;
				break;
			}
		}
	}

	return 0;
}

unsigned char CommandParserRandomWalk(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	int delay,from,to,jump;
	int exit=0;

	//fprintf(file_pri,"Ramp\n");

	if(ParseCommaGetInt(buffer,4,&delay,&from,&to,&jump))
		return 2;

	fprintf(file_pri,"Random generation: from %d to %d, jumping %d,every %dms\n",from,to,jump,delay);

	HAL_Delay(100);

	int v = from+(to-from)/2;

	while(!exit)
	{
		// Update v;
		int r = (rand()%(2*jump+1))-jump;
		v+=r;
		if(v<from)
			v=from;
		if(v>to)
			v=to;


		fprintf(file_pri,"%d\n",v);
		HAL_Delay(delay);
		if(fgetc(file_pri)!=-1)
		{
			exit=1;
			break;
		}

	}

	return 0;
}

/*unsigned char CommandTestPwr(char *buffer,unsigned char size)
{


	fprintf(file_pri,"testing pwr\n");

	fprintf(file_pri,"Store power\n");

	system_storepowerdata();

	//HAL_Delay(1000);
	HAL_Delay(1);

	// Add some additional wait
	//while(!_i2c_transaction_idle);

	fprintf(file_pri,"Read eeprom\n");
	m24xx_printreg(file_pri,500,545);

	fprintf(file_pri,"Erase cells\n");
	for(int i=512;i<532;i++)
		m24xx_write(i,0x7f);

	fprintf(file_pri,"Read eeprom again\n");
		m24xx_printreg(file_pri,500,545);

	fprintf(file_pri,"Done\n");

	return 0;
}*/
unsigned char CommandParserVT100(char *buffer,unsigned char size)
{
	if(ParseCommaGetNumParam(buffer)!=0)
	{
		unsigned en;
		if(ParseCommaGetUnsigned(buffer,1,&en))
			return 2;
		if(en>1)
			return 2;

		__mode_vt100=en;
		ConfigSaveVT100(__mode_vt100);
	}

	// Prints the current VT100 mode.
	fprintf(file_pri,"VT 100 mode: %s\n",__mode_vt100?"ON":"OFF");
	return 0;

}




