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
#include "mode_serial.h"
#include "mode_modulation.h"

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
	{0,0,"---- RTC ----"},
	{'T', CommandParserTime,help_t},
	{'t', CommandParserDate,help_d},
	{'Z',CommandParserSync,help_z},
#if 1
	{'R', CommandParserRTC,help_rtc},
	{'r', CommandParserRTCExt,"External RTC functions"},
#endif
	{0,0,"---- Power ----"},
	{'Q', CommandParserBatteryInfoLong,help_batterylong},
	{'q', CommandParserBatteryInfo,help_battery},
	{'O', CommandParserOff,help_O},
	{'o', CommandParserOffPower,help_o},
	//{'t', CommandParserTimeTest,help_timetest},
	//{'r', CommandParserRTCTest,help_rtctest},

	//{'1', CommandParser1,help_1},



	//{'z',CommandParserSyncFromRTC,help_zsyncfromrtc},
	//{'Y',CommandParserTestSync,help_y},

	//{'L',CommandParserLCD,help_l},
	
	
	{0,0,"---- Sound ----"},
	{'S', CommandParserSampleSound,"S[,<mode>[,<left_right>[,<logfile>[,<duration>]]]: Sound streaming/logging.\n\t\tNo parameters to list modes.\n\t\tLogs to <logfile> (use -1 not to log) and runs for the specified duration.\n\t\tleft_right: 0=left, 1=right, 2=stereo."},
	{'s', CommandParserAudio,help_audio},
	{0,0,"---- ADC ----"},
	{'A', CommandParserADC,"A,<mask>,<period>,[fastbin,[<logfile>[,<duration>]]]: ADC sampling/logging.\n\t\tInterrupt with keypress or !.\n\t\tmask: ADC channel bitmask in decimal. Bits 0 to 4=ext channel (bit 0 is ADC0, ...), bit 5=vbat, 6=vref, 7=temp)\n\t\tperiod: sample period in microseconds.\n\t\tfastbin=1: streams in frameless 8-bit binary (use only 1 channel to allow decoding).\n\t\tfastbin=2: binary \"D;s\" 16-bit format with 1 byte frame.\n\t\tLogs to <logfile> (use -1 not to log) and runs for the specified duration."},
	{'a', CommandParserADCTest,help_a_test},
	{0,0,"---- Motion ----"},
	{'M', CommandParserMotion,help_M},
	{'m', CommandParserMPUTest,help_m},
	{'n', CommandParserMPUTestN,help_m},
	{0,0,"---- Multimodal ----"},
	{'U', CommandParserSampleMultimodal,"U,[<mode>,<stereo>,<adcmask>,<adcperiod>[[,<logfile>[,<duration>]]]: Multimodal streaming/logging.\n\t\t<stereo> is ignored if the mode does not include sound. 0=left; 1=right; 2=stereo.\n\t\t<adcmask> and <adcperiod> are ignored if the mode does not include ADC.\n\t\tLogs to <logfile> (use -1 not to log) and runs for the specified duration."},
	{0,0,"---- DAC ----"},
	{'D', CommandParserModeDAC,"D[,sr,<frq0>,<vol0>[,<frq1>,<vol1>,...]]: DAC cosine waveform generation.\n\t\tsr: sample rate in Hz; vol: 0 to 4096; frqn: frequency in Hz\n\t\tNo parameters stops. Maximum sample rate is about 250KHz for 1 wavefom."},
	{'d', CommandParserModeDACTest,"Additional DAC functions"},



	//{'C', CommandParserClock,help_c},
	//{'V', CommandParserDemo,help_demo},
	//{'B', CommandParserBench,help_b},
	//{'I', CommandParserIO,help_i},

	{0,0,"---- SD Card ----"},
	{'I', CommandParserInterface,help_interface},
	{'X', CommandParserSD,help_sd},
	{'Y', CommandParserSDYModemSendLog,"Y,<logid0>[,<logid1>[,<logid2>...]] Send log files, specified by their number, by YMODEM to the host"},
	{0,0,"---- Various ----"},
	{'F', CommandParserStreamFormat,help_f},
	//{'G', CommandParserMotionRecog,help_g},
	{'W', CommandParserSwap,help_w},
	//{'p', CommandParserPowerTest,help_powertest},
	//{'S', CommandParserTeststream,help_s},
	{'b', CommandParserBootScript,help_bootscript},
	{'~', CommandParserClearBootCounter,help_clearbootctr},
	{'?', CommandParserIdentify,help_identify},
	{'V',CommandParserVT100,"V[,<en>] Queries the VT100 mode; or enables (en=1) or disables (en=0) it"},
	{0,0,"---- Development/Test ----"},
	{'K', CommandParserBenchmark,"Benchmarks"},

#if 1		// Deactivated for the vidoe demo
	//{'x', CommandParserx,help_x}
	{'E',CommandParserEEPROM,help_eeprom},
	{'u',CommandParserUSB,help_usbreinit},
	{'.',CommandParserInterrupts,help_interrupts},
	{'2',CommandParserI2CTest,help_i2c},
	//{'f',CommandParserFatTest,help_fat},
	{'P',CommandParserPeriphPower,"P,0|1: disable/enable BT&SD power"},
	{'p',CommandParserSleep,help_sleep},
	{'G',CommandParserCPUReg,help_cpureg},

	{'w',CommandParserWait,"w,<delay> waits for the specified delay in ms"},
	{'9',CommandParserRamp,"9,<delay>,<from>,<lowerthan>,<step> Generates a ramp signal every delay ms starting at from by increment of step"},
	{'8',CommandParserRandomWalk,"8,<delay>,<from>,<to>,<jump> Generates a random walk between from and to, jumping by up to jump every delay ms"},

	//{'§',CommandTestPwr,"test prw stuff"},


	{'-', CommandParserModeSerial,"Serial test mode"},
	{',', CommandParserModeSerialReset,"Serial reset"},
	{':', CommandParserDump,"Serial last"},
	{'_', CommandParserSerInfo,"Serial info"},

#if ENABLEMODECOULOMB==1
	{'C', CommandParserCoulomb,help_coulomb},
#endif
#if BUILD_BLUETOOTH==1
#if DBG_RN41TERMINAL==1
	{'B',CommandParserBT,help_r},
#endif
#endif
#if DBG_TIMERELATEDTEST==1
	{'t', CommandParserTime_Test,help_ttest},
#endif

	{'i', CommandParserInfo,help_info},
	{'c', CommandParserCallback,help_callback},
	{'L', CommandParserModeModulation,"Modulation"},
#endif

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
			//fprintf(file_pri,"Wakeup counter: %lu\n",ctr);
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



