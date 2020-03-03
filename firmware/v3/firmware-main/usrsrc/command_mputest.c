/*
 * command_mputest.c
 *
 *  Created on: 23 sept. 2019
 *      Author: droggen
 */

#include <string.h>
#include "global.h"
#include "usrmain.h"
#include "main.h"
#include "mpu.h"
#include "mpu-spi.h"
#include "stmspi.h"
#include "helper.h"
#include "system.h"

/*

// MPU9250 datasheet:
1. Data is delivered MSB first and LSB last
2. Data is latched on the rising edge of SCLK
3. Data should be transitioned on the falling edge of SCLK
4. The maximum frequency of SCLK is 1MHz
5. SPI read and write operations are completed in 16 or more clock cycles (two or more bytes).
   The first byte contains the SPI Address, and the following byte(s) contain(s) the SPI data.
   The first bit of the first byte contains the Read/Write bit and indicates the Read (1) or Write (0) operation.
   The following 7 bits contain the Register Address. In cases of multiple-byte Read/Writes, data is two or more bytes.

// Set frame format
// SPI mode: MSB first, data latched on rising edge, data must be transitionned on falling edge, sck high when idle -> mode 3

CPOL=high:		sck high when idle
CPHA=set		second edge is latch strobe: data latched on rising edge


STM datasheet:
CPOL=1
CPHA=1


On init CR1: 0000032C CR2: 00000000


Works with:
36F: ok			CPOL=1 (ck=1 when idle)	CPHA=1 (2nd clock is data capture edge)
36C: ok			CPOL=0 (ck=0 when idle) CPHA=0 (1st clock is data capture edge)
36E: weird data
36D: always zero


*/

unsigned char mpu_readreg_hal(unsigned char reg)
{
	unsigned n=8;
	unsigned char buf[n],bufr[n];
	memset(buf,0,n);
	memset(bufr,0,n);
	buf[0] = 0x80|reg;

	//spiusart0_rwn(buf,2);

	fprintf(file_pri,"CR1: %08X CR2: %08X\n",(unsigned)SPI1->CR1,(unsigned)SPI1->CR2);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1,buf,bufr,n,1000);
	spiusart0_rwn_wait_poll_block(buf,n);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive_IT(&hspi1,buf,bufr,2);
	fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");

	/*while(1)
	{
		HAL_Delay(50);
		s=HAL_SPI_GetState(&hspi1);
		fprintf(file_pri,"Status: %d.\n",s);

		if(fgetc(file_pri)!=-1)
			break;
	}

	fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");*/

	return bufr[1];
}
unsigned char mpu_readreg_test(unsigned char reg)
{
	unsigned n=2;
	unsigned char buf[n],bufr[n];
	unsigned sr;

	memset(buf,0xff,n);
	memset(bufr,0xff,n);


	buf[0] = 0x80|reg;

	//fprintf(file_pri,"Before: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]);

	//sr=SPI1->SR; fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);

	spiusart0_rwn_wait_poll_block(buf,n);



	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1,buf,bufr,n,1000);
	//spiusart0_rwn_block_poll(buf,n);

	//sr=SPI1->SR; fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive_IT(&hspi1,buf,bufr,2);
	//fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");
	//fprintf(file_pri,"After: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]);

	/*while(1)
	{
		HAL_Delay(50);
		s=HAL_SPI_GetState(&hspi1);
		fprintf(file_pri,"Status: %d.\n",s);

		if(fgetc(file_pri)!=-1)
			break;
	}

	fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");*/

	return buf[1];
	//return bufr[1];
}
unsigned char mpu_readreg_test_int(unsigned char reg)
{
	unsigned n=2;
	unsigned char buf[n],bufr[n];
	unsigned sr;

	memset(buf,0xff,n);
	memset(bufr,0xff,n);


	buf[0] = 0x80|reg;

	//fprintf(file_pri,"Before: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]);

	//sr=SPI1->SR; fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);

	spiusart0_rwn_wait_int_block(buf,n);



	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1,buf,bufr,n,1000);
	//spiusart0_rwn_block_poll(buf,n);

	//sr=SPI1->SR; fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive_IT(&hspi1,buf,bufr,2);
	//fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");
	//fprintf(file_pri,"After: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]);

	/*while(1)
	{
		HAL_Delay(50);
		s=HAL_SPI_GetState(&hspi1);
		fprintf(file_pri,"Status: %d.\n",s);

		if(fgetc(file_pri)!=-1)
			break;
	}

	fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");*/

	return buf[1];
	//return bufr[1];
}

/*void spi_init1()
{
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
		fprintf(file_pri,"Init error\n");
	else
		fprintf(file_pri,"Init OK\n");
}*/

/******************************************************************************
	function: CommandParserMPUTest1
*******************************************************************************


	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid

******************************************************************************/
unsigned char CommandParserMPUTest1(char *buffer,unsigned char size)
{
	fprintf(file_pri,"mpu test 1\n");

	// Read status of SPI

	// Register 117 is WHOAMI

	// Try read
	unsigned char r;
	r = mpu_readreg_hal(117);
	fprintf(file_pri,"Reg: %02X\n",r);

	return 0;
}
unsigned char CommandParserMPUTest2(char *buffer,unsigned char size)
{
/*	fprintf(file_pri,"mpu test 2\n");
	unsigned char r;
	r = mpu_readreg(117);
	fprintf(file_pri,"WhoAmI: %02x\n",r);

	for(int a=0;a<=126;a++)
	{
		r = mpu_readreg(a);
		fprintf(file_pri,"%02X ",r);
		if((a&0xf)==0xf)
			fprintf(file_pri,"\n");
	}
	fprintf(file_pri,"\n");


*/
	mpu_writereg_wait_poll_block(119,0x55);
	mpu_writereg_wait_poll_block(120,0xaa);
	mpu_writereg_wait_poll_block(121,0xa5);
	mpu_writereg_wait_poll_block(122,0x5a);


	return 0;
}
unsigned char CommandParserMPUTest3(char *buffer,unsigned char size)
{
	/*fprintf(file_pri,"mpu test 3\n");

	// Register 117 is WHOAMI

	// Try read
	unsigned char r;
	r = mpu_readreg(117);
	fprintf(file_pri,"Reg 117: %02X\n",r);
	// Try write
	r = mpu_readreg(119);
	fprintf(file_pri,"Reg 119: %02X\n",r);
	r = mpu_readreg(120);
	fprintf(file_pri,"Reg 120: %02X\n",r);

	fprintf(file_pri,"Write 119,120\n");

	mpu_writereg(119,0x55);
	mpu_writereg(120,0xaa);

	r = mpu_readreg(117);
	fprintf(file_pri,"Reg 117: %02X\n",r);
	// Try write
	r = mpu_readreg(119);
	fprintf(file_pri,"Reg 119: %02X\n",r);
	r = mpu_readreg(120);
	fprintf(file_pri,"Reg 120: %02X\n",r);
*/

	fprintf(file_pri,"SPI interrupt+wait\n");
	unsigned char reg = 117;
	unsigned n=8;
	unsigned char buf[n],bufr[n];
	memset(buf,0,n);
	memset(bufr,0,n);
	buf[0] = 0x80|reg;

	//spiusart0_rwn(buf,2);

	fprintf(file_pri,"CR1: %08X CR2: %08X\n",SPI1->CR1,SPI1->CR2);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1,buf,bufr,n,1000);
	//spiusart0_rwn(buf,n);
	spiusart0_rwn_wait_int_block(buf,n);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive_IT(&hspi1,buf,bufr,2);
	fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");


	return 0;
}
void CommandParserMPUTest4_cb()
{
	fprintf(file_pri,"cb\n");
}
unsigned char CommandParserMPUTest4(char *buffer,unsigned char size)
{
	fprintf(file_pri,"SPI interrupt+callback\n");
	unsigned char reg = 117;
	unsigned n=8;
	unsigned char buf[n],bufr[n];
	memset(buf,0,n);
	memset(bufr,0,n);
	buf[0] = 0x80|reg;

	//spiusart0_rwn(buf,2);

	fprintf(file_pri,"CR1: %08X CR2: %08X\n",SPI1->CR1,SPI1->CR2);

	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1,buf,bufr,n,1000);
	//spiusart0_rwn(buf,n);
	spiusart0_rwn_try_int_cb(buf,n,CommandParserMPUTest4_cb);

	memcpy(bufr,buf,n);

	unsigned it=0;

	while(spiusart0_isbusy())
		it++;
	fprintf(file_pri,"#it: %u\n",it);


	//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive_IT(&hspi1,buf,bufr,2);
	fprintf(file_pri,"Status: %d. tx: "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",buf[i]); fprintf(file_pri,"| "); for(int i=0;i<n;i++) fprintf(file_pri,"%02x ",bufr[i]); fprintf(file_pri,"\n");


	return 0;
}
unsigned char CommandParserMPUInit1(char *buffer,unsigned char size)
{
	/*fprintf(file_pri,"POLARITY_HIGH 2EDGE\n");

	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;						// MPU9250
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;							// rising edge?
	//hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
		fprintf(file_pri,"Init error\n");
	else
		fprintf(file_pri,"Init OK\n");*/

	return 0;
}

unsigned char CommandParserMPUInit2(char *buffer,unsigned char size)
{
	// This mode works for the first byte read
	/*fprintf(file_pri,"POLARITY_LOW 1EDGE\n");

	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	//hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
		fprintf(file_pri,"Init error\n");
	else
		fprintf(file_pri,"Init OK\n");
*/
	return 0;
}

unsigned char CommandParserMPUInit3(char *buffer,unsigned char size)
{
	/*fprintf(file_pri,"POLARITY_HIGH 1EDGE\n");

	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	//hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
		fprintf(file_pri,"Init error\n");
	else
		fprintf(file_pri,"Init OK\n");
*/
	return 0;
}

unsigned char CommandParserMPUInit4(char *buffer,unsigned char size)
{
/*	fprintf(file_pri,"POLARITY_LOW 2EDGE\n");

	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	//hspi1.Init.NSS = SPI_NSS_HARD_OUTPUT;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK)
		fprintf(file_pri,"Init error\n");
	else
		fprintf(file_pri,"Init OK\n");
*/
	return 0;
}

unsigned char CommandParserMPUInitSPICube(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"Init SPI\n");
	HAL_SPI_MspInit(SPI1);
	MX_SPI1_Init();


	return 0;
}

unsigned char CommandParserMPUSS(char *buffer,unsigned char size)
{
	int ss;

	if(ParseCommaGetInt(buffer,1,&ss))
		return 2;

	if(ss)
	{
		fprintf(file_pri,"Set SS# high\n");
		SPIUSART0_DESELECT;

	}
	else
	{
		fprintf(file_pri,"Set SS# low\n");
		SPIUSART0_SELECT;

	}


	return 0;
}
unsigned char CommandParserMPUTestPrintModes(char *buffer,unsigned char size)
{
	mpu_printmotionmode(file_pri);
	return 0;
}

unsigned char CommandParserMPUTestPoweron(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	system_motionvcc_enable();
	return 0;
}
unsigned char CommandParserMPUTestPoweroff(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	system_motionvcc_disable();
	return 0;
}

unsigned char CommandParserMPUTestXchange(char *buffer,unsigned char size)
{
	int n;
	unsigned int d[16];
	unsigned char buftx[16],bufrx[16];

	if(ParseCommaGetNumParam(buffer)>16 || ParseCommaGetNumParam(buffer)<1)
			return 2;

	n = ParseCommaGetNumParam(buffer);

	if(ParseCommaGetInt(buffer,ParseCommaGetNumParam(buffer),&d[0],&d[1],&d[2],&d[3],&d[4],&d[5],&d[6],&d[7],&d[8],&d[9],&d[10],&d[11],&d[12],&d[13],&d[14],&d[15]))
		return 2;

	fprintf(file_pri,"Exchange send: ");
	for(int i=0;i<n;i++)
		fprintf(file_pri,"%02x ",d[i]);
	fprintf(file_pri,"\n");


	for(int i=0;i<n;i++)
	{
		buftx[i] = d[i];
		bufrx[i] = 0xAA;
	}

	//for(int i=0;i<n;i++)
	//{
		// SPI exchange


		//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&hspi1,buftx,bufrx,n,1000);
		spiusart0_rwn_wait_poll_block(buftx,n);

			//HAL_StatusTypeDef s = HAL_SPI_TransmitReceive_IT(&hspi1,buf,bufr,2);

		for(int i=0;i<n;i++)
			fprintf(file_pri,"%02x ",buftx[i]);
		/*
		fprintf(file_pri,"Status: %d. ",s);
		for(int i=0;i<n;i++)
			fprintf(file_pri,"%02x ",buftx[i]);
		fprintf(file_pri,"| ");
		for(int i=0;i<n;i++)
			fprintf(file_pri,"%02x ",bufrx[i]);
		fprintf(file_pri,"\n");*/


	//}


	return 0;
}

unsigned char CommandParserMPUTestPowerosc(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	while(1)
	{
		fprintf(file_pri,"ON\n");
		system_motionvcc_enable();
		HAL_Delay(2000);

		fprintf(file_pri,"OFF\n");
		system_motionvcc_disable();
		HAL_Delay(2000);

		if(fgetc(file_pri)!=-1)
				break;
	}
	fprintf(file_pri,"Leaving in OFF state\n");
	system_motionvcc_disable();

	return 0;
}
unsigned char CommandParserMPUTestSSosc(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	while(1)
	{
		fprintf(file_pri,"SS#=0\n");
		SPIUSART0_SELECT;
		HAL_Delay(2000);

		fprintf(file_pri,"SS#=1\n");
		SPIUSART0_DESELECT;
		HAL_Delay(2000);

		if(fgetc(file_pri)!=-1)
				break;
	}
	fprintf(file_pri,"Leaving with SS#=1\n");
	SPIUSART0_DESELECT;

	return 0;
}

unsigned char CommandParserMPUTestPrintSPIRegisters(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	unsigned cr1=SPI1->CR1;
	unsigned cr2=SPI1->CR2;
	unsigned sr=SPI1->SR;

	fprintf(file_pri,"SPI1 config registers:\n");
	fprintf(file_pri,"\tCR1: %04X\n",cr1);
	fprintf(file_pri,"\tCR2: %04X\n",cr2);
	fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);
	fprintf(file_pri,"\tDR: %08X\n",SPI1->DR);
	fprintf(file_pri,"\tCRCPR: %04X\n",SPI1->CRCPR);
	fprintf(file_pri,"\tRXCRCR: %04X\n",SPI1->RXCRCR);
	fprintf(file_pri,"\tTXCRCR: %04X\n",SPI1->TXCRCR);

	return 0;
}

unsigned char CommandParserMPUTestReadRegistersPoll(char *buffer,unsigned char size)
{
	int regstart,n;

	if(ParseCommaGetNumParam(buffer)<1 || ParseCommaGetNumParam(buffer)>2)
		return 2;
	if(ParseCommaGetInt(buffer,ParseCommaGetNumParam(buffer),&regstart,&n))
		return 2;
	if(ParseCommaGetNumParam(buffer)==1)
		n = 1;
	n&=0x7f;		// Cap n at 127.

	fprintf(file_pri,"Reading %d registers starting from %d\n",n,regstart);

	for(int i=0;i<n;i++)
	{
		unsigned r = mpu_readreg_wait_poll_block(regstart+i);
		fprintf(file_pri,"R%02X: %02X\n",regstart+i,r);

	}




	return 0;
}
unsigned char CommandParserMPUTestReadRegistersInt(char *buffer,unsigned char size)
{
	int regstart,n;


	if(ParseCommaGetNumParam(buffer)<1 || ParseCommaGetNumParam(buffer)>2)
		return 2;
	if(ParseCommaGetInt(buffer,ParseCommaGetNumParam(buffer),&regstart,&n))
		return 2;
	if(ParseCommaGetNumParam(buffer)==1)
		n = 1;
	n&=0x7f;		// Cap n at 127.

	fprintf(file_pri,"Reading %d registers starting from %d\n",n,regstart);

	for(int i=0;i<n;i++)
	{
		unsigned r = mpu_readreg_test_int(regstart+i);
		fprintf(file_pri,"R%02X: %02X\n",regstart+i,r);

	}




	return 0;
}
unsigned char CommandParserMPUTest5(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"Tryouts writing to DR and see how many bytes are effectively written");

	unsigned sr,sr2;
	sr = SPI1->SR;


	// Initial state
	fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);

	// Write once to TX
	//SPI1->DR = 0x8080;
	*(unsigned char *)&(SPI1->DR) = 0x80;
	//SPI1->DR = 0x8080;

	// New state
	for(int i=0;i<5;i++)
	{
		sr2=SPI1->SR;
		sr=sr2; fprintf(file_pri,"\tSR: %04X FTLVL %d FRLVL %d TXE %d RXNE %d\n",sr,(sr>>11)&0b11,(sr>>9)&0b11,(sr>>1)&0b1,sr&0b1);
	}

	return 0;
}
unsigned char CommandParserMPUTestPrintMPURegisters(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	mpu_printreg(file_pri);
	return 0;
}
unsigned char CommandParserMPUTestWriteRegistersPoll(char *buffer,unsigned char size)
{
	int startreg,n;
	unsigned int d[16];



	//fprintf(file_pri,"num param: %d\n",ParseCommaGetNumParam(buffer));

	if(ParseCommaGetNumParam(buffer)>17 || ParseCommaGetNumParam(buffer)<2)
		return 2;

	n = ParseCommaGetNumParam(buffer)-1;		// Number of bytes to write

	if(ParseCommaGetInt(buffer,ParseCommaGetNumParam(buffer),&startreg,&d[0],&d[1],&d[2],&d[3],&d[4],&d[5],&d[6],&d[7],&d[8],&d[9],&d[10],&d[11],&d[12],&d[13],&d[14],&d[15]))
		return 2;

	fprintf(file_pri,"Write: register=%02X data: ",startreg);
	for(int i=0;i<n;i++)
		fprintf(file_pri,"%02x ",d[i]);
	fprintf(file_pri,"\n");

	for(int i=0;i<n;i++)
	{
		mpu_writereg_wait_poll_block(startreg,d[i]);
	}


	fprintf(file_pri,"OK\n");


	return 0;
}
