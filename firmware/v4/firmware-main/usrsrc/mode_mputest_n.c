/*
 * mode_mputest.c


 *
 *  Created on: 23 sept. 2019
 *      Author: droggen
 */

#include "global.h"
#include "command.h"
#include "commandset.h"
#include "command_mputest.h"

/*
Pin mapping:
PA15:	motion_int
PA4: SS
PA5: SCK
PA6: MISO
PA7: MOSI

MPU on SPI1

*/


const char help_mput_test[] ="MPU tests";
const char help_mput_ss[] ="S,[0|1]		clear (0) or set (1) SS# signal";
const char help_mput_poweron[] = "Turn on the LDO regulator for the MPU";
const char help_mput_poweroff[] = "Turn off the LDO regulator for the MPU";
const char help_mput_xchange[] = "X,<d0>[,<d1>,<d2>...] Exchange data on the SPI interface";
const char help_mput_pm[] = "print modes";
const char help_mput_powerosc[] = "Toggle VCC1.8 at 0.25Hz";
const char help_mput_ssosc[] = "Toggle SS# at 0.25Hz";
const char help_mput_printspireg[] = "Print the SPI configuration registers";
const char help_mput_printmpureg[] = "Print the MPU registers";
const char help_mput_initcube[] = "Initialisation with cube code";
const char help_mput_readregspoll[] = "r,<regstart>[,<n>] Read 1 or n MPU registers starting from regstart using polling";
const char help_mput_readregsint[] = "r,<regstart>[,<n>] Read 1 or n MPU registers starting from regstart using interrupts";
const char help_mput_writeregspoll[] = "W,<startreg>,<val0>,<val1>,... Write values to MPU registers starting from startreg";

const COMMANDPARSER CommandParsersMPUTestN[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'O',CommandParserMPUTestPoweron,help_mput_poweron},
	{'o',CommandParserMPUTestPoweroff,help_mput_poweroff},
	{'P',CommandParserMPUTestPowerosc,help_mput_powerosc},
	{'S',CommandParserMPUTestSSosc,help_mput_ssosc},
	{'G',CommandParserMPUTestPrintSPIRegisters,help_mput_printspireg},
	{'g',CommandParserMPUTestPrintMPURegisters,help_mput_printmpureg},
	{'R',CommandParserMPUTestReadRegistersPoll,help_mput_readregspoll},
	{'W',CommandParserMPUTestWriteRegistersPoll,help_mput_writeregspoll},
	{'r',CommandParserMPUTestReadRegistersInt,help_mput_readregsint},
	//{'I',CommandParserMPUInitSPICube,help_mput_initcube},
	{'S',CommandParserMPUSS,help_mput_ss},
	{'M',CommandParserMPUTestPrintModes,help_mput_pm},
	{'X',CommandParserMPUTestXchange,help_mput_xchange},
	{'1',CommandParserMPUTest1,help_mput_test},
	{'2',CommandParserMPUTest2,help_mput_test},
	{'3',CommandParserMPUTest3,help_mput_test},
	{'4',CommandParserMPUTest4,help_mput_test},
	{'5',CommandParserMPUTest5,help_mput_test},
	{'q',CommandParserMPUInit1,help_mput_test},
	{'w',CommandParserMPUInit2,help_mput_test},
	{'e',CommandParserMPUInit3,help_mput_test},
	{'r',CommandParserMPUInit4,help_mput_test},


};
const unsigned char CommandParsersMPUTestNNum=sizeof(CommandParsersMPUTestN)/sizeof(COMMANDPARSER);



void mode_mputest_n(void)
{

	fprintf(file_pri,"MPUTST>\n");


	while(1)
	{


		//fprintf(file_pri,"eeprom: call command process\n");

		while(CommandProcess(CommandParsersMPUTestN,CommandParsersMPUTestNNum));

		if(CommandShouldQuit())
			break;


		//HAL_Delay(500);

	}
	fprintf(file_pri,"<MPUTST\n");
}


