/*
 * i2c_pol.c
 *
 *  Created on: 3 oct. 2019
 *      Author: droggen
 */

#include <stdio.h>
#include "main.h"
#include "i2c_poll.h"
#include "global.h"
#include "usrmain.h"

#if 0
void i2c_poll_write(unsigned char addr, unsigned char n, unsigned char *data,unsigned char stop)
{
	// Code path for STM32F401
	unsigned sr1,sr1b,sr2;
	unsigned ctr;


	I2C1->CR2=I2C1->CR2&0b1111100011111111;		// No interrupts
	//I2C1->CR2|=(1<<10)|(1<<9)|(1<<8);			// Enable interrupts



	fprintf(file_pri,"Send START\n");

	I2C1->CR1 |= 0x0100; // send START bit
	ctr=0;
	while (!(I2C1->SR1 & 0x0001)) {ctr++;}; // wait for START condition (SB=1)


	fprintf(file_pri,"(%u) Send ADR\n",ctr);

	I2C1->DR = (addr<<1)|0; // slave address -> DR & write
	ctr=0;
	while (!(I2C1->SR1 & 0x0002)) {ctr++;}; // wait for ADDRESS sent (ADDR=1)

	sr1 = I2C1->SR1; // adr flag
	sr2 = I2C1->SR2; // read status to clear flag
	sr1b = I2C1->SR1; // adr flag should be cleared

	fprintf(file_pri,"(%u) Adr: before %08X after %08X\n",ctr,sr1,sr1b);





	for(int i=0;i<n;i++)
	{
		fprintf(file_pri,"Send data %d\n",i);

		I2C1->DR = data[i]; // Address in chip -> DR & write
		ctr=0;
		while (!(I2C1->SR1 & 0x0080)) {ctr++;}; // wait for DR empty (TxE)
		fprintf(file_pri,"(%u)\n",ctr);
		/*ctr=0;
		while (!(I2C1->SR1 & 0x0080))
		{
			unsigned int cr1 = I2C1->CR1;
			unsigned int dr = I2C1->DR;
			sr1 = I2C1->SR1;
			sr2 = I2C1->SR2;
			ctr++;
			fprintf(file_pri,"(%u) CR: %04X DR: %04X SR: %04X %04x\n",ctr,cr1,dr,sr1,sr2);
			HAL_Delay(10);
		}; // wait for DR empty (TxE)
		*/

	}


	fprintf(file_pri,"Wait BTF\n");

	ctr=0;
	while (!(I2C1->SR1 & 0x0004)) {ctr++;}; // wait for Byte sent (BTF)

	fprintf(file_pri,"(%u) Send stop\n",ctr);

	I2C1->CR1 |= 0x0200; // send STOP bit

	ctr=0;
	while (I2C1->CR1 & 0x0200) {ctr++;}; // wait for STOP cleared
	fprintf(file_pri,"(%u) Done\n",ctr);
}
void i2c_poll_read(unsigned char addr, unsigned char n, unsigned char *data,unsigned char stop)
{
	// Code path for STM32F401
	unsigned sr1,sr1b,sr2;
	unsigned ctr;

	I2C1->CR2=I2C1->CR2&0b1111100011111111;		// No interrupts

	fprintf(file_pri,"Send START\n");

	I2C1->CR1 |= 0x0100; // send START bit
	ctr=0;
	while (!(I2C1->SR1 & 0x0001)) {ctr++;}; // wait for START condition (SB=1)

	fprintf(file_pri,"(%u) Send ADR\n",ctr);

	I2C1->DR = (addr<<1)|1; // slave address
	ctr=0;
	while (!(I2C1->SR1 & 0x0002)) {ctr++;}; // wait for ADDRESS sent (ADDR=1)

	sr1 = I2C1->SR1; // adr flag
	sr2 = I2C1->SR2; // read SR2 to clear flag
	sr1b = I2C1->SR1; // adr flag should be cleared

	fprintf(file_pri,"(%u) Adr: before %08X after %08X\n",ctr,sr1,sr1b);

	for(int i=0;i<n;i++)
	{
		ctr=0;
		while (!(I2C1->SR1 & 0x0040)) {ctr++;}; // wait for ByteReceived (RxNE=1)

		data[i] = I2C1->DR;

		fprintf(file_pri,"(%u) Rd %d: %02X\n",ctr,i,data[i]);

	}

	I2C1->CR1 |= 0x0200; // send STOP bit

	ctr=0;
	while (I2C1->CR1 & 0x0200) {ctr++;}; // wait for STOP cleared
	fprintf(file_pri,"(%u) Done\n",ctr);


}
#endif

void i2c_poll_write(unsigned char addr, unsigned char n, unsigned char *data,unsigned char stop)
{
	// Code path for STM32L4
	unsigned sr1,sr1b,sr2;
	unsigned ctr;


	I2C1->CR1&=(0xFFFFFFFF & 0b00000001);		// Deactivate interrupts: clear bit 1-7

	// Reference manual sec. 39.4.8
	// Master communication initialization (address phase)
	I2C1->CR2 = (addr<<1);		// Set slave address (bit 7:1)
	I2C1->CR2 |= (n<<16);		// Set number of bytes
	if(stop)
		I2C1->CR2 |= (1<<25);		// Auto-end=1 for stop condition after transfer
	else
		;							// No stop (restart condition): software end
	// Other CR2 parameters will be: no PEC transfer, reload disabled (max 255 bytes transfer), addr is 7 bit
	I2C1->CR2 |= (1<<13);		// Start


	ctr=0;

	while(1)
	{
		unsigned isr = I2C1->ISR;
		fprintf(file_pri,"ISR %08X\n",isr);
		if(isr & (1<<4))
		{
			// NACKF set - this signals an error if nack set after the address
			fprintf(file_pri,"NACKF\n");
			break;
		}
		if(isr & (1<<1))
		{
			// TXIS set
			fprintf(file_pri,"TXIS\n");
			// Write byte
			I2C1->TXDR = data[ctr];
			ctr++;
		}
		if(isr & (1<<6))
		{
			// TC set
			fprintf(file_pri,"TC\n");
			break;
		}
		if(isr & (1<<5))
		{
			// STOPF set
			fprintf(file_pri,"STOPF\n");

			// Clear the stopf bit
			I2C1->ICR|=(1<<5);
			break;
		}
		HAL_Delay(1);
	}
	fprintf(file_pri,"Done\n");


}


void i2c_poll_read(unsigned char addr, unsigned char n, unsigned char *data,unsigned char stop)
{
	// Code path for STM32FL4
	unsigned sr1,sr1b,sr2;
	unsigned ctr;

	I2C1->CR1&=(0xFFFFFFFF & 0b00000001);		// Deactivate interrupts: clear bit 1-7
	//I2C1->CR1|=0b11110111;		// Enable some interrupts: STOPIE RXIE NACKIE TCIE ERRIE


	// Reference manual sec. 39.4.8
	// Master communication initialization (address phase)
	I2C1->CR2 = (addr<<1);		// Set slave address (bit 7:1)
	I2C1->CR2 |= (n<<16);		// Set number of bytes
	if(stop)
		I2C1->CR2 |= (1<<25);		// Auto-end=1 for stop condition after transfer
	else
		;							// No stop (restart condition): software end
	I2C1->CR2 |= (1<<10);			// I2C read
	// Other CR2 parameters will be: no PEC transfer, reload disabled (max 255 bytes transfer), addr is 7 bit
	I2C1->CR2 |= (1<<13);		// Start


	ctr=0;

	while(1)
	{
		unsigned isr = I2C1->ISR;
		fprintf(file_pri,"ISR %08X\n",isr);
		if(isr & (1<<4))
		{
			// NACKF set
			fprintf(file_pri,"NACKF\n");
			// this signals an error. Clear bit
			I2C1->ICR|=(1<<4);		// Clear NACKF
			//break;
		}
		if(isr & (1<<2))
		{
			// RXNE set
			fprintf(file_pri,"RXNE\n");
			// Write byte
			data[ctr] = I2C1->RXDR;
			ctr++;
		}
		if(isr & (1<<6))
		{
			// TC set
			fprintf(file_pri,"TC\n");
			break;
		}
		if(isr & (1<<5))
		{
			// STOPF set
			fprintf(file_pri,"STOPF\n");

			// Clear the stopf bit
			I2C1->ICR|=(1<<5);
			break;
		}
		HAL_Delay(1);
	}
	fprintf(file_pri,"Done\n");




}
