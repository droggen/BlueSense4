/*
 * megalol_i2c.c
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

#include "i2c.h"
#include "stm32l4xx_hal_i2c.h"

// Select whether the HAL codepath or custom codepath is used.
#define I2C_USEHAL 0

#if I2C_USEHAL==1
#include "stm32l4xx_hal_i2c.h"
#else
#include "i2c_transaction.h"
#endif

#include <stdio.h>
#include "usrmain.h"
#include "global.h"


/******************************************************************************
	i2c_readreg
*******************************************************************************
	General purpose read I2C register, for most I2C devices.
	Operation: writes the register of interest, and performs a single byte read.

	Blocks until successful

	Return value:
		0:			Success
		nonzero:	Error (never happens)
******************************************************************************/
unsigned char i2c_readreg(unsigned char addr7,unsigned char reg,unsigned char *val)
{
#if I2C_USEHAL==1
	HAL_StatusTypeDef r;
	do
	{
		r = HAL_I2C_Mem_Read(&hi2c1,addr7<<1,reg,1,val,1,100);
	}
	while(r!=HAL_OK);
	return r;
#else
	I2C_TRANSACTION t1,t2;

	// Register selection transaction
	i2c_transaction_setup(&t1,addr7,I2C_WRITE,0,1);
	t1.data[0]=reg;
	// Register read transaction
	i2c_transaction_setup(&t2,addr7,I2C_READ,1,1);
	t2.extdata=val;


	unsigned char r;
	do
	{
		r = i2c_transaction_queue(2,1,&t1,&t2);
	} while(r);

	// Check error code
	if(t1.status!=0)
		return t1.i2cerror;
	if(t2.status!=0)
		return t2.i2cerror;
	return 0;
#endif
}
/******************************************************************************
	i2c_readreg_a16
*******************************************************************************
	General purpose read I2C register, for most I2C devices, with a 16-bit address.

	Operation: writes the register of interest (2 bytes), and performs a single byte read.

	Blocks until successful

	Parameters:
		reg			-		16 bit register. Register is transferred MSB first
		dostop		-		Generate a stop condition between register select and read

	Return value:
		0:			Success
		nonzero:	Error (never happens)
******************************************************************************/
unsigned char i2c_readreg_a16(unsigned char addr7,unsigned short reg,unsigned char *val,unsigned char dostop)
{
#if I2C_USEHAL==1
	HAL_StatusTypeDef r;
	do
	{
		r= HAL_I2C_Mem_Read(&hi2c1,addr7<<1,reg,2,val,1,100);
	}
	while(r!=HAL_OK);
	return r;
#else
	I2C_TRANSACTION t1,t2;

	// Register selection transaction

	i2c_transaction_setup(&t1,addr7,I2C_WRITE,dostop,2);
	t1.data[0]=reg>>8;
	t1.data[1]=reg;
	// Register read transaction
	i2c_transaction_setup(&t2,addr7,I2C_READ,1,1);
	t2.extdata=val;



	//fprintf(file_pri,"Trans 1\n");
	unsigned char r;
	do
	{
		r = i2c_transaction_queue(2,1,&t1,&t2);
		//r = i2c_transaction_queue(1,1,&t1);
	}
	while(r);
	/*fprintf(file_pri,"Trans 2\n");
	do
	{
		//r = i2c_transaction_queue(2,1,&t1,&t2);
		r = i2c_transaction_queue(1,1,&t2);
	}
	while(r);*/


	// Check error code
	if(t1.status!=0)
	{
		return t1.i2cerror;
	}
	if(t2.status!=0)
	{
		return t2.i2cerror;
	}
	return 0;
#endif
}
/******************************************************************************
	i2c_writereg
*******************************************************************************
	General purpose write an I2C register, for most I2C devices.
	Operation: writes the register of interest, writes the value

	Block until successful


	Return value:
		0:			Success
		nonzero:	Error (never happens)
******************************************************************************/
unsigned char i2c_writereg(unsigned char addr7,unsigned char reg,unsigned char val)
{
#if I2C_USEHAL==1
	HAL_StatusTypeDef r;
	do
	{
		r = HAL_I2C_Mem_Write(&hi2c1,addr7<<1,reg,1,&val,1,100);
	}
	while(r!=HAL_OK);
	return r;
#else
	I2C_TRANSACTION t1;


	// Transaction: select register, write, stop
	i2c_transaction_setup(&t1,addr7,I2C_WRITE,1,2);
	t1.data[0]=reg;
	t1.data[1]=val;

	unsigned char r;
	do
	{
		//printf("i2cwri queue\n");
		r = i2c_transaction_queue(1,1,&t1);
		//if(r)
			//printf("i2cwri fail %02X\n",r);
	} while(r);

	//printf("i2cwri done status: %02x error: %02x\n",t1.status,t1.i2cerror);
	// Check error code
	if(t1.status!=0)
		return t1.i2cerror;
	return 0;
#endif
}
/******************************************************************************
	i2c_writereg_a16
*******************************************************************************
	General purpose write an I2C register, for most I2C devices with 16-bit register.
	Operation: writes the register of interest, writes the value

	Block until successful


	Return value:
		0:			Success
		nonzero:	Error (never happens)
******************************************************************************/
unsigned char i2c_writereg_a16(unsigned char addr7,unsigned short reg,unsigned char val)
{
#if I2C_USEHAL==1
	HAL_StatusTypeDef r;
	do
	{
		r = HAL_I2C_Mem_Write(&hi2c1,addr7<<1,reg,2,&val,1,100);
	}
	while(r!=HAL_OK);
	return r;
#else
	I2C_TRANSACTION t1;


	// Transaction: select register, write, stop
	i2c_transaction_setup(&t1,addr7,I2C_WRITE,1,3);
	t1.data[0]=reg>>8;
	t1.data[1]=reg;
	t1.data[2]=val;

	unsigned char r;
	do
	{
		//printf("i2cwri queue\n");
		r = i2c_transaction_queue(1,1,&t1);
		//if(r)
			//printf("i2cwri fail %02X\n",r);
	} while(r);

	//printf("i2cwri done status: %02x error: %02x\n",t1.status,t1.i2cerror);
	// Check error code
	if(t1.status!=0)
		return t1.i2cerror;
	return 0;
#endif
}


/******************************************************************************
	i2c_readregs
*******************************************************************************
	General purpose read several I2C registers, for most I2C devices.
	Operation: writes the register of interest, and performs a multiple byte read.


	val: 	if zero result is in the transaction buffer;
			if nonzero then result is in val

	Block until successful

	Return value:
		zero:		Success
		nonzero:	Error (never happens)
******************************************************************************/
unsigned char i2c_readregs(unsigned char addr7,unsigned char reg,unsigned char *val,unsigned char n)
{
#if I2C_USEHAL==1
	HAL_StatusTypeDef r;
	do
	{
		r = HAL_I2C_Mem_Read(&hi2c1,addr7<<1,reg,1,val,n,100);
	}
	while(r!=HAL_OK);
	return r;
#else
	I2C_TRANSACTION t1,t2;

	// Register selection transaction
	i2c_transaction_setup(&t1,addr7,I2C_WRITE,0,1);
	t1.data[0]=reg;
	// Register read transaction
	i2c_transaction_setup(&t2,addr7,I2C_READ,1,n);
	t2.extdata=val;


	unsigned char r;
	do
	{
		r = i2c_transaction_queue(2,1,&t1,&t2);
	} while(r);

	// Check error code
	if(t1.status!=0)
		return t1.i2cerror;
	if(t2.status!=0)
		return t2.i2cerror;
	return 0;
#endif

}
/******************************************************************************
	i2c_writeregs
*******************************************************************************
	General purpose write an I2C register, for most I2C devices.
	Operation: writes the register address, writes the values

	Maximum number of registers: size of transaction-1

	Block until successful

	Return value:
		0:			Success
		nonzero:	Error (never happens)
******************************************************************************/
unsigned char i2c_writeregs(unsigned char addr7,unsigned char reg,unsigned char *val,unsigned char n)
{
#if I2C_USEHAL==1
	HAL_StatusTypeDef r;
	do
	{
		r = HAL_I2C_Mem_Write(&hi2c1,addr7<<1,reg,1,val,n,100);
	}
	while(r!=HAL_OK);
	return r;
#else
	I2C_TRANSACTION t1;

	// Transaction: select register, write, stop
	i2c_transaction_setup(&t1,addr7,I2C_WRITE,1,n+1);
	t1.data[0]=reg;
	for(unsigned char i=0;i<n;i++)
		t1.data[1+i]=val[i];

	unsigned char r;
	do
	{
	//	printf("queue ");
		r = i2c_transaction_queue(1,1,&t1);
		//if(r)
			//_delay_us(10);
		//if(r)
			//printf("fail\n");
	} while(r);

	// Check error code
	if(t1.status!=0)
		return t1.i2cerror;
	return 0;
#endif
}

