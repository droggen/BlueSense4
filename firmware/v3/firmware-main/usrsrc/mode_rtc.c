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
#include "m24xx.h"
#include "helper.h"
#include "command_datetime.h"

//const char help_eepromread[] ="R[,<addrfrom>[,addrto]]: Read all the EEPROM, or at/from <addrfrom> until <addrto>";
const char help_rtc_mwritebackupreg[] ="W,<addr>,<word>: Write 32-bit <word> in backup register <addr>";
const char help_rtc_writereg[] ="w,<addr>,<word>: Write 32-bit <word> at <addr>";
const char help_rtc_readbackupreg[] ="Read RTC backup registers";
const char help_rtc_readallregs[] ="Read all RTC registers";
const char help_rtc_test[] ="Test RTC functions";
const char help_rtc_init[] ="Init RTC";
const char help_rtc_datetime[] = "Show date/time";
const char help_rtc_datetimems[] = "Show time ms/us";
const char help_rtc_bypass[] ="b,<0|1> disable/enable bypass";
const char help_rtc_protect[] ="p[,<0|1>] get protection or disable/enable protection";
const char help_rtc_prescaler[] = "P[,<async>,<sync>]: get or set prescalers";
const char help_rtc_inittimefromrtc[] = "Init system time from RTC";


const COMMANDPARSER CommandParsersRTC[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'R', CommandParserRTCReadBackupRegisters, help_rtc_readbackupreg},
	{'r',CommandParserRTCReadAllRegisters,help_rtc_readallregs},
	//{'R', CommandParserEEPROMRead,help_eepromread},
	{'W', CommandParserRTCWriteBackupRegister,help_rtc_mwritebackupreg},
	{'w', CommandParserRTCWriteRegister,help_rtc_writereg},
	{'T',CommandParserRTCShowDateTime,help_rtc_datetime},
	{'t',CommandParserRTCShowDateTimeMs,help_rtc_datetimems},
	{'I', CommandParserRTCInit, help_rtc_init},
	{'i', CommandParserRTCInitTimeFromRTC, help_rtc_inittimefromrtc},
	{'1',CommandParserRTCTest1,help_rtc_test},
	{'2',CommandParserRTCTest2,help_rtc_test},
	{'3',CommandParserRTCTest3,help_rtc_test},
	{'4',CommandParserRTCTest4,help_rtc_test},
	{'b',CommandParserRTCBypass,help_rtc_bypass},
	{'P',CommandParserRTCPrescaler,help_rtc_prescaler},
	{'p',CommandParserRTCProtect,help_rtc_protect},

};
const unsigned char CommandParsersRTCNum=sizeof(CommandParsersRTC)/sizeof(COMMANDPARSER);



void mode_rtc(void)
{

	fprintf(file_pri,"RTC>\n");


	while(1)
	{


		//fprintf(file_pri,"eeprom: call command process\n");

		while(CommandProcess(CommandParsersRTC,CommandParsersRTCNum));

		if(CommandShouldQuit())
			break;


		//HAL_Delay(500);

	}
	fprintf(file_pri,"<RTC\n");
}


