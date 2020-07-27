#ifndef __MODE_IDLE
#define __MODE_IDLE

#include "command.h"

void mode_run(const char *identifier,const COMMANDPARSER *CommandParsers,unsigned char CommandParsersNum);

void mode_main(void);
unsigned char CommandParserClock(char *buffer,unsigned char size);
unsigned char CommandParserDemo(char *buffer,unsigned char size);

#endif
