/*
 * mode_serial.c
 *
 *  Created on: 1 sept. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include "global.h"
#include "mode_main.h"
#include "mode_serial.h"
#include "commandset.h"
#include "mode.h"
#include "wait.h"
#include "serial.h"
#include "serial_uart.h"
#include "rn41.h"
#include "usart.h"
#include "dma.h"

void usart_rx_check(void);


const COMMANDPARSER CommandParsersSerTest[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'1', CommandParserSerTestSendEnter,"Send $$$"},
	{'2', CommandParserSerTestSendLeave,"Send ---"},
	{'Q', CommandParserSerTestSendWithoutConnection,"Enable sending data without connection"},
	{'I', CommandParserSerTestInit,"Init"},
	{'W', CommandParserSerTestSendH,"Send H	"},
	{'R', CommandParserSerTestRead,"Read"},
	{'A', CommandParserSerTestSendCr,"Send CR"},
	{'S', CommandParserSerTestSendNl,"Send NL"},
	{'D', CommandParserSerTestDMAEn,"Enable DMA receive"},
	{'Y', CommandParserSerTestDMARead,"Read DMA"},
	{'X', CommandParserSerTestDMAClear,"Clear DMA"},
	{'T', CommandParserSerTestDMATx,"Transmit with DMA"},
	{'t', CommandParserSerTestDMATx2,"Transmit with DMA 2"},
	{'g', CommandParserSerTestDMATx3,"Restart with DMA"},
	{'E', CommandParserSerTestDMAIsEn,"DMA is en"},
	{'B', CommandParserSerTestWrite,"Write lots"},
	{'b', CommandParserSerTestWrite2,"Write lots 2"},
	/*{'I', CommandParserRTCExtInit, "Init external RTC with default settings"},
	{'R',CommandParserRTCExtReadAllRegisters,"Read all registers"},
	{'r', CommandParserRTCExtReadRegister,"r,<reg>: read register <reg>"},
	{'w', CommandParserRTCExtWriteRegister,"w,<reg>,<val>: write <val> to register <reg>"},
	{'Z', CommandParserRTCExtReadDateTime,"Read date and time"},
	{'z', CommandParserRTCExtWriteDateTime,"z,<hh>,<mm>,<ss>,<dd>,<mm>,<yy>: sets the external RTC to hhmmss ddmmyy"},
	{'X', CommandParserRTCExtSwRst,"X,<rst>: Software reset when rst=1; clear reset when rst=0"},
	{'P', CommandParserRTCExtPollDateTime,"Poll date and time and SQW signal"},
	{'a', CommandParserRTCExtWriteAlarm,"a,<hh>,<mm>,<ss>,<dd>,<mm>,<yy>: sets the external RTC alarm 1 to hhmmss ddmmyy"},
	{'S', CommandParserRTCExtBgdReadStatus,"Read status register in background (interrupt driven)"},
	{'T', CommandParserRTCExtTemp,"Read temperature"},
	{0,0,"---- Development ----"},

	{'1', CommandParserRTCExtTest1,"test 1"},
	{'2', CommandParserRTCExtTest2,"test 2"},*/

};
const unsigned char CommandParsersSerTestNum=sizeof(CommandParsersSerTest)/sizeof(COMMANDPARSER);

/*
 * DMA1, LL_DMA_CHANNEL_6, LL_DMA_REQUEST_2
 */

void mode_serialtest(void)
{
	mode_run("SERTST",CommandParsersSerTest,CommandParsersSerTestNum);
}

unsigned char CommandParserModeSerial(char *buffer,unsigned char size)
{
// Change to serial mode
	CommandChangeMode(APP_MODE_SERIALTEST);
	return 0;
}

unsigned char CommandParserModeSerialReset(char *buffer,unsigned char size)
{
	// reset USB

	serial_usb_initbuffers();
	return 0;
}
unsigned char CommandParserModeSerialLast(char *buffer,unsigned char size)
{


	return 0;
}

unsigned char CommandParserSerTestSendEnter(char *buffer,unsigned char size)
{
	fprintf(file_bt,"$$$");
	return 0;
}
unsigned char CommandParserSerTestSendLeave(char *buffer,unsigned char size)
{
	fprintf(file_bt,"---\r");
	return 0;
}
unsigned char CommandParserSerTestSendH(char *buffer,unsigned char size)
{
	fprintf(file_bt,"H\r");
	return 0;
}
unsigned char CommandParserSerTestInit(char *buffer,unsigned char size)
{
	// Communicate with the BT chip even if not connected remotely
	fbufferwhendisconnected(file_bt,1);


	fprintf(file_usb,"<Setting serial to 115.2kbps>\n");
	serial_uart_changespeed(115200);

	// Reset
	rn41_Reset(file_usb);

	return 0;
}
unsigned char CommandParserSerTestSendWithoutConnection(char *buffer,unsigned char size)
{
	// Communicate with the BT chip even if not connected remotely
	fbufferwhendisconnected(file_bt,1);



	return 0;
}
unsigned char CommandParserSerTestRead(char *buffer,unsigned char size)
{
	/*while(1)
	{
		int c = fgetc(file_bt);
		if(c==-1)
			break;
		fprintf(file_usb,"%c",c);
	}*/
	char r[64];
	int nr;
	do
	{
		nr = fread(r,1,64,file_bt);
		if(nr)
			fwrite(r,1,nr,file_usb);
	}
	while(nr);
	return 0;
}
unsigned char CommandParserSerTestSendCr(char *buffer,unsigned char size)
{
	fprintf(file_bt,"\r");
	return 0;
}

unsigned char CommandParserSerTestSendNl(char *buffer,unsigned char size)
{
	fprintf(file_bt,"\n");
	return 0;
}



unsigned char CommandParserSerTestDMAEn(char *buffer,unsigned char size)
{
	fprintf(file_pri,"Enabling RX DMA\n");

#if 0
	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)&USART2->RDR);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)usart_rx_dma_buffer);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, SERIAL_UART_DMA_RX_BUFFERSIZE);

	LL_USART_Disable(USART2);

    LL_USART_EnableDMAReq_RX(USART2);
    //LL_USART_EnableIT_IDLE(USART2);

	/* Enable HT & TC interrupts */
	LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_6);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_6);


	/* Enable USART and DMA */
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);
	LL_USART_Enable(USART2);		// done by cube
#endif
	// Deactivate
	LL_USART_DeInit(USART2);
	LL_USART_Disable(USART2);


	LL_USART_InitTypeDef USART_InitStruct = {0};


	// USART2 DMA init
	LL_DMA_SetPeriphRequest(DMA1, LL_DMA_CHANNEL_6, LL_DMA_REQUEST_2);
	LL_DMA_SetDataTransferDirection(DMA1, LL_DMA_CHANNEL_6, LL_DMA_DIRECTION_PERIPH_TO_MEMORY);
	LL_DMA_SetChannelPriorityLevel(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PRIORITY_LOW);
	LL_DMA_SetMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MODE_CIRCULAR);
	LL_DMA_SetPeriphIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PERIPH_NOINCREMENT);
	LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MEMORY_INCREMENT);
	LL_DMA_SetPeriphSize(DMA1, LL_DMA_CHANNEL_6, LL_DMA_PDATAALIGN_BYTE);
	LL_DMA_SetMemorySize(DMA1, LL_DMA_CHANNEL_6, LL_DMA_MDATAALIGN_BYTE);
	LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)&USART2->RDR);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_6, (uint32_t)_serial_uart_rx_dma_buffer);
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_6, SERIAL_UART_DMA_RX_BUFFERSIZE);

	// Enable HT & TC interrupts
	LL_DMA_EnableIT_HT(DMA1, LL_DMA_CHANNEL_6);
	LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_6);

	// DMA interrupt init
	NVIC_SetPriority(DMA1_Channel6_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
	NVIC_EnableIRQ(DMA1_Channel6_IRQn);

	/* USART configuration */
	USART_InitStruct.BaudRate = 115200;
	USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
	USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
	USART_InitStruct.Parity = LL_USART_PARITY_NONE;
	USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
	USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
	USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
	ErrorStatus status = LL_USART_Init(USART2, &USART_InitStruct);
	if(status == ERROR)
		fprintf(file_pri,"Error init USART\n");
	LL_USART_ConfigAsyncMode(USART2);
	LL_USART_EnableDMAReq_RX(USART2);
	LL_USART_EnableIT_IDLE(USART2);

	/* USART interrupt */
	NVIC_SetPriority(USART2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(), 0, 0));
	NVIC_EnableIRQ(USART2_IRQn);

	/* Enable USART and DMA */
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_6);
	LL_USART_Enable(USART2);

	return 0;
}


unsigned char CommandParserSerTestDMARead(char *buffer,unsigned char size)
{
	fprintf(file_pri,"DMA buffer:\n");
	for(int i=0;i<SERIAL_UART_DMA_RX_BUFFERSIZE;i++)
		fprintf(file_pri,"%02X ",_serial_uart_rx_dma_buffer[i]);
	fprintf(file_pri,"\n");
	return 0;
}

unsigned char CommandParserSerTestDMAClear(char *buffer,unsigned char size)
{
	fprintf(file_pri,"Clear DMA buffer\n");
	for(int i=0;i<SERIAL_UART_DMA_RX_BUFFERSIZE;i++)
		_serial_uart_rx_dma_buffer[i]=0x55;
	return 0;
}

unsigned char CommandParserSerTestDMATx(char *buffer,unsigned char size)
{
	fprintf(file_pri,"DMA TX\n");

	LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_7);

	for(int i=0;i<DMATXLEN;i++)
		_serial_uart_tx_dma_buffer[i]='0'+i;


	// Configure DMA
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, DMATXLEN);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)_serial_uart_tx_dma_buffer);

	// Clear all flags
	LL_DMA_ClearFlag_TC7(DMA1);
	LL_DMA_ClearFlag_HT7(DMA1);
	LL_DMA_ClearFlag_TE7(DMA1);

	// Start transfer
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
	unsigned long t = timer_us_get();

	fprintf(file_pri,"Start at %ld\n",t);

	return 0;
}

unsigned char CommandParserSerTestDMATx2(char *buffer,unsigned char size)
{
	fprintf(file_pri,"DMA TX\n");

	//LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_7);

	for(int i=0;i<DMATXLEN;i++)
		_serial_uart_tx_dma_buffer[i]='0'+i;


	// Configure DMA
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, DMATXLEN);
	LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_7, (uint32_t)_serial_uart_tx_dma_buffer);

	// Clear all flags
	LL_DMA_ClearFlag_TC7(DMA1);
	LL_DMA_ClearFlag_HT7(DMA1);
	LL_DMA_ClearFlag_TE7(DMA1);

	// Start transfer
	LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
	const int n=20;
	unsigned long t[n];
	unsigned datleft[n];

	for(int i=0;i<n;i++)
	{
		t[i] = timer_us_get();
		datleft[i]=(unsigned)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_7);
		timer_us_wait(250);
		if(i==5)
			//LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_7);
			LL_USART_DisableDMAReq_TX(USART2);

	}
	for(int i=0;i<n;i++)
		fprintf(file_pri,"%d: %ld: %d\n",i,t[i],datleft[i]);

	return 0;
}
unsigned char CommandParserSerTestDMATx3(char *buffer,unsigned char size)
{
	fprintf(file_pri,"DMA TX restart\n");

	// Check if DMA is ongoing
	fprintf(file_pri,"Is en: %d\n",LL_DMA_IsEnabledChannel(DMA1,LL_DMA_CHANNEL_7));

	//LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_7);
	LL_USART_EnableDMAReq_TX(USART2);

	const int n=20;
	unsigned long t[n];
	unsigned datleft[n];

	for(int i=0;i<n;i++)
	{
		t[i] = timer_us_get();
		datleft[i]=(unsigned)LL_DMA_GetDataLength(DMA1, LL_DMA_CHANNEL_7);
		timer_us_wait(250);
	}
	for(int i=0;i<n;i++)
		fprintf(file_pri,"%d: %ld: %d\n",i,t[i],datleft[i]);

	return 0;
}
unsigned char CommandParserSerTestDMAIsEn(char *buffer,unsigned char size)
{
	// Check if DMA is ongoing
	fprintf(file_pri,"Is en: %d\n",LL_DMA_IsEnabledChannel(DMA1,LL_DMA_CHANNEL_7));

	return 0;
}
unsigned char CommandParserSerTestWrite(char *buffer,unsigned char size)
{
	fprintf(file_bt,"Hello world\n");

	return 0;
}
unsigned char CommandParserSerTestWrite2(char *buffer,unsigned char size)
{
	char *b="Hello world putbuf\n";

	fputbuf(file_bt,b,strlen(b));

	return 0;
}
