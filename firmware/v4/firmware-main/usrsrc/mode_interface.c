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


const COMMANDPARSER CommandParsersInterface[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},


};
const unsigned char CommandParsersInterfaceNum=sizeof(CommandParsersInterface)/sizeof(COMMANDPARSER);


void mode_interface(void)
{
	mode_run("INTF",CommandParsersInterface,CommandParsersInterfaceNum);
}


