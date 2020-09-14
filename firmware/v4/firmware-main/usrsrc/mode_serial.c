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
#include "mode_benchmarks.h"
#include "mode.h"
#include "wait.h"
#include "serial.h"
#include "serial_uart.h"
#include "rn41.h"
#include "usart.h"
#include "dma.h"
#include "system.h"
#include "helper.h"
#include "ltc2942.h"
#include "stm32l4xx.h"

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
	{'n', CommandParserSerTestDMAIsEn,"DMA is en"},
	{'B', CommandParserSerTestWrite,"Write lots"},
	{'b', CommandParserSerTestWrite2,"Write lots 2"},
	{'m', CommandParserSerTestWrite3,"m,<size>,<delay> Bluetooth debug write: write continuously by polling size bytes; then pause for delay; and write again. Stop on keypress."},
	{'E',CommandParserIntfEvents,"Show UART event counters"},
	{'C',CommandParserIntfEventsClear,"Clear UART event counters"},
	{'r',CommandParserSerReg,"USART registers"},

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

	for(int i=0;i<SERIAL_UART_DMA_TX_BUFFERSIZE;i++)
		_serial_uart_tx_dma_buffer[i]='0'+i;


	// Configure DMA
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, SERIAL_UART_DMA_TX_BUFFERSIZE);
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

	for(int i=0;i<SERIAL_UART_DMA_TX_BUFFERSIZE;i++)
		_serial_uart_tx_dma_buffer[i]='0'+i;


	// Configure DMA
	LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_7, SERIAL_UART_DMA_TX_BUFFERSIZE);
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
unsigned char CommandParserSerReg(char *buffer,unsigned char size)
{
	fprintf(file_usb,"USART CR1: %08X\n",USART2->CR1);
	fprintf(file_usb,"USART CR2: %08X\n",USART2->CR2);
	fprintf(file_usb,"USART CR3: %08X\n",USART2->CR3);
	fprintf(file_usb,"USART ISR: %08X\n",USART2->ISR);
	return 0;
}


unsigned char CommandParserSerTestWrite3(char *buffer,unsigned char size)
{
	// Check the packet size and delay

	int bufsiz,delay;

	if(ParseCommaGetInt(buffer,2,&bufsiz,&delay))
		return 2;
	if(bufsiz<1)
		return 2;
	if(delay<0 || delay>10000)
		return 2;

	WAITPERIOD wp=0;

#if 0
	unsigned t1 = timer_ms_get();
	for(int i=0;i<100;i++)
	{
		//timer_waitperiod_ms(delay,&wp);
		timer_us_wait(6000);
		//unsigned long t = timer_waitperiod_us(delay,&wp);
		//rintf(file_usb,"%u\n",t);
	}
	unsigned t2 = timer_ms_get();
	fprintf(file_pri,"dt: %u\n",t2-t1);
#endif



#if 1

	fprintf(file_pri,"Writing to UART until keypress on USB\n");
	fprintf(file_pri,"Packets (written as quickly as possible): %d followed by %d us pause\n",bufsiz,delay);

	// Works only in non-dma mode

	fprintf(file_usb,"Deactivate interface change detect\n");
	interface_changedetectenable(0);


	// Deactivate interrupts
	LL_USART_DisableIT_TXE(USART2);			// No TX DMA -> enable TXE interrupt
	//LL_USART_EnableIT_CTS(USART2);
	LL_USART_DisableIT_RXNE(USART2);			// No RX DMA -> enable RX interrupt

	fprintf(file_usb,"USART CR3: %08X\n",USART2->CR3);

	// CTS to input push-pull
	// No need to configure as INPUT; can read pin state also in alternate function with LL_USART_HWCONTROL_NONE
	/*LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = LL_GPIO_PIN_0;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	ErrorStatus e = LL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	fprintf(file_usb,"Init of CTS as input: %d\n",e);*/

	// Wait for BT connection
	fprintf(file_usb,"Waiting for BT connection\n");

	while(!system_isbtconnected())
	{
		if(fgetc(file_usb)!=-1)
		{
			fprintf(file_usb,"Interrupted\n");
			goto CommandParserSerTestWrite3_end;
		}
	}

	fprintf(file_usb,"BT Connected\n");

	HAL_Delay(100);

	serial_uart_clearevents();
	fprintf(file_usb,"UART events cleared\n");


	unsigned nit=0;
	unsigned ts1 = timer_ms_get();
	unsigned tsctr = 0;
	unsigned t1,t2,n=0;
	unsigned char cts,cts_last;
	unsigned infointerval=1000;
	unsigned ctsblock=0,ctschange=0;
	unsigned maxdur = 10000;

	cts = cts_last = _serial_usart_cts();
	fprintf(file_usb,"CTS: %d\n",cts);

	t1 = timer_ms_get();
	while(1)
	{
		if(fischar(file_usb) || !system_isbtconnected())		// If keypress or disconnection -> interrupt
			break;

		unsigned t = timer_ms_get();
		if(t-t1>maxdur)
		{
			fprintf(file_usb,"Interrupting after maximum duration\n");
			break;
		}

		if(t>ts1+infointerval)
		{
			// infointerval-ms periodic info
			ts1+=infointerval;
			fprintf(file_usb,"%u\n",tsctr++);

			// Get power consumption
			fprintf(file_pri,"#%s\n",ltc2942_last_strstatus());
			for(unsigned char i=0;i<LTC2942NUMLASTMW;i++)
				fprintf(file_usb,"%d ",ltc2942_last_mWs(i));
			fprintf(file_usb,"\n");
		}

		// Write to BT
		// Transmit
		for(int i=0;i<bufsiz;i++)
		{
			cts = _serial_usart_cts();
			if(cts!=cts_last)
			{
				ctschange++;
				//fprintf(file_usb,"CTS: %d\n",cts);
				cts_last = cts;
			}

			// Check what is the CTS status - if CTS forbids us from sending, then wait
			if(0)	// Enable manual CTS check
			{
				if(cts==0)
				{
					ctsblock++;
					while( _serial_usart_cts() == 0 && !fischar(file_usb))
					{
						// Wait
					}
					//fprintf(file_usb,"*\n");
				}
			}

			//serial_usart_putchar_block(USART2,'@');
			if((n&1)==0)
			{
				serial_usart_putchar_block(USART2,'0'+(n&0xf));
			}
			else
			{
				serial_usart_putchar_block(USART2,'\n');
			}
			n++;
		}
		timer_us_wait(delay);
		nit++;

	}
	t2 = timer_ms_get();

	HAL_Delay(1000);

	fprintf(file_usb,"Interrupted\n");
	fprintf(file_usb,"Number of iterations: %u\n",nit);

	fprintf(file_usb,"Bytes: %d. Time: %d ms\n",n,t2-t1);
	fprintf(file_usb,"Throughput: %u KB/s\n",n*1000/1024/(t2-t1));

	fprintf(file_usb,"CTS changes: %u. CTS wait: %u\n",ctschange,ctsblock);

	fprintf(file_usb,"UART events\n");
	serial_uart_printevents(file_usb);


	HAL_Delay(1000);


CommandParserSerTestWrite3_end:
	LL_USART_EnableIT_TXE(USART2);			// No TX DMA -> enable TXE interrupt
	//LL_USART_EnableIT_CTS(USART2);
	LL_USART_EnableIT_RXNE(USART2);			// No RX DMA -> enable RX interrupt



	fprintf(file_usb,"Activate interface change detect\n");
	fprintf(file_usb,"This is USB\n");
	fprintf(file_bt,"This is BT\n");

	interface_changedetectenable(1);
#endif
	return 0;
}

