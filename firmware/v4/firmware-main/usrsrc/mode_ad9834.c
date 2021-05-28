/*
 * mode_ad9834.c
 *
 *  Created on: 28 mai 2021
 *      Author: droggen
 */



#include <stdio.h>
#include <string.h>
#include "main.h"

#include "global.h"
#include "mode_main.h"
#include "command.h"
#include "commandset.h"
#include "mode_ad9834.h"
#include "mode.h"
#include "wait.h"
#include "helper.h"


/*
	AD9834 interface on BlueSense4 v4:
		Reset:		X_5				PC6
		FSEL:		X_3_ADC3		PC4
		PSEL:		X_2_ADC2		PC3

		FSYNC:		X_SAIA2_FS_NSS	PB12
		SDATA:		X_SAIA2_SD_MOSI	PB15
		SCLK:		X_SAIA2_SCK		PB13

	SPI interface:
		Idle: FSYNC=1; SCK = 1. SCK could be idle in any state; but must be 1 before clearing FSYNC
		Data outputted: on SCK rising/high
		Data read: on falling SCK

*/


const COMMANDPARSER CommandParsersAD9834[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'I', CommandParserAD9834InitPort,"Init I/O ports"},
	{'R', CommandParserAD9834Reset,"R,<0|1> Sets reset pin"},
	{'F', CommandParserAD9834Frq,"F,<0|1> Sets frequency pin"},
	{'P', CommandParserAD9834Phase,"P,<0|1> Sets phase pin"},
	{'W', CommandParserAD9834Write,"W,<word> Writes the 16-bit <word>"},
	{'1', CommandParserAD9834Test1,"Test 1"},
};
const unsigned char CommandParsersAD9834Num=sizeof(CommandParsersAD9834)/sizeof(COMMANDPARSER);

unsigned char CommandParserModeAD9834(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	// Change to modulation mode
	CommandChangeMode(APP_MODE_AD9834);
	return 0;
}

void mode_ad9834(void)
{
	mode_run("AD9834",CommandParsersAD9834,CommandParsersAD9834Num);
}


unsigned char CommandParserAD9834InitPort(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	AD9834InitPort();






	return 0;
}
unsigned char CommandParserAD9834Reset(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>1)
		return 2;


	fprintf(file_pri,"Reset: %d\n",x);
	if(x)
		HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);

	return 0;
}
unsigned char CommandParserAD9834Frq(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>1)
		return 2;



	fprintf(file_pri,"Frq: %d\n",x);

	if(x)
		HAL_GPIO_WritePin(X_3_ADC3_GPIO_Port, X_3_ADC3_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(X_3_ADC3_GPIO_Port, X_3_ADC3_Pin, GPIO_PIN_RESET);


	return 0;
}
unsigned char CommandParserAD9834Phase(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>1)
		return 2;




	fprintf(file_pri,"Phase: %d\n",x);

	if(x)
		HAL_GPIO_WritePin(X_2_ADC2_GPIO_Port, X_2_ADC2_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(X_2_ADC2_GPIO_Port, X_2_ADC2_Pin, GPIO_PIN_RESET);

	return 0;
}
unsigned char CommandParserAD9834Write(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>65535)
		return 2;


	fprintf(file_pri,"Write: %04X (%05d)\n",x,x);

	AD9834Write(x);

	return 0;
}
void AD9834InitPort(void)
{
	// Initialise PC3 (PSEL), PC4 (FSEL), PC6 (RST) as output push-pull
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = X_2_ADC2_Pin|X_3_ADC3_Pin|X_5_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);		// All are on GPIOC

	// Write 0 to all 3 pins
	HAL_GPIO_WritePin(X_2_ADC2_GPIO_Port, X_2_ADC2_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(X_3_ADC3_GPIO_Port, X_3_ADC3_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);


	// Initialise the SPI interface - we use a software interface as speed isn't critical
	GPIO_InitStruct.Pin = X_SAIA2_FS_NSS_Pin|X_SAIA2_SD_MOSI_Pin|X_SAIA2_SCK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);		// All are on GPIOC

	// Write idle to all 3 pins
	HAL_GPIO_WritePin(X_SAIA2_FS_NSS_GPIO_Port, X_SAIA2_FS_NSS_Pin, GPIO_PIN_SET);				// FSYNC: Idle is 1
	HAL_GPIO_WritePin(X_SAIA2_SD_MOSI_GPIO_Port, X_SAIA2_SD_MOSI_Pin, GPIO_PIN_RESET);			// MOSI: Idle is x
	HAL_GPIO_WritePin(X_SAIA2_SCK_GPIO_Port, X_SAIA2_SCK_Pin, GPIO_PIN_SET);					// SCK: Idle is 1
}
void AD9834Write(unsigned int word)
{
	// Writes the 16-bit word
	// Assumes IDLE state on start and end, i.e. FSYNC=1, SCK=1


	unsigned t1 = timer_ms_get();
	for(int i=0;i<1000;i++)
		timer_us_wait(10);
	unsigned t2 = timer_ms_get();
	fprintf(file_pri,"%d\n",t2-t1);

	// Clear FSYNC
	HAL_GPIO_WritePin(X_SAIA2_FS_NSS_GPIO_Port, X_SAIA2_FS_NSS_Pin, GPIO_PIN_RESET);
	for(int i=0;i<16;i++)
	{
		// Write data & wait
		unsigned d = (word&(1<<(15-i))) ? 1:0;		// Get bit i, first bit is bit 15
		fprintf(file_pri,"%d ",d);
		HAL_GPIO_WritePin(X_SAIA2_SD_MOSI_GPIO_Port, X_SAIA2_SD_MOSI_Pin, d);
		timer_us_wait(50);

		// Lower clock & wait
		HAL_GPIO_WritePin(X_SAIA2_SCK_GPIO_Port, X_SAIA2_SCK_Pin, GPIO_PIN_RESET);
		timer_us_wait(50);


		// Raise clock
		HAL_GPIO_WritePin(X_SAIA2_SCK_GPIO_Port, X_SAIA2_SCK_Pin, GPIO_PIN_SET);

	}
	// Wait and set FSYNC
	timer_us_wait(50);
	HAL_GPIO_WritePin(X_SAIA2_FS_NSS_GPIO_Port, X_SAIA2_FS_NSS_Pin, GPIO_PIN_SET);

	fprintf(file_pri,"\n");
}

unsigned char CommandParserAD9834Test1(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	fprintf(file_pri,"Test 1\n");

	AD9834InitPort();


	// Initialise the control word
	AD9834Write(0b0010001100101000);

	// Reset
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);

	// Set some frequency. 1Hz = 4
	AD9834Write(0b0100000000000100);		// FRQ0 LSB
	AD9834Write(0b0100000000000000);		// FRQ0 MSB

	// 2Hz = 8
	AD9834Write(0b1000000000001000);		// FRQ1 LSB
	AD9834Write(0b1000000000000000);		// FRQ1 MSB

	// Phase
	AD9834Write(0b1100000000000000);		// PH0: 0deg
	AD9834Write(0b1110100000000000);		// PH1: 180deg


	return 0;
}


