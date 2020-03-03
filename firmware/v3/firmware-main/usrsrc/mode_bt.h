#ifndef __MODE_BT_H
#define __MODE_BT_H

#if BUILD_BLUETOOTH==1

#include "command.h"


void mode_bt(void);
unsigned char CommandParserBT(char *buffer,unsigned char size);

#endif

#endif

