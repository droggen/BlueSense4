/*
 * usrmain.c
 *
 *  Created on: 3 Jul 2019
 *      Author: droggen
 */

#include <stdio.h>
#include <string.h>
//#include <usart.h.not>
#include "stm32l4xx_hal.h"
//#include <mode.h>
#include "main.h"
//#include "gpio.h"
#include "serial.h"
#include "serial_itm.h"
//#include "serial_usb.h"
//#include "usbd_cdc_if.h"
//#include "wait.h"
#include "core_cm4.h"
//#include "usbd_cdc_if.h"
/*#include "i2c.h"
#include "i2c/megalol_i2c.h"
#include "ltc2942.h"
#include "m24xx.h"*/
//#include "stm32f4xx_hal_uart.h"
/*#include "serial_uart.h"
#include "serial_itm.h"
#include "rn41.h"*/
#include "global.h"
#include "system.h"
#include "bs4-init.h"
/*#include "helper_num.h"
#include "i2c/stmi2c.h"*/


void blink0()
{
	for(int i=0;i<3;i++)
	{
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(500);

	}
}

void blink1()
{
	while(1)
	{
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(500);

	}
}
void blink2()
{
	while(1)
	{
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		ITM_SendChar('o');
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		ITM_SendChar('O');
		HAL_Delay(500);

	}
}
void blink3()
{
	while(1)
	{
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		itmprintf("LED off\n");
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		itmprintf("LED on\n");
		HAL_Delay(500);
	}
}
// Test push button
void blink4()
{
	while(1)
	{
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(50);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(50);

		// Read button
		if(HAL_GPIO_ReadPin(PWR_PBSTAT_GPIO_Port,PWR_PBSTAT_Pin))
			itmprintf("Pressed\n");
		else
			itmprintf("Not pressed\n");

	}
}

// Test USB print
#if 0
void blink5()
{
	char s1[]="Led off\n";
	char s2[]="Led on\n";
	while(1)
	{
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		itmprintf("LED off\n");
		CDC_Transmit_FS(s1,strlen(s1));
		HAL_Delay(500);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		itmprintf("LED on\n");
		CDC_Transmit_FS(s2,strlen(s2));
		HAL_Delay(500);
	}
}
#endif
#if 0
// Test USB VBUS detect
void blink6()
{
	int i=0;
	char t[64];
	while(1)
	{
		sprintf(t,"USB %d\n",i++);
		CDC_Transmit_FS(t,strlen(t));
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(100);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(100);
		// Read button
		if(HAL_GPIO_ReadPin(USB_SENSE_GPIO_Port,USB_SENSE_Pin))
			itmprintf("Pressed\n");
		else
			itmprintf("Not pressed\n");
	}
}
#endif
void itmblink()
{
	//ITM_SendChar('A');
  /*while(1)
  {
	  for(int i=0;i<100000;i++)
		  ITM_SendChar('x');

	  HAL_GPIO_WritePin(GPIOD, LD5_Pin, GPIO_PIN_RESET);

	  for(int i=0;i<100000;i++)
		  ITM_SendChar('y');

	  HAL_GPIO_WritePin(GPIOD, LD5_Pin, GPIO_PIN_SET);
  }*/
}

/*void itmblink2()
{
	file_itm = serial_open(SERIAL_ITM,0);
	file_usb = serial_open(SERIAL_USB,0);

	//ITM_SendChar('A');
  while(1)
  {
	  //for(int i=0;i<100000;i++)
		//  ITM_SendChar('x');



	  HAL_Delay(500);

	  danprintf("Off\n");

	  HAL_GPIO_WritePin(GPIOD, LD5_Pin, GPIO_PIN_RESET);

	  //for(int i=0;i<100000;i++)
		//  ITM_SendChar('y');
	  HAL_Delay(500);

	  danprintf("On\n");

	  HAL_GPIO_WritePin(GPIOD, LD5_Pin, GPIO_PIN_SET);

	  danprintf("buffer level: %d\n",buffer_level(&SERIALPARAM_USB.rxbuf));

	  //CDC_Transmit_FS("Hello\n",6);
	  printf("toto\n");

	  fprintf(file_usb,"Call1\n");
	  HAL_Delay(100);
	  fprintf(file_usb,"9876\n");
	  fputs("fputs\n",file_usb);

	  char b[128];
	  if(fgets(b,128,file_usb))
		  printf("got: '%s'\n",b);

	  //fprintf(file_itm,"fprintf\n");
	  //fflush(file_dbg);
	  //char c = fgetc(file_itm);
	  //printf("c: %d\n",c);


	  //fprintf(file_usb,"qfprintf to usb\n");
	  //fflush(file_usb);
  }

}*/



void BS_GPIO_Init()
{
/*


	//while(1);
	  // LEDs
	  GPIO_InitTypeDef GPIO_InitStruct;
	  GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_13|GPIO_PIN_12;
	  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	  GPIO_InitStruct.Pull = GPIO_NOPULL;
	  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
	  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin, GPIO_PIN_SET);





	  HAL_GPIO_WritePin(GPIOD, LD5_Pin, GPIO_PIN_RESET);



*/


}

/*
void init()
{
	file_pri = file_usb = serial_open(SERIAL_USB,0);
}
*/

/*
Bus answers at:
	ADR RD A2: rcv: 0 FF 55 55 55
	ADR RD A3: rcv: 0 FF 55 55 55
	ADR RD C8: rcv: 0 00 55 55 55
	ADR RD C9: rcv: 0 00 55 55 55

	ADR WR A2: rcv: 0
	ADR WR A3: rcv: 0
	ADR WR C8: rcv: 0
	ADR WR C9: rcv: 0

	// LTC2942: 7-bit addr: 0x64	(8-bit: C8, C9)
	// M24xxx: 	7-bit addr: 0x51	(8-bit: A2, A3)

 */

#if 0
void test_i2c()
{
	uint8_t data[16];
	HAL_StatusTypeDef r;
	while(1)
	{
		for(int adr=1;adr<=255;adr++)
		{
			// Read
			data[0] = data[1] = data[2] = data[3] = 0x55;
			r = HAL_I2C_Master_Receive(&hi2c1,adr,(uint8_t*)&data,1,100);
			if(r==0)
				itmprintf("ADR RD %02X: rcv: %d %02X %02X %02X %02X\n",adr,r,data[0],data[1],data[2],data[3]);

			HAL_Delay(10);
		}
		itmprintf("\n");
		for(int adr=1;adr<=255;adr++)
		{
			// Read
			data[0] = 0;
			r = HAL_I2C_Master_Transmit(&hi2c1,adr,(uint8_t*)&data,1,100);
			if(r==0)
				itmprintf("ADR WR %02X: rcv: %d\n",adr,r);

			HAL_Delay(10);
		}
		itmprintf("\n");
		HAL_Delay(1000);
	}
}
void test_coulomb()
{
	int i=0;
	char t[64];

	unsigned char reg;
	unsigned char rv;
	unsigned char tmp;
	HAL_StatusTypeDef r;
	uint8_t data[16];

	ltc2942_init();

	while(1)
	{
		sprintf(t,"USB %d\n",i++);
		CDC_Transmit_FS(t,strlen(t));
		itmprintf("%s",t);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(100);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(100);


		unsigned short charge = ltc2942_getchargectr();
		unsigned long chargeuah = ltc2942_getcharge();
		int temp=ltc2942_gettemperature();
		int volt=ltc2942_getvoltage();
		itmprintf("charge: %d charge uAh: %d temp: %d volt: %d\n",charge,chargeuah,temp,volt);




		itmprintf("\n");


	}
}
void test_eeprom()
{
	int i=0;
	char t[64];

	unsigned char reg;
	unsigned char rv;
	unsigned char tmp;
	HAL_StatusTypeDef r;
	uint8_t data[16];

	rv = m24xx_write(8,0x22);
	itmprintf("write: %d\n",rv);

	rv = m24xx_write(10,0x33);
	itmprintf("write: %d\n",rv);
	rv = m24xx_write(12,0x77);
	itmprintf("write: %d\n",rv);
	rv = m24xx_write(13,0x88);
	itmprintf("write: %d\n",rv);

	while(1)
	{
		sprintf(t,"USB %d\n",i++);
		CDC_Transmit_FS(t,strlen(t));
		itmprintf("%s",t);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(100);
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(100);


		//unsigned char rv,data;
		//rv = m24xx_read(i,&data);
		//itmprintf("%02X: %d %02X\n",i,rv,data);
		m24xx_printreg(file_pri,0,16384);





		itmprintf("\n");


	}
}

void test_extperiph()
{
	int i=0;
	char t[64];
	while(1)
	{
		sprintf(t,"USB %d\n",i++);
		CDC_Transmit_FS(t,strlen(t));
		itmprintf("%s",t);

		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);

		// Reset BT
		HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_RESET);
		system_periphvcc_disable();

		for(int j=0;j<15;j++)
		{
			HAL_Delay(1000);
			itmprintf(".");
		}
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);

		// Turn on power switch
		system_periphvcc_enable();
		// Unreset BT
		HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_SET);

		for(int j=0;j<15;j++)
		{
			HAL_Delay(1000);
			itmprintf("O");
		}


	}
}
void test_rn41()
{
	int i=0;
	char t[64];
	char buffer[256];

	/*while(1)
	{
		int t = HAL_GetTick();
		itmprintf("t: %d\n",t);
		HAL_Delay(1000);
	}*/

	HAL_StatusTypeDef r;

	// Turn on power switch
	system_periphvcc_enable();
	// Reset BT
	HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_RESET);
	HAL_Delay(300);
	HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_SET);
	HAL_Delay(300);

	HAL_Delay(2000);

	int p = serial_uart_init(USART2,1);
	if(p==-1)
	{
		while(1)
		{
			itmprintf("usart init fail\n");
			HAL_Delay(1000);
		}
	}
	else
		itmprintf("usart init ok\n");

	while(1)
	{
		sprintf(t,"USB %d\n",i++);
		CDC_Transmit_FS(t,strlen(t));
		itmprintf("%s",t);

		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(1000);

		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(1000);

		// Send a string to UART2
		// huart2
		//HAL_USART_Transmit();
		//HAL_USART_Receive();

		/*for(int j=0;j<128;j++)
			buffer[j]=32;
		//r = HAL_UART_Receive(&huart2,buffer,128,1000);
		itmprintf("read: %d\n",r);
		itmprintf("\"\n"); for(int j=0;j<128;j++) itmprintf("%c",buffer[j]); itmprintf("\"\n");*/

		//for(int j=0;j<128;j++) buffer[j]=32;
		//for(int j=0;j<128;j++) buffer[j]=serial_usart_getchar_nonblock(USART2);


		/*int n = serial_usart_getbuff_nonblock(USART2,buffer,127,100);
		buffer[n] = 0;
		itmprintf("read %02d. \"%s\"\n",n,buffer);*/

		int c;
		while((c = serial_uart_getchar_nonblock_int(p))!=-1)
		{
			//itmprintf("read: %d (%c)",c,c);
			itmprintf("%c ",c);
		}


		//sprintf(buffer,"AT\r\n");
		switch(i)
		{
			case 1:
				sprintf(buffer,"$$$");
				break;
			case 2:
				sprintf(buffer,"E\n");
				break;
			default:
			{
				buffer[0]=0;
			}
		}

		HAL_Delay(1100);

		//r = HAL_UART_Transmit(&huart2,buffer,strlen(buffer),1000);
		//r = HAL_UART_Transmit_IT(&huart2,buffer,strlen(buffer));

		//serial_usart_putbuff_block(USART2,buffer,strlen(buffer));
		serial_uart_putbuf_int(buffer,strlen(buffer),p);
		//
		HAL_Delay(1100);

		/*for(int j=0;j<10;j++)
		{
			serial_uart_putchar_block_int('A'+j,p);
		}*/

		int txl = serial_uart_gettxbuflevel(p);
		int rxl = serial_uart_getrxbuflevel(p);
		itmprintf("txl: %d rxl: %d\n",txl,rxl);






	}
}


void test_rn41_b()
{
	int i=0;
	char t[64];
	char buffer[256];

	/*while(1)
	{
		int t = HAL_GetTick();
		itmprintf("t: %d\n",t);
		HAL_Delay(1000);
	}*/

	HAL_StatusTypeDef r;

	// Turn on power switch
	system_periphvcc_enable();
	// Reset BT
	/*
	HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_RESET);
	HAL_Delay(300);
	HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_SET);
	HAL_Delay(300);
	*/
	rn41_Reset(file_itm);

	//HAL_Delay(2000);

	/*int p = serial_uart_init(USART2,1);
	if(p==-1)
	{
		while(1)
		{
			itmprintf("usart init fail\n");
			HAL_Delay(1000);
		}
	}
	else
		itmprintf("usart init ok\n");*/

	/*int p;
	FILE *file = serial_open_uart(USART2,&p);
	if(file==-1)
	{
		while(1)
		{
			itmprintf("usart init fail\n");
			HAL_Delay(1000);
		}
	}
	else
		itmprintf("usart init ok. p: %d. file: %p\n",p,file);*/
	FILE *file = file_bt;


	while(1)
	{
		sprintf(t,"USB %d\n",i++);
		CDC_Transmit_FS(t,strlen(t));
		itmprintf("%s",t);

		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
		HAL_Delay(1000);

		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
		HAL_Delay(1000);

		// Send a string to UART2
		// huart2
		//HAL_USART_Transmit();
		//HAL_USART_Receive();

		/*for(int j=0;j<128;j++)
			buffer[j]=32;
		//r = HAL_UART_Receive(&huart2,buffer,128,1000);
		itmprintf("read: %d\n",r);
		itmprintf("\"\n"); for(int j=0;j<128;j++) itmprintf("%c",buffer[j]); itmprintf("\"\n");*/

		//for(int j=0;j<128;j++) buffer[j]=32;
		//for(int j=0;j<128;j++) buffer[j]=serial_usart_getchar_nonblock(USART2);


		/*int n = serial_usart_getbuff_nonblock(USART2,buffer,127,100);
		buffer[n] = 0;
		itmprintf("read %02d. \"%s\"\n",n,buffer);*/

		/*int c;
		while((c = serial_uart_getchar_nonblock_int(p))!=-1)
		{
			//itmprintf("read: %d (%c)",c,c);
			itmprintf("%c ",c);
		}*/

		char rdbuf[64];
		if(fgets(rdbuf,8,file))
			itmprintf("Read: '%s'\n",rdbuf);
		else
			itmprintf("Read nothing\n");

		//sprintf(buffer,"AT\r\n");
		switch(i)
		{
			case 1:
				sprintf(buffer,"$$$");
				break;
			case 2:
				sprintf(buffer,"E\n");
				break;
			default:
			{
				buffer[0]=0;
			}
		}

		HAL_Delay(1100);

		//r = HAL_UART_Transmit(&huart2,buffer,strlen(buffer),1000);
		//r = HAL_UART_Transmit_IT(&huart2,buffer,strlen(buffer));

		//serial_usart_putbuff_block(USART2,buffer,strlen(buffer));
		//serial_uart_putbuf_int(buffer,strlen(buffer),p);

		fputs(buffer,file);

		//
		HAL_Delay(1100);

		/*for(int j=0;j<10;j++)
		{
			serial_uart_putchar_block_int('A'+j,p);
		}*/

		//int txl = serial_uart_gettxbuflevel(p);
		//int rxl = serial_uart_getrxbuflevel(p);
		//itmprintf("txl: %d rxl: %d\n",txl,rxl);






	}
}
#endif

unsigned char timercb1(unsigned char p)
{
	// Toggle light in callback
	HAL_GPIO_TogglePin(LED_0_GPIO_Port, LED_0_Pin);
	return 0;
}
#if 0
void testtimercallbacks()
{
	itmprintf("Testing timer callbacks\n");

	/*for(int i=0;i<2;i++)
	{
		HAL_Delay(1000);
		int t = timer_ms_get();
		itmprintf("Time: %d\n",t);
	}*/
	timer_printcallbacks(file_itm);

	// Register a 1 second callback

	//timer_register_callback(timercb1,999);

	timer_printcallbacks(file_itm);
	timer_printcallbacks(file_itm);

}


#endif

#if 0
volatile int32_t ITM_RxBuffer;
void test_itmget()
{
	while(1)
	{
		int r = ITM_ReceiveChar();
		fprintf(file_itm,"%04X\n",r);
		HAL_Delay(100);
	}
}

void test_usb_io1()
{
	char buf[128];

	while(1)
	{
		fprintf(file_usb,"Write something\n");
		buf[0]=0;
		char *rv = fgets_timeout(buf,128,file_usb,10000);
		fprintf(file_usb,"rv: %p\n",rv);
		fprintf(file_usb,"got: '%s'\n",buf);
	}
}

void test_putbuf1()
{
	fprintf(file_pri,"Testing putbuf\n");
	char buffer[512];
	unsigned char s[]={1,8,16,32,64,128,255};
	unsigned char rv=0;

	for(int i=0;i<512;i++)
	{
		buffer[i]='0'+i%10;
	}
	while(1)
	{
		for(int i=0;i<=7;i++)
		{
			fprintf(file_pri,"putbuf %d\n",s[i]);
			HAL_Delay(50);
			rv = fputbuf(file_pri,buffer,s[i]);
			fprintf(file_pri,"\nresult: %d\n",rv);
			HAL_Delay(1000);
		}

	}
}
#endif

void usrmain()
{
	//blink1();
	//blink2();
	//blink3();
	//blink4();
	//blink5();
	//blink6();
	//test_coulomb();
	//test_eeprom();
	//test_i2c();
	//test_extperiph();
	//test_rn41();

	//blink0();
	//init1();

	bs4_init();

	//testtimercallbacks();

	//test_rn41_b();

	//test_itmget();

	//rn41_Setup(file_itm,file_bt,system_devname);
	//rn41_Setup(file_usb,file_bt,system_devname);

	//fprintf(file_itm,"rn41 setup returns devname: '%s'\n",system_devname);
	//fprintf(file_usb,"rn41 setup returns devname: '%s'\n",system_devname);

#if 0
	int ctr=0;
	while(1)
	{
		//fprintf(file_pri,"Hello %d\n",ctr);
		fprintf(file_itm,"Hello %d\n",ctr);
		ctr++;
		HAL_Delay(200);
	}
#endif

	//
	/*int ctr=0,pwr=1;
	while(1)
	{
		fprintf(file_pri,"ctr: %d USB: %d BT: %d\n",ctr,system_isusbconnected(),system_isbtconnected());
		ctr++;
		//HAL_Delay(100);
		HAL_Delay(500);

		// Try power toggling the BT interface
		//if((ctr%200)==0)
		if(0)
		{
			fprintf(file_pri,"toggling power\n");
			if((ctr/200)&1)
			{
				fprintf(file_pri,"on\n");
				system_periphvcc_enable();
			}
			else
			{
				fprintf(file_pri,"off\n");
				system_periphvcc_disable();
			}
		}

	}*/

#if 0
  while(1)
  {

	  itmprintf("Hello world\n");

	  // Try led on
	  HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
	  HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);

	  HAL_Delay(1000);

	  // Try led on
	  HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
	  HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
	  HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);

	  HAL_Delay(1000);

  }
#endif



	//i2c_printconfig();


	itmprintf("Entering main loop\n");

	mode_dispatch();

	while(1);
#if 0

	test_usb_io1();

	// Check fgets_timeout

	char b1[64];
	char *rv;


	fprintf(file_itm,"call fgets_timeout\n");
	int t1 = HAL_GetTick();
	rv = fgets_timeout(b1,61,file_bt,1000);
	int t2 = HAL_GetTick();
	fprintf(file_itm,"fgets_timeout return: %d ms\n",t2-t1);
	fprintf(file_itm,"rv: %p. length: %d. str: '%s'\n",rv,strlen(b1),b1);



	while(1)
	{
		int t = HAL_GetTick();
		fprintf(file_itm,"tick: %d\n",t);
		HAL_Delay(100);
	}

	while(1);
#endif
/*	BS_GPIO_Init();

	init();

	for(int i=0;i<10;i++)
	{
		fprintf(file_pri,"Waiting %d...\n",i);
		printf("Waiting %d...\n",i);
		HAL_Delay(100);
	}

	mode_main();
	while(1)
	{
		//fprintf(file_usb,"a");
		 //printf("Call2\n");
		  //HAL_Delay(500);



		 if(feof(file_usb))
			 printf("EOF\n");
		 else
			 printf("!EOF!\n");

		//char c;
		//c=fgetc(file_usb);
		//printf("c: %02X\n",c);

		 char b[128];
		 char *c = fgets(b,128,file_usb);
		 printf("c: %p\n",c);
		 if(c)
			 printf("got: '%s'\n",b);

		HAL_Delay(200);

	}

	fprintf(file_pri,"After idle\n");
	printf("After idle\n");



	//itmblink();
	itmblink2();
	*/
}



void HAL_SYSTICK_Callback(void)
{
	static int ctr=0;



	ctr++;
	if(ctr>1000)
	{
		itmprintf("s\n");
		ctr=0;
	}

}


