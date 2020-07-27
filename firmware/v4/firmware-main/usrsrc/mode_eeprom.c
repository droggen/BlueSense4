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


const char help_eepromread[] ="R[,<addrfrom>[,addrto]]: Read EEPROM at <addrfrom>, or from <addrfrom> to <addrto>";
const char help_eepromwrite[] ="W,<addr>,<byte>: Write <byte> at <addr>";


const COMMANDPARSER CommandParsersEEPROM[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'R', CommandParserEEPROMRead,help_eepromread},
	{'W', CommandParserEEPROMWrite,help_eepromwrite},

};
const unsigned char CommandParsersEEPROMNum=sizeof(CommandParsersEEPROM)/sizeof(COMMANDPARSER);

unsigned char CommandParserEEPROMRead(char *buffer,unsigned char size)
{
	int addrfrom,addrto;

	int n = ParseCommaGetNumParam(buffer);

	switch(n)
	{
		case 0:
			addrfrom = 0;
			addrto = 16384;
			break;
		case 1:
			if(ParseCommaGetInt(buffer,1,&addrfrom))
				return 2;
			addrto = addrfrom+1;
			break;
		case 2:
			if(ParseCommaGetInt(buffer,2,&addrfrom,&addrto))
				return 2;
			break;
		default:
			return 2;
	}

	fprintf(file_pri,"EEPROM read from address %X to %X\n",addrfrom,addrto);

	m24xx_printreg(file_pri,addrfrom,addrto);

	return 0;
}
unsigned char CommandParserEEPROMWrite(char *buffer,unsigned char size)
{
	int addr,byte;


	if(ParseCommaGetInt(buffer,2,&addr,&byte))
		return 2;

	fprintf(file_pri,"EEPROM write of %X at address %X\n",byte,addr);

	m24xx_write(addr,byte);

	return 0;
}

void mode_eeprom(void)
{

	fprintf(file_pri,"EEPROM>\n");


	while(1)
	{


		//fprintf(file_pri,"eeprom: call command process\n");

		while(CommandProcess(CommandParsersEEPROM,CommandParsersEEPROMNum));

		if(CommandShouldQuit())
			break;


		//HAL_Delay(500);

	}
	fprintf(file_pri,"<EEPROM\n");
}


