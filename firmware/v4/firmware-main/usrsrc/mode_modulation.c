/*
 * mode_modulation.c
 *
 *  Created on: 10 oct. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include "global.h"
#include "mode_main.h"
#include "command.h"
#include "commandset.h"
#include "mode.h"

const COMMANDPARSER CommandParsersModulation[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
};
const unsigned char CommandParsersModulationNum=sizeof(CommandParsersModulation)/sizeof(COMMANDPARSER);


void mode_modulation(void)
{
	mode_run("MODULATION",CommandParsersModulation,CommandParsersModulationNum);
}

unsigned char CommandParserModeModulation(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	// Change to modulation mode
	CommandChangeMode(APP_MODE_MODULATION);
	return 0;
}
