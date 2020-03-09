/*#include "cpu.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/eeprom.h>
#include <util/delay.h>*/
#include "main.h"
#include <mode_main.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
/*#include "adc.h"
#include "serial.h"
#include "i2c.h"
#include "ds3232.h"*/
#if BUILD_BLUETOOTH==1
#include "rn41.h"
#include "mode_bt.h"
#endif
/*#include "mpu.h"
#include "mpu_test.h"
#include "pkt.h"
#include "wait.h"
#include "init.h"
#include "lcd.h"
#include "fb.h"
#include "uiconfig.h"
#include "helper.h"
#include "i2c_internal.h"
#include "system.h"
#include "pkt.h"*/

/*#include "mode_demo.h"*/
#include "mode_sample_adc.h"
/*#include "mode_bench.h"*/
#include <mode_i2ctst.h>


#include "commandset.h"
#include "command_datetime.h"
#include "mode_sample_motion.h"
//#include "mode_motionrecog.h"
#include "mode_coulomb.h"
#include "mode_sd.h"
//#include "mode_teststream.h"
#include "command_usb.h"
#include "mode_mputest_n.h"
//#include "mode_mputest.h"
#include "mode_audio.h"
#include "mode_adc.h"
#include <mode_benchmarks.h>
#include "mode_sample_sound.h"
#include "mode_sample_multimodal.h"
#include "mode_dac.h"

#include "system-extra.h"


const char help_x[] ="x";

unsigned char CommandParserx(char *buffer,unsigned char size)
{

	/*for(unsigned char i=0;i<5;i++)
	{
		fputs(_poweruse_off.str[i],file_pri);
	}*/
	
	
		
	return 0;
}



const COMMANDPARSER CommandParsersIdle[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'T', CommandParserTime,help_t},
	{'t', CommandParserDate,help_d},
	{'r', CommandParserRTC,help_rtc},

	//{'t', CommandParserTimeTest,help_timetest},
	//{'r', CommandParserRTCTest,help_rtctest},

	//{'1', CommandParser1,help_1},


	//{'Z',CommandParserSync,help_z},
	//{'z',CommandParserSyncFromRTC,help_zsyncfromrtc},
	//{'Y',CommandParserTestSync,help_y},

#if BUILD_BLUETOOTH==1
#if DBG_RN41TERMINAL==1
	{'R',CommandParserBT,help_r},
#endif
#endif
	//{'L',CommandParserLCD,help_l},
	
#if DBG_TIMERELATEDTEST==1
	{'t', CommandParserTime_Test,help_ttest},
#endif
	
	{'S', CommandParserSampleSound,"S[,<mode>[,<frame>[,<logfile>[,<duration>]]]: Sound streaming/logging. No parameters to list modes.\n\t\tUse logfile=-1 for no logging. Duration is seconds."},
	{'s', CommandParserAudio,help_audio},

	{'A', CommandParserADC,help_a},
	{'a', CommandParserADCTest,help_a_test},
	{'M', CommandParserMotion,help_M},
	{'m', CommandParserMPUTest,help_m},
	{'n', CommandParserMPUTestN,help_m},
	{'U', CommandParserSampleMultimodal,"U,[<mode>,<adcmask>,<adcperiod>[[,<logfile>[,<duration>]]]: Multimodal streaming/logging.\n\t\tadcmask and adcperiod are ignored if the mode does not include ADC.\n\t\tUse logfile=-1 for no logging. Duration is seconds."},
	//{'C', CommandParserClock,help_c},
	//{'V', CommandParserDemo,help_demo},
	//{'B', CommandParserBench,help_b},
	//{'I', CommandParserIO,help_i},
	{'I', CommandParserInterface,help_interface},

	//{'G', CommandParserMotionRecog,help_g},
	{'W', CommandParserSwap,help_w},
	//{'O', CommandParserOff,help_O},
	//{'o', CommandParserOffPower,help_o},
	{'F', CommandParserStreamFormat,help_f},
	{'i', CommandParserInfo,help_info},
	{'D', CommandParserModeDAC,"D[,sr,<frq0>,<vol0>[,<frq1>,<vol1>,...]]: DAC cosine waveform generation. sr: sample rate in Hz; vol: 0 to 4096; frqn: frequency in Hz\n\t\tNo parameters stops. Maximum sample rate is about 250KHz for 1 wavefom."},
	{'d', CommandParserModeDACTest,"DAC test"},
#if ENABLEMODECOULOMB==1
	{'C', CommandParserCoulomb,help_coulomb},
#endif
	{'Q', CommandParserBatteryInfoLong,help_batterylong},
	{'q', CommandParserBatteryInfo,help_battery},
	{'c', CommandParserCallback,help_callback},
	{'X', CommandParserSD,help_sd},
	//{'p', CommandParserPowerTest,help_powertest},
	//{'S', CommandParserTeststream,help_s},
	{'b', CommandParserBootScript,help_bootscript},
	{'?', CommandParserIdentify,help_identify},
	{'~', CommandParserClearBootCounter,help_clearbootctr},
	//{'x', CommandParserx,help_x}
	{'E',CommandParserEEPROM,help_eeprom},
	//{'K',CommandParserBenchmark,help_benchmark},
	{'u',CommandParserUSB,help_usbreinit},
	{'.',CommandParserInterrupts,help_interrupts},
	{'2',CommandParserI2CTest,help_i2c},
	//{'f',CommandParserFatTest,help_fat},
	{'p',CommandParserSleep,help_sleep},
	{'G',CommandParserCPUReg,help_cpureg},

	{'w',CommandParserWait,"w,<delay> waits for the specified delay in ms"},


	{'k', CommandParserBenchmarkCPU,help_benchmark_cpu},


};
const unsigned char CommandParsersIdleNum=sizeof(CommandParsersIdle)/sizeof(COMMANDPARSER);

unsigned char CommandParserClock(char *buffer,unsigned char size)
{
	//CommandChangeMode(APP_MODE_CLOCK);
		
	return 0;
}
unsigned char CommandParserDemo(char *buffer,unsigned char size)
{
	//CommandChangeMode(APP_MODE_DEMO);
		
	return 0;
}

/******************************************************************************
	function: mode_run
*******************************************************************************
	Run a specified interactive mode

	Parameters:
		identifier			-		Identifier displayed on entering and leaving a mode
		CommandParsers		-		Available commands
		CommandParsersNum	-		Number of available commands

	Returns:
		-
******************************************************************************/
void mode_run(const char *identifier,const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum)
{
	fprintf(file_pri,"%s>\n",identifier);
	unsigned long ctr=0;
	while(1)
	{
		// Check if sleep
		if(__mode_sleep)
			__WFI();

		// Wakeup counter increment
		ctr++;
		// Check if command to parse
		unsigned char rv = CommandProcess(CommandParsers,CommandParsersNum);
		if(rv)
		{
			fprintf(file_pri,"Wakeup counter: %lu\n",ctr);
			ctr=0;
		}
		if(CommandShouldQuit())
			break;

	}
	fprintf(file_pri,"<%s\n",identifier);
}


void mode_main(void)
{
	mode_run("IDLE",CommandParsersIdle,CommandParsersIdleNum);
}



