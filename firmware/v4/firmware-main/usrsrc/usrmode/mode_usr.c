#include <stdio.h>
#include <string.h>

#include "main.h"
#include "global.h"
#include "wait.h"
#include "system.h"
#include "system-extra.h"
#include "helper.h"
#include "commandset.h"
#include "uiconfig.h"
#include "mode.h"
#include "mode_main.h"
#include "mode_usr.h"


// "User mode" for testing of functions

const COMMANDPARSER CommandParsersUsr[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},

	{0,0,"---- General functions  ----"},

	{'1', CommandParserUSRTest1,"Test function 1"},


	{0,0,"---- Other functions  ----"},
	{'2', CommandParserUSRTest2,"Test function 2"},

};
unsigned char CommandParsersUsrNum=sizeof(CommandParsersUsr)/sizeof(COMMANDPARSER);


unsigned char CommandParserModeUser(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	CommandChangeMode(APP_MODE_USER);
	return 0;
}


void mode_user(void)
{
	mode_run("USR",CommandParsersUsr,CommandParsersUsrNum);
}



unsigned char CommandParserUSRTest1(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"Test function 1\n");


	return 0;
}
unsigned char CommandParserUSRTest2(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"Test function 2\n");


	return 0;
}
