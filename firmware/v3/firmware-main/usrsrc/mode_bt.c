#if BUILD_BLUETOOTH==1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
#include "serial.h"
#include "i2c.h"
#include "rn41.h"
#include "wait.h"
//#include "init.h"
#include "uiconfig.h"
#include "helper.h"
//#include "i2c_internal.h"
#include "system.h"
#include "mode.h"
#include "commandset.h"
#include "serial_uart.h"


#if DBG_RN41TERMINAL==1

#define _delay_ms HAL_Delay


const COMMANDPARSER CommandParsersBT[] =
{ 
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit}
};
const unsigned char CommandParsersBTNum=sizeof(CommandParsersBT)/sizeof(COMMANDPARSER);



unsigned char CommandParserBT(char *buffer,unsigned char size)
{	
	
	CommandChangeMode(APP_MODE_BT);
	
	return 0;
}
void help(void)
{
	fprintf(file_usb,"The following commands are available:\n");
	fprintf(file_usb,"\t111: Set speed to 115.2kbps\n");
	fprintf(file_usb,"\t222: Set speed to 230.4kbps\n");
	fprintf(file_usb,"\t444: Set speed to 460.8kbps\n");
	fprintf(file_usb,"\t999: Set speed to 921.6kbps\n");
	fprintf(file_usb,"\t555: Set speed to 57.6kbps\n");
	fprintf(file_usb,"\tRRR: BT hardware reset\n");
	fprintf(file_usb,"\tIII: BT initialisation\n");
	fprintf(file_usb,"\tCCC: Clear event counters\n");
	fprintf(file_usb,"\tEEE: Report data overrun, errors, events in interrupts\n");
	fprintf(file_usb,"\trrr: Print USART registers\n");
	fprintf(file_usb,"\t???: Command help\n");
	fprintf(file_usb,"\tXXX: Exit\n");
}

void mode_bt(void)
{
	unsigned char cmdchar=0;
	unsigned char cmdn=0;
	unsigned char shouldrun=1;
	int c;
	
	fprintf(file_usb,"bt mode\n");
	//fprintf(file_pri,PSTR("bt mode\n"));
	help();	
	
	// Switch mode to buffer even if remote not connected
	fbufferwhendisconnected(file_bt,1);

	while(shouldrun)
	{
		//while(CommandProcess(CommandParsersBench,CommandParsersBenchNum));		
		//if(CommandShouldQuit())
			//break;
			
		if((c=fgetc(file_usb))!=-1)
		//if((c=fgetc(file_pri))!=-1)
		{
			cmddecodestart:
			if( (c=='1' || c=='2'|| c=='4' || c=='5'  || c=='9' || c=='C' || c=='c' || c=='R' || c=='r' || c=='E' || c=='e' || c=='X' || c=='x' || c=='?' || c=='I' || c=='i') || cmdn)
			{
				
				if(cmdn==0)
				{
					//fprintf(file_usb,PSTR("<first char>\n"));
					cmdchar=c;
					cmdn++;
				}
				else
				{
					if(c==cmdchar)
					{
						cmdn++;
						//fprintf(file_usb,PSTR("<%d char in sequence>\n"),cmdn);
						if(cmdn==3)
						{
							// command recognised
							switch(cmdchar)
							{
								case '1':
									fprintf(file_usb,"<Setting serial to 115.2kbps>\n");
									serial_uart_init_ll(115200);
									break;
								case '2':
									fprintf(file_usb,"<Setting serial to 230.4kbps>\n");
									serial_uart_init_ll(230400);
									break;
								case '4':
									fprintf(file_usb,"<Setting serial to 460.8kbps>\n");
									serial_uart_init_ll(460800);
									break;
								case '5':
									fprintf(file_usb,"<Setting serial to 57.6kbps>\n");
									serial_uart_init_ll(57600);
									break;
								case '9':
									fprintf(file_usb,"<Setting serial to 921.6kbps>\n");
									serial_uart_init_ll(921600);
									break;
								case 'R':
									fprintf(file_usb,"<Resetting BT>\n");
									rn41_Reset(file_usb);
									break;
								case 'r':
									fprintf(file_usb,"<USART registers>\n");
									serial_uart_printreg(file_usb);
									break;
								case 'X':
								case 'x':
									fprintf(file_usb,"<Exit>\n");
									//fprintf(file_pri,PSTR("<Exit>\n"));
									shouldrun=0;
									break;
								case 'C':
								case 'c':
									fprintf(file_usb,"<Clear event counters>\n");
									Serial1DOR = 0;
									Serial1NR = 0;
									Serial1PE = 0;
									Serial1FE = 0;
									Serial1TXE = 0;
									Serial1RXNE = 0;
									Serial1EvtWithinInt = 0;
									Serial1Int=0;
									break;
								case 'e':
								case 'E':
									serial_uart_printevents(file_usb);
									break;
								case 'i':
								case 'I':
								{
									char devname[64];
									fprintf(file_usb,"Initialising RN41\n");
									rn41_Setup(file_usb,file_bt,devname);
									fprintf(file_usb,"devname: '%s'\n",devname);
									break;
								}
								case '?':
								default:
									help();
									break;
							}
							cmdn=0;
						}						
					}
					else
					{
						//fprintf(file_usb,PSTR("<%d char in sequence (%02x), current not similar (%02x)>\n"),cmdn,cmdchar,c);
						// Not same as previous characters, output previous and current
						for(unsigned char i=0;i<cmdn;i++)
							fputc(cmdchar,file_bt);
						cmdn=0;
						goto cmddecodestart;					
					}
				}
			}
			else
			{
				//fprintf(file_usb,"send %02x\n",c);
				fputc(c,file_bt);
				_delay_ms(10);
			}
		}
		//else
			//fputc('.',file_usb);
		while((c=fgetc(file_bt))!=-1)
		{
			fputc(c,file_usb);
		}
		//fprintf(file_usb,"%d\n",buffer_level(&SerialData1Rx));
		
		//_delay_ms(5);
		//_delay_ms(1);

	}
	fprintf(file_pri,"bt end\n");
	fbufferwhendisconnected(file_bt,0);				// Stop buffering when disconnected

}

#endif
#endif
