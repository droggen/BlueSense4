/*
 * max31343.c
 *
 *  Created on: 27 juil. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include <time.h>
#include "i2c/megalol_i2c.h"
#include "max31343.h"
#include "stmrtc.h"
#include "i2c.h"
#include "usrmain.h"
#include "global.h"

/******************************************************************************
	file: max31343
*******************************************************************************


	*Hardware characteristics*
	- RTC time changes synchronously with the rising edge of the SQW signal
	- EXTI2 interrupt on rising edge of PB2 (SQW) change



	* Main functions*







TODO:

******************************************************************************/

// Indicates if power is lost
unsigned __max31343_powerlost=0;

// Custom function to call in the RTC interrupt handler every second
void (*max31343_irqhandler)()=0;

//
I2C_TRANSACTION __max31343_trans_sel_reg;							// I2C transaction to select status register
I2C_TRANSACTION __max31343_trans_read_status;						// I2C transaction to read RTC status, and clear RTC interrupts



/******************************************************************************
	function: max31343_isok
*******************************************************************************
	Checks if an I2C device responds at the RTC I2C address.


	Parameters:
		-

	Returns:
		0			-	Error
		1			-	Ok

******************************************************************************/
unsigned char max31343_isok()
{
	unsigned char d;


	unsigned char rv = i2c_readreg(MAX31343_ADDRESSS,0,&d);

	if(rv!=0)
		return 0;

	return 1;
}
/******************************************************************************
	function: max31343_init
*******************************************************************************
	Initialise the RTC.

	There is little to do except disabling interrupts, and enabling the IRQ for
	EXTI2.


	Parameters:
		-


	Returns:
		0			-	Ok
		Nonzero		-	Error

******************************************************************************/
unsigned char max31343_init()
{
	unsigned char d;

	// Read the status register - this clears possible interrupts
	i2c_readreg(MAX31343_ADDRESSS,0,&d);
	(void)d;

	// Initialise the I2C transactions to read the status register in background
	// Register selection transaction: select register 0
	i2c_transaction_setup(&__max31343_trans_sel_reg,MAX31343_ADDRESSS,I2C_WRITE,0,1);		// No stop bit
	__max31343_trans_sel_reg.data[0]=0x00;													// Register 0: status
	// Register read transaction: read status reg
	i2c_transaction_setup(&__max31343_trans_read_status,MAX31343_ADDRESSS,I2C_READ,1,1);	// Stop bit; read 1 byte
	__max31343_trans_read_status.callback=__max31343_trans_read_status_done;


	// Disable all interrupts
	max31343_writereg(0x01,0b1000000);		// Disable oscillator flag, disable all interrupts
	// RTC_config1:
	// Default settings are OK.
	// RTC_config2:
	// Default settings are OK. Square wave is always active.

	/*
	Assume initialisation is done with CubeMX: ETI2 interrupt, rising edge, pull-up
	// Check current external interrupt: SYSCFG->EXTICR[0] must be 0x100
	uint32_t temp = SYSCFG->EXTICR[0];
	fprintf(file_pri,"EXTICR1: %08X\n",temp);

	// Interrupt mask register: EXTI->IMR1 bit 2 must be set
	temp = EXTI->IMR1;
	fprintf(file_pri,"IMR1: %08X\n",temp);
	// Edge select
	temp = EXTI->RTSR1;
	fprintf(file_pri,"RTSR1: %08X\n",temp);
	temp = EXTI->FTSR1;
	fprintf(file_pri,"FTSR1: %08X\n",temp);
	*/

	// Enable line: this is necessary as not done by CubeMX
	NVIC_SetPriority(EXTI2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(EXTI2_IRQn);




	return 0;
}

void max31343_printregs(FILE *f)
{
	unsigned char d,rv;
	fprintf(f,"MAX31343 RTC Registers:\n");

	// Read main registers
	fprintf(f,"\tConfig registers:\n");
	for(unsigned int i=0;i<=0x1C;i++)
	{
		fprintf(file_pri,"\tReg %02X: ",i);
		rv = i2c_readreg(MAX31343_ADDRESSS,i,&d);
		if(rv)
			fprintf(file_pri,"KO\t");
		else
			fprintf(file_pri,"%02X\t",d);
		if((i%8)==7 || i==0x1C)
			fprintf(file_pri,"\n");
	}
	fprintf(f,"\tRAM registers:\n");
	int c=0;
	for(unsigned int i=0x22;i<=0x61;i++)
	{
		fprintf(file_pri,"\tReg %02X: ",i);
		rv = i2c_readreg(MAX31343_ADDRESSS,i,&d);
		if(rv)
			fprintf(file_pri,"KO\t");
		else
			fprintf(file_pri,"%02X\t",d);
		if(((c++)%8)==7)
			fprintf(file_pri,"\n");
	}

}
/******************************************************************************
	function: max31343_writereg
*******************************************************************************
	Writes one byte to a specific register



	Parameters:


	Returns:
		0			-	Ok
		Nonzero		-	Error

******************************************************************************/
unsigned char max31343_writereg(unsigned char r,unsigned char val)
{
	return i2c_writereg(MAX31343_ADDRESSS,r,val);
}

/******************************************************************************
	function: max31343_readreg
*******************************************************************************
	Reads one byte from a specific register



	Parameters:


	Returns:
		0			-	Ok
		Nonzero		-	Error

******************************************************************************/
unsigned char max31343_readreg(unsigned char r,unsigned char *val)
{
	unsigned char rv = i2c_readreg(MAX31343_ADDRESSS,r,val);
	return rv;
}


unsigned char max31343_readdatetime_conv_int(unsigned char sync, unsigned char *hour,unsigned char *min,unsigned char *sec,unsigned char *weekday,unsigned char *day,unsigned char *month,unsigned char *year)
{
	if(sync)
	{
		max31342_sync();
	}

	// Read multiple registers
	unsigned char regs[7];
	i2c_readregs(MAX31343_ADDRESSS,0x06,regs,7);

	// Print register content
	/*for(int i=0;i<7;i++)
		fprintf(file_pri,"%02X ",regs[i]);
	fprintf(file_pri,"\n");*/


	if(hour)
		*hour = MAX31341_BCD2BIN(regs[2]);
	if(min)
		*min = MAX31341_BCD2BIN(regs[1]);
	if(sec)
		*sec = MAX31341_BCD2BIN(regs[0]);

	if(year)
		*year = MAX31341_BCD2BIN(regs[6]);
	if(weekday)
		*weekday = regs[3]&0b111;
	if(month)
		*month = MAX31341_BCD2BIN(regs[5]);
	if(day)
		*day = MAX31341_BCD2BIN(regs[4]);



	return 0;
}

/******************************************************************************
	function: max31343_writetime
*******************************************************************************
	Writes the time to the RTC.

	Disables write protection and enters initialisation mode, updates,
	and leaves init and re-protect

	Range of values:
		sec € [0;59]
		min € [0;59]
		hour € [0;23]

	Return value:
		0:		Success
		other:	Error
******************************************************************************/
unsigned char max31343_writetime(unsigned int hour,unsigned int min,unsigned int sec)
{
	// Format the data
	unsigned char regs[3];

	regs[0] = MAX31341_BIN2BCD(sec);
	regs[1] = MAX31341_BIN2BCD(min);
	regs[2] = MAX31341_BIN2BCD(hour);

	unsigned char rv=i2c_writeregs(MAX31343_ADDRESSS,0x06,regs,3);

	HAL_Delay(MAX31341_WAITTIME);	// Time is updated 1s after writing to registers according to datasheet

	return rv;
}

/******************************************************************************
	max31343_writedate
*******************************************************************************
	Writes the date in day,month,year to the RTC.
	Range of values:
		day € [1;7]
		date € [1;30]
		month € [1;12]
		year € [0;99]

	Return value:
		0:			Success
		other:	Error
******************************************************************************/
unsigned char max31343_writedate(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year)
{
	// Format the data
	unsigned char regs[4];

	regs[0] = weekday;
	regs[1] = MAX31341_BIN2BCD(day);
	regs[2] = MAX31341_BIN2BCD(month);
	regs[3] = MAX31341_BIN2BCD(year);

	unsigned char rv=i2c_writeregs(MAX31343_ADDRESSS,0x09,regs,4);

	HAL_Delay(MAX31341_WAITTIME);	// Time is updated 1s after writing to registers according to datasheet

	return rv;
}
/******************************************************************************
	function: max31343_writedatetime
*******************************************************************************
	Writes the date and time to the RTC.



	Range of values:
		day € [1;7]
		date € [1;30]
		month € [1;12]
		year € [0;99]
		sec € [0;59]
		min € [0;59]
		hour € [0;23]

	Return value:
		0:		Success
		other:	Error
******************************************************************************/
unsigned char max31343_writedatetime(unsigned char weekday,unsigned char day,unsigned char month,unsigned char year,unsigned int hour,unsigned int min,unsigned int sec)
{
	// Format the data
	unsigned char regs[7];

	regs[0] = MAX31341_BIN2BCD(sec);
	regs[1] = MAX31341_BIN2BCD(min);
	regs[2] = MAX31341_BIN2BCD(hour);
	regs[3] = weekday;
	regs[4] = MAX31341_BIN2BCD(day);
	regs[5] = MAX31341_BIN2BCD(month);
	regs[6] = MAX31341_BIN2BCD(year);

	unsigned char rv=i2c_writeregs(MAX31343_ADDRESSS,0x06,regs,7);

	HAL_Delay(MAX31341_WAITTIME);	// Time is updated 1s after writing to registers according to datasheet

	return rv;
}


/******************************************************************************
	function: max31343_swrst
*******************************************************************************
	Software reset of the RTC.

	Must be called with reset=1 then reset=0.

	On reset, the oscillator is stopped, and the date/time registers are reset.



	Parameters:
		reset		-	1 to reset; 0 to clear reset


	Returns:
		0			-	Ok
		Nonzero		-	Error

******************************************************************************/
unsigned char max31343_swrst(unsigned char reset)
{
	reset=reset?1:0;
	// Set or clear SWRST
	max31343_writereg(0x02,reset);



	return 0;
}


/******************************************************************************
	function: EXTI2_IRQHandler
*******************************************************************************
	Square wave interrupt



	Parameters:
		-


	Returns:
		0			-	Ok
		Nonzero		-	Error

******************************************************************************/
void EXTI2_IRQHandler()
{
	// Must deactivate the interrupt
	EXTI->PR1 = (1<<2);

	//fprintf(file_pri,"a\n");

	if(max31343_irqhandler)
		max31343_irqhandler();
}

void max31343_defaultirqhandler();
/******************************************************************************
	function: max31343_setirqhandler
*******************************************************************************
	Set the 1Hz IRQ handler


	Parameters:
		h		-		Custom IRQ handler
	Returns:
		-

******************************************************************************/
void max31343_setirqhandler(void (*h)(void))
{
	max31343_irqhandler = h;
}


/******************************************************************************
	function: max31343_alarm_at
*******************************************************************************
	Set an alarm at the specified date/time.


	Parameters:
		-
	Returns:
		-

******************************************************************************/
unsigned char max31343_alarm_at(unsigned char day, unsigned char month, unsigned char year,unsigned char hour,unsigned char min,unsigned char sec)
{
	unsigned char v[6],r;

	// Alarm 1: 0x0D-0x12 is time and date
	// Upper bit of registers are used to mask matching criteria; all set to zero to have exact match.
	v[0] = MAX31341_BIN2BCD(sec);
	v[1] = MAX31341_BIN2BCD(min);
	v[2] = MAX31341_BIN2BCD(hour);
	v[3] = MAX31341_BIN2BCD(day);
	v[4] = MAX31341_BIN2BCD(month);
	v[5] = MAX31341_BIN2BCD(year);
	r = i2c_writeregs(MAX31343_ADDRESSS,0x0d,v,6);

	// Activate the alarm 1 interrupt
	max31343_writereg(1,65);						// DSOF and ALM1IE

	return 0;
}


unsigned char max31343_alarm_in(unsigned short insec)
{
	unsigned char r __attribute__((unused));
	unsigned char hour,min,sec;
	unsigned char day,month,year;

	max31343_readdatetime_conv_int(1,&hour,&min,&sec,0,&day,&month,&year);

	// Use the C lib to normalise and add seconds
	struct tm now;
	now.tm_hour = hour;
	now.tm_min = min;
	now.tm_sec = sec;
	now.tm_year = 100+year;		// Year is since 1900
	now.tm_mon = month-1;		// 0-based
	now.tm_mday = day;			// 1-based
	now.tm_isdst = 0;			// Daylight saving time - unused
	now.tm_yday = 0;			// Day of the year - used in asctime for display purposes only
	now.tm_wday = 0;			// Day since sunday - used in asctime for display purposes only

	mktime(&now);      			// Normalize time. Calculates the name of the weekday from the date

	fprintf(file_pri,"Current date/time: %s\n",asctime(&now));

	// Add the alarm delay

	struct tm alm = now;
	alm.tm_sec += insec;

	mktime(&alm);				// Normalize

	fprintf(file_pri,"Alarm date/time: %s\n",asctime(&alm));


	// Set-up the alarm. Month day is 0-based; year is 1900 based
	max31343_alarm_at(alm.tm_mday,alm.tm_mon+1,alm.tm_year-100,alm.tm_hour,alm.tm_min,alm.tm_sec);



	return 0;
}

/******************************************************************************
	__max31343_trans_read_status_done
*******************************************************************************
	Callback called by the I2C engine when an interrupt-read completes.
	Do not call from user code.

	Called when the status register is read in background.


	Parameters:
		t		-	Current transaction
	Returns:
		-
******************************************************************************/
unsigned char __max31343_trans_read_status_done(I2C_TRANSACTION *t)
{
	(void)t;

	fprintf(file_pri,"RTC status: %02X\n",t->data[0]);
	//for(int i=0;i<7;i++)
		//fprintf(file_pri,"%02X ",t->data[i]);
	//fprintf(file_pri,"\n");

	return 0;
}
/******************************************************************************
	max31343_background_read_status
*******************************************************************************
	Initiates a background read of the RTC status register.
	Can be called from interrupts.
	Used to test/reset the RTC interrupt when an RTC alarm occurs.

	Must be called after max31343_init which initialises the I2C transactions.

	No error checking - should not fail if not called at high frequency.

	Parameters:
		-
	Returns:
		-

******************************************************************************/
void max31343_background_read_status()
{
	unsigned char r = i2c_transaction_queue(2,0,&__max31343_trans_sel_reg,&__max31343_trans_read_status);

	if(r)
	{
		// Failed to queue the transactions
		fprintf(file_pri,"Failed to bgd read rtc status\n");
	}
}

/******************************************************************************
	max31342_sync
*******************************************************************************
	Wait for a rising edge of the SQW signal. Seconds change at the rising
	edge.

	Parameters:
		-
	Returns:
		-

******************************************************************************/
void max31342_sync()
{
#if 0
		// Wait for a change of second by continuously polling the RTC
		unsigned char s1;
		max31343_readdatetime_conv_int(0,0,0,&s1,0,0,0,0);
		unsigned char s2 = s1;
		while(s2==s1)
			max31343_readdatetime_conv_int(0,0,0,&s2,0,0,0,0);
#else
		// Wait for a second change looking for rising edge of SQW
		// First wait for SQW to be low (loop while it's high)
		while(max31341_sqw()==1);
		// Now wait for SQW to rise (loop while it's low)
		while(max31341_sqw()==0);
		// A read immediately on or within <2ms from the rising edge of the clock still reports the 'old' second -> wait 4ms.
		HAL_Delay(4);
#endif



}
/******************************************************************************
	max31343_get_temp
*******************************************************************************
	Returns the temperature x 100 (i.e. 1 LSB is 0.01deg).

	Parameters:
		-
	Returns:
		temperaturex100

******************************************************************************/
int max31343_get_temp()
{
	// Read temp registers
	unsigned char regs[2];
	i2c_readregs(MAX31343_ADDRESSS,0x1A,regs,2);

	//fprintf(file_pri,"%02X %02X\n",regs[0],regs[1]);

	// Combine both registers
	unsigned short tr = regs[0];
	tr<<=8;
	tr|=regs[1];

	//fprintf(file_pri,"tr: %04X\n",tr);

	// Cast to signed
	signed short trs = (signed short)tr;

	//fprintf(file_pri,"trs: %04X\n",trs);

	// Discard low 6 bits. LSB will be 0.25deg
	trs>>=6;

	//fprintf(file_pri,"trs: %04X\n",trs);

	// Convert to LSB=0.01deg
	trs*=25;

	//fprintf(file_pri,"trs: %04X\n",trs);

	return trs;

}


/******************************************************************************
	max31341_sqw
*******************************************************************************
	Returns the square wave signal

	Parameters:
		-
	Returns:
		0		-		sqw is low (microcontroller input pin is 0)
		1		-		sqw is hgh (microcontroller input pin is 1)

******************************************************************************/
unsigned char max31341_sqw()
{
	return (HAL_GPIO_ReadPin(RTC_SQW_GPIO_Port,RTC_SQW_Pin)==GPIO_PIN_RESET)?0:1;
}
