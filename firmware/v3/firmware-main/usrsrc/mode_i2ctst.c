/*
 * mode-i2ctst.c
 *
 *  Created on: 30 sept. 2019
 *      Author: droggen
 */



#include <mode_i2ctst.h>
#include "global.h"
#include "command.h"
#include "commandset.h"
#include "main.h"
#include "usrmain.h"
#include "global.h"
#include "m24xx.h"
#include "helper.h"
#include "i2c.h"
#include "i2c/stmi2c-isr.h"
#include "i2c/stmi2c.h"
#include "i2c/i2c_transaction.h"
#include "i2c/i2c_poll.h"
#include "i2c/megalol_i2c.h"
#include "wait.h"

//const char help_rtc_mwritebackupreg[] ="W,<addr>,<word>: Write 32-bit <word> in backup register <addr>";
const char help_i2c_init10[] ="Init 10KHz";
const char help_i2c_init100[] ="Init 100KHz";
const char help_i2c_init400[] ="Init 400KHz";
const char help_i2c_printreg[] ="Print registers";
const char help_i2c_initfrq[] ="I,frq,dummy (dummy 0=2, 1=16/9)";
const char help_i2c_init2[] ="init 1khz with interrupts";
const char help_i2c_deinitint[] ="disable interrutps";
const char help_i2c_tst[] = "tests";
const char help_i2c_rst[] = "Software reset";
const char help_i2c_write[] = "w,addr7,dostop,byte1,byte2,byte3,... (maximum 16 bytes)";
const char help_i2c_read[] = "r,addr7,n,dostop  (maximum 16 bytes)";
const char help_i2c_reset[] = "Software reset of the I2C peripheral";

const COMMANDPARSER CommandParsersI2CTST[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
//
	{'I', CommandParserI2CTstInitFrq,help_i2c_initfrq},
	{'i', CommandParserI2CTstInit2,help_i2c_init2},
	{'d', CommandParserI2CTstDeinitInt,help_i2c_deinitint},
	{'0', CommandParserI2CTstInit10,help_i2c_init10},
	{'1', CommandParserI2CTstInit100,help_i2c_init100},
	{'4', CommandParserI2CTstInit400,help_i2c_init400},
	{'P', CommandParserI2CTstPrintReg,help_i2c_printreg},
	{'R', CommandParserI2CTstReset,help_i2c_reset},
	{'q', CommandParserI2CTst1,help_i2c_tst},
	{'u', CommandParserI2CTst2,help_i2c_tst},
	{'a', CommandParserI2CTst3,help_i2c_tst},
	{'p', CommandParserI2CTst4,help_i2c_tst},
	{'l', CommandParserI2CTst5,help_i2c_tst},
	{'v', CommandParserI2CTst6,help_i2c_tst},
	{'X', CommandParserI2CReset,help_i2c_rst},
	{'w', CommandParserI2CWrite,help_i2c_write},
	{'r', CommandParserI2CRead,help_i2c_read},

};
const unsigned char CommandParsersI2CTSTNum=sizeof(CommandParsersI2CTST)/sizeof(COMMANDPARSER);


/******************************************************************************
	function:
*******************************************************************************
	Parameters:
		buffer	-		Pointer to the command string
		size	-		Size of the command string

	Returns:
		0		-		Success
		1		-		Message execution error (message valid)
		2		-		Message invalid

******************************************************************************/
unsigned char CommandParserI2CTstInitFrq(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4

	unsigned char rv;
	int f,d;

	rv = ParseCommaGetInt((char*)buffer,2,&f,&d);
	if(rv)
		return 2;



	fprintf(file_pri,"Init %dHz dummy: %d... ",f,d);

	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = f;
	if(d==0)
		hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	else
		hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_16_9;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		fprintf(file_pri,"Fail\n");
	else
		fprintf(file_pri,"OK\n");

	i2c_printconfig();
#endif
	return 0;
}
unsigned char CommandParserI2CTstInit10(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4

	fprintf(file_pri,"Init 10KHz... ");

	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 10000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		fprintf(file_pri,"Fail\n");
	else
		fprintf(file_pri,"OK\n");

	i2c_printconfig();
#endif

#if 0
	fprintf(file_pri,"Init 10KHz... ");

	hi2c1.Instance = I2C1;
	hi2c1.Init.
	hi2c1.Init.ClockSpeed = 10000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		fprintf(file_pri,"Fail\n");
	else
		fprintf(file_pri,"OK\n");

	i2c_printconfig();

#endif

	return 0;
}
unsigned char CommandParserI2CTstInit100(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4
	fprintf(file_pri,"Init 100KHz... ");

	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		fprintf(file_pri,"Fail\n");
	else
		fprintf(file_pri,"OK\n");

	i2c_printconfig();
#endif
	return 0;
}
unsigned char CommandParserI2CTstInit400(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4
	fprintf(file_pri,"Init 400KHz... ");

	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 400000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	//hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_16_9;			// Cube allows both settings
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
		fprintf(file_pri,"Fail\n");
	else
		fprintf(file_pri,"OK\n");

	i2c_printconfig();
#endif
	return 0;
}
unsigned char CommandParserI2CTstPrintReg(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
	i2c_printconfig();
	return 0;
}
unsigned char CommandParserI2CTstInit2(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4
	fprintf(file_pri,"Init 2\n");
	i2c_init();

	i2c_printconfig();
#endif
	return 0;
}
unsigned char CommandParserI2CTstDeinitInt(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4
	i2c_deactivateinterrupts();
#endif
	return 0;
}
unsigned char CommandParserI2CTst1(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4
	fprintf(file_pri,"Generate a start condition\n");

	//I2C1->CR1|=(1<<8);

	// Memory: 7-bit address 0x51
	// mode: 0=write, 1=read
	//i2c_transaction_setup_dbg(0x51,unsigned char mode,1,5)
	//i2c_transaction_setup_dbg(0x51,1,1,5);		// read 5 bytes
	//i2c_transaction_setup_dbg(0x51,1,1,1);		// read 1 bytes

	//i2c_transaction_setup_dbg(0x51,0,1,3);		// write 1 byte
	i2c_transaction_setup_dbg(0x51,0,1,6);		// write 1 byte
	//i2c_transaction_setup_dbg(0x50,0,1,2);		// write 1 byte

	// Byte to write:
	i2c_data[0] = 0x00;
	i2c_data[1] = 0x00;
	i2c_data[2] = 0x60;
	i2c_data[3] = 0x61;
	i2c_data[4] = 0x62;
	i2c_data[5] = 0x63;

	fprintf(file_pri,"idle: %d\n",_i2c_transaction_idle);

	// start the transaction
	if(_i2c_transaction_idle)
	{
		//TWI_vect_intx();
		fprintf(file_pri,"calling sm\n");
		HAL_Delay(100);
		//I2C_statemachine_internal();
	}

	HAL_Delay(1000);
	fprintf(file_pri,"I2C wait\n");


	while(!_i2c_transaction_idle);


	fprintf(file_pri,"not idle\n");

	fprintf(file_pri,"SR %04X %04X\n",(unsigned)I2C1->SR1,(unsigned)I2C1->SR2);

	for(int i=0;i<10;i++)
		fprintf(file_pri,"%02x ",i2c_data[i]);
	fprintf(file_pri,"\n");
#endif
	return 0;
}

unsigned char CommandParserI2CTst2(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;
#if 0
	// STM32F401
#warning Must be translated to STM32L4
	fprintf(file_pri,"Write address\n");

	//i2c_transaction_setup_dbg(0x51,0,1,2);		// write 1 byte
	i2c_transaction_setup_dbg(0x51,0,0,2);		// write 1 byte
	// Byte to write:
	i2c_data[0] = 0x00;
	i2c_data[1] = 0x00;

	fprintf(file_pri,"idle: %d\n",_i2c_transaction_idle);
	// start the transaction
	if(_i2c_transaction_idle)
	{
		//TWI_vect_intx();
		fprintf(file_pri,"calling sm\n");
		HAL_Delay(100);
		//I2C_statemachine_internal();
	}
	HAL_Delay(100);
	//fprintf(file_pri,"I2C wait\n");
	while(!_i2c_transaction_idle);
	fprintf(file_pri,"-------------------------Idle--------------------\n");

	//while(1)
	{
		fprintf(file_pri,"SR %04X %04X\n",(unsigned)I2C1->SR1,(unsigned)I2C1->SR2);
		//HAL_Delay(10);
	}

	fprintf(file_pri,"+++++++++++++++++++++++Read data+++++++++++++++++++++++\n");


	//I2C1->CR1|=(1<<8);

	// Memory: 7-bit address 0x51
	// mode: 0=write, 1=read
	//i2c_transaction_setup_dbg(0x51,unsigned char mode,1,5)
	//i2c_transaction_setup_dbg(0x51,1,1,5);		// read 5 bytes
	i2c_transaction_setup_dbg(0x51,1,1,4);		// read 1 bytes

	//i2c_transaction_setup_dbg(0x51,0,1,3);		// write 1 byte
	//i2c_transaction_setup_dbg(0x50,0,1,2);		// write 1 byte

	// Byte to write:
	/*i2c_data[0] = 0x00;
	i2c_data[1] = 0x00;
	i2c_data[2] = 0x79;*/

	fprintf(file_pri,"idle: %d\n",_i2c_transaction_idle);

	// start the transaction
	if(_i2c_transaction_idle)
	{
		//TWI_vect_intx();
		fprintf(file_pri,"calling sm\n");
		HAL_Delay(100);
		//I2C_statemachine_internal();
	}

	HAL_Delay(1000);
	fprintf(file_pri,"I2C wait\n");



	for(int i=0;i<10;i++)
		fprintf(file_pri,"%02x ",i2c_data[i]);
	fprintf(file_pri,"\n");

	while(!_i2c_transaction_idle);


	fprintf(file_pri,"not idle\n");
	fprintf(file_pri,"SR %04X %04X\n",(unsigned)I2C1->SR1,(unsigned)I2C1->SR2);
#endif
	return 0;
}
unsigned char CommandParserI2CTst3(char *buffer,unsigned char size)
{
	//(void) size; (void) buffer;

	I2C_TRANSACTION t1,t2,t3,t4;
	unsigned char rv;

	// Initialise the first transaction: write address
	//i2c_transaction_setup(&t1,0x55,I2C_WRITE,1,2);
	i2c_transaction_setup(&t1,0x55,I2C_READ,1,2);
	//i2c_transaction_setup(&t1,0x51,I2C_WRITE,1,2);
	t1.data[0] = 0x00;		// Address
	t1.data[1] = 0x01;

	// Initialise the second transaction: read data
	i2c_transaction_setup(&t2,0x51,I2C_READ,1,5);
	memset(t2.data,0x55,16);

	fprintf(file_pri,"Queue\n");
	HAL_Delay(100);
	rv = i2c_transaction_queue(1,1,&t1);
	//rv = i2c_transaction_queue(4,1,&t1,&t2,&t3,&t4);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");


	HAL_Delay(500);
	// get result of transactio
	fprintf(file_pri,"%X %X\n",t1.status,t1.i2cerror);

	return 0;
}
unsigned char CommandParserI2CTst4(char *buffer,unsigned char size)
{
	// STM32L4
	fprintf(file_pri,"CR1: %04X CR2: %04X ISR: %04X\n",I2C1->CR1,I2C1->CR2,I2C1->ISR);



	I2C1->CR1&=(0xFFFFFFFF & 0b00000001);		// Deactivate interrupts: clear bit 1-7

	//fprintf(file_pri, "idle: %d\n",_i2c_transaction_idle);

	int b=0;

	/*if(ParseCommaGetNumParam(buffer)!=1)
		return 2;

	ParseCommaGetInt((char*)buffer,1,&b);*/

	I2C_TRANSACTION t1,t2,t3,t4;
	unsigned char rv;

	// Initialise the first transaction: write address
	//i2c_transaction_setup(&t1,0x51,I2C_WRITE,0,3);
	i2c_transaction_setup(&t1,0x51,I2C_READ,0,3);
	memset(t1.data,0,16);
	/*t1.data[0] = 0x00;		// Address
	t1.data[1] = 0x01;
	t1.data[2] = 10;*/



	i2c_transaction_setup(&t2,0x51,I2C_READ,0,1);
	memset(t2.data,0,16);

	i2c_transaction_setup(&t3,0x51,I2C_READ,1,1);
	memset(t3.data,0,16);

	i2c_transaction_setup(&t4,0x51,I2C_READ,1,1);
	memset(t4.data,0,16);

	fprintf(file_pri,"Queue\n");
	HAL_Delay(100);
	//rv = i2c_transaction_queue(1,1,&t1);
	rv = i2c_transaction_queue(4,1,&t1,&t2,&t3,&t4);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");

	//HAL_Delay(100);

	/*rv = i2c_transaction_queue(1,1,&t2);
	//rv = i2c_transaction_queue(2,1,&t1,&t2);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");*/

	HAL_Delay(500);

	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t1.data[i]);
	fprintf(file_pri,"\n");
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t2.data[i]);
	fprintf(file_pri,"\n");
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t3.data[i]);
	fprintf(file_pri,"\n");
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t4.data[i]);
	fprintf(file_pri,"\n");

/*
 * M24XX_ADDRESS: 0x51
 */

	return 0;


	unsigned char d[16];

	memset(d,0,16);
	//d[2] = b;

	//i2c_poll_write(0x51,3,d,1);

	//i2c_poll_write(0x55,3,d,1);			// Non-existant peripheral

	//i2c_poll_write(0x51,1,d,2);


	HAL_Delay(100);

	// Read

	fprintf(file_pri,"\n\nNow reading data\n\n\n");

	memset(d,0x55,16);

	i2c_poll_read(0x51,3,d,1);
	//i2c_poll_read(0x55,4,d,0);

	for(int i=0;i<3;i++)
		fprintf(file_pri,"%02X ",d[i]);
	fprintf(file_pri,"\n");



	return 0;
}
unsigned char CommandParserI2CTst5(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;


	// STM32L4
	fprintf(file_pri,"CR1: %04X CR2: %04X ISR: %04X\n",I2C1->CR1,I2C1->CR2,I2C1->ISR);
	I2C1->CR1&=(0xFFFFFFFF & 0b00000001);		// Deactivate interrupts: clear bit 1-7

	I2C_TRANSACTION t1,t2,t3,t4;
	unsigned char rv;

	// Initialise the first transaction: write address
	i2c_transaction_setup(&t1,0x51,I2C_WRITE,1,3);
	memset(t1.data,0xaa,16);
	t1.data[0] = 0x00;		// Address
	t1.data[1] = 0x00;
	t1.data[2] = 10;
	t1.data[3] = 11;
	t1.data[4] = 12;


	i2c_transaction_setup(&t2,0x51,I2C_READ,0,3);
	//i2c_transaction_setup(&t2,0x51,I2C_WRITE,1,3);
	memset(t2.data,0xaa,16);
	/*t2.data[0]=0x1;
	t2.data[1]=0x0;
	t2.data[2]=0x95;*/


	// Initialise the first transaction: write address
	i2c_transaction_setup(&t3,0x51,I2C_READ,0,1);
	//i2c_transaction_setup(&t3,0x51,I2C_WRITE,0,1);
	memset(t3.data,0xaa,16);

	// Initialise the first transaction: write address
	i2c_transaction_setup(&t4,0x51,I2C_READ,1,1);
	//i2c_transaction_setup(&t3,0x51,I2C_WRITE,0,1);
	memset(t4.data,0xaa,16);


	// Initialise the first transaction: write address
	/*i2c_transaction_setup(&t3,0x51,I2C_WRITE,1,1);
	memset(t3.data,0xaa,16);
	t3.data[0]=0x88;*/




	fprintf(file_pri,"Queue\n");
	HAL_Delay(100);
	//rv = i2c_transaction_queue(1,1,&t1);
	//rv = i2c_transaction_queue(2,1,&t1,&t2);
	rv = i2c_transaction_queue(4,1,&t1,&t2,&t3,&t4);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");


	HAL_Delay(500);
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t1.data[i]);
	fprintf(file_pri,"\n");
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t2.data[i]);
	fprintf(file_pri,"\n");
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t3.data[i]);
	fprintf(file_pri,"\n");
	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02X ",t4.data[i]);
	fprintf(file_pri,"\n");


	/*I2C_TRANSACTION t2;


	// Initialise the first transaction: write address
	//i2c_transaction_setup(&t2,0x51,I2C_READ,0,10);
	i2c_transaction_setup(&t2,0x51,I2C_READ,0,3);
	memset(t2.data,0xaa,16);

	fprintf(file_pri,"\n\nNow read transaction\n\n\n");

	HAL_Delay(100);
	rv = i2c_transaction_queue(1,1,&t2);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");

	for(int i=0;i<16;i++)
			fprintf(file_pri,"%02x ",t2.data[i]);
		fprintf(file_pri,"\n");*/


	return 0;
}
unsigned char CommandParserI2CTst6(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;



	// STM32L4
	fprintf(file_pri,"CR1: %04X CR2: %04X ISR: %04X\n",I2C1->CR1,I2C1->CR2,I2C1->ISR);
	I2C1->CR1&=(0xFFFFFFFF & 0b00000001);		// Deactivate interrupts: clear bit 1-7

	fprintf(file_pri, "idle: %d\n",_i2c_transaction_idle);




	I2C_TRANSACTION t1;
	unsigned char rv;

	// Initialise the first transaction: write address
	i2c_transaction_setup(&t1,0x64,I2C_WRITE,0,1);
	t1.data[0] = 0x04;		// Address

	/*fprintf(file_pri,"Queue\n");
	rv = i2c_transaction_queue(1,1,&t1);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");*/


	I2C_TRANSACTION t2;
	i2c_transaction_setup(&t2,0x64,I2C_READ,1,7);
	memset(t2.data,0xaa,16);

	//fprintf(file_pri,"\n\nNow read transaction\n\n\n");

	HAL_Delay(100);
	//rv = i2c_transaction_queue(1,1,&t2);
	rv = i2c_transaction_queue(2,1,&t1,&t2);
	if(rv)
		fprintf(file_pri,"Error\n");
	else
		fprintf(file_pri,"OK\n");

	for(int i=0;i<16;i++)
			fprintf(file_pri,"%02x ",t2.data[i]);
		fprintf(file_pri,"\n");

	return 0;

	fprintf(file_pri,"Try readregs\n");

	// Try same with readregs
	fprintf(file_pri,"readregs\n");
	unsigned char v[16];
	memset(v,0x44,16);
	unsigned short charge;
	unsigned char LTC2942_ADDRESS=0x64;

	rv = i2c_readregs(LTC2942_ADDRESS,0x04,v,14);

	for(int i=0;i<16;i++)
		fprintf(file_pri,"%02x ",v[i]);
	fprintf(file_pri,"\n");

	// Try same with readreg (singular)
	fprintf(file_pri,"readreg\n");
	for(int i=4;i<4+14;i++)
	{
		rv = i2c_readreg(LTC2942_ADDRESS,i,v);
		fprintf(file_pri,"%d: %02x\n",i,v[0]);
	}


	ltc2942_printreg(file_pri);


	return 0;
}

unsigned char CommandParserI2CReset(char *buffer,unsigned char size)
{
	(void) size; (void) buffer;

#if 0
	// STM32F401
#warning Must be translated to STM32L4

	fprintf(file_pri,"Prior to reset bit: CR1=%04X SR1=%04X SR2=%04X\n",I2C1->CR1,I2C1->SR1,I2C1->SR2);
	fprintf(file_pri,"Setting reset bit\n");
	I2C1->CR1|=0x8000;
	HAL_Delay(100);
	fprintf(file_pri,"After setting reset bit: CR1=%04X SR1=%04X SR2=%04X\n",I2C1->CR1,I2C1->SR1,I2C1->SR2);
	I2C1->CR1&=0x7fff;
	// Set the PE bit
	I2C1->CR1|=0x0001;

	// reinit
	MX_I2C1_Init();
	i2c_init();

	HAL_Delay(100);
	fprintf(file_pri,"After clearing reset bit: CR1=%04X SR1=%04X SR2=%04X\n",I2C1->CR1,I2C1->SR1,I2C1->SR2);

#endif

	return 0;
}


// LTC address: 0x64 100d
// r,100,10,1
// w,100,1,01;r,100,10,1
// EEPROM address: 81d
// w,addr7,dostop,byte1,byte2,byte3,...
// w,81,1,10,11,12,13,14
// w,81,1,00,00;r,81,10,1
unsigned char CommandParserI2CWrite(char *buffer,unsigned char size)
{
	int addr7,n,dostop;
	unsigned int d[16];



	//fprintf(file_pri,"num param: %d\n",ParseCommaGetNumParam(buffer));

	if(ParseCommaGetNumParam(buffer)>18 || ParseCommaGetNumParam(buffer)<2)
		return 2;

	n = ParseCommaGetNumParam(buffer)-2;

	if(ParseCommaGetInt(buffer,ParseCommaGetNumParam(buffer),&addr7,&dostop,&d[0],&d[1],&d[2],&d[3],&d[4],&d[5],&d[6],&d[7],&d[8],&d[9],&d[10],&d[11],&d[12],&d[13],&d[14],&d[15]))
		return 2;

	fprintf(file_pri,"Write: addr7=%02X n=%d stop=%d data: ",addr7,n,dostop);
	for(int i=0;i<n;i++)
		fprintf(file_pri,"%02x ",d[i]);
	fprintf(file_pri,"\n");


	I2C_TRANSACTION t1;
	unsigned char rv;

	// Initialise the first transaction: write address
	i2c_transaction_setup(&t1,addr7,I2C_WRITE,dostop,n);
	for(int i=0;i<n;i++)
		t1.data[i] = d[i];

	fprintf(file_pri,"Executing transaction\n");
	rv = i2c_transaction_queue(1,1,&t1);
	if(rv)
	{
		fprintf(file_pri,"Queuing error\n");
		return 1;
	}
	if(t1.status!=0)
	{
		fprintf(file_pri,"I2C error %04X\n",t1.i2cerror);
		return 1;
	}
	fprintf(file_pri,"OK\n");


	return 0;
}
// r,addr7,n,dostop
unsigned char CommandParserI2CRead(char *buffer,unsigned char size)
{
	int addr7,n,dostop;
	//fprintf(file_pri,"buffer: '%s'\n",buffer);



	if(ParseCommaGetInt(buffer,3,&addr7,&n,&dostop))
		return 2;
	fprintf(file_pri,"Read: addr7=%02X n=%d stop=%d\n",addr7,n,dostop);
	if(n>16)
	{
		fprintf(file_pri,"Error: max transaction is 16\n");
		return 2;
	}


	I2C_TRANSACTION t1;
	unsigned char rv;

	// Initialise the first transaction: write address
	i2c_transaction_setup(&t1,addr7,I2C_READ,dostop,n);

	fprintf(file_pri,"Executing transaction\n");
	rv = i2c_transaction_queue(1,1,&t1);
	if(rv)
	{
		fprintf(file_pri,"Queuing error\n");
		return 1;
	}
	if(t1.status!=0)
	{
		fprintf(file_pri,"I2C error %04X\n",t1.i2cerror);
		return 1;
	}
	fprintf(file_pri,"OK\n");

	for(int i=0;i<n;i++)
		fprintf(file_pri,"%02x ",t1.data[i]);
	fprintf(file_pri,"\n");



	return 0;
}
unsigned char CommandParserI2CTstReset(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	// Software reset of the I2C hardware - this may be suboptimal as error conditions could be recovered without reset.
	// This takes 3 APB clock cycles which is slow.
	I2C1->CR1&=0xFFFFFFFE;		// Clear PE
	while(I2C1->CR1&1);			// Loop until PE is clear - ensures a delay of 3 APB clock cycles according to datasheet
	I2C1->CR1|=0x1;				// Set PE

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void mode_i2ctst(void)
{

	fprintf(file_pri,"I2CTST>\n");


	while(1)
	{


		//fprintf(file_pri,"eeprom: call command process\n");

		while(CommandProcess(CommandParsersI2CTST,CommandParsersI2CTSTNum));

		if(CommandShouldQuit())
			break;


		//HAL_Delay(500);

	}
	fprintf(file_pri,"<I2CTST\n");
}

