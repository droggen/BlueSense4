/*
 * command_usb.c
 *
 *  Created on: 23 sept. 2019
 *      Author: droggen
 */

#include "main.h"
#include "usrmain.h"
#include "global.h"
#include "system.h"

/******************************************************************************
	function: CommandParserUSB
*******************************************************************************


	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid

******************************************************************************/
unsigned char CommandParserUSB(char *buffer,unsigned char size)
{
	unsigned b;
	unsigned bo= system_buttonpress();
	unsigned usbisinit = 1;
	while(1)
	{
		// Debouncing
		b = system_buttonpress();
		if(b^bo && b)
		{
			// Button has changed
			fprintf(file_pri,"Button press\n");
			if(usbisinit)
			{
				fprintf(file_pri,"Disabling usb\n");
				// Do disable here
				usbisinit=0;
			}
			else
			{
				fprintf(file_pri,"Enabling usb\n");
				// Do enable here
				usbisinit=1;
			}
		}
		bo = b;

		HAL_Delay(100);
	}

	return 0;
}

