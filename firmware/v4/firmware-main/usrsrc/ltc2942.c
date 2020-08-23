#include <stdio.h>
#include <string.h>
#include "atomicop.h"
#include "ltc2942.h"
#include "i2c/i2c_transaction.h"
#include "i2c/megalol_i2c.h"
#include "wait.h"
#include "usrmain.h"
#include "global.h"

/*
	Convert last into mAh using prescaler
	Compute average power (using voltage)
	Reset counter to midrange if reaches top/bottom
*/


/*
	File: ltc2942
	
	LTC2942 functions
	
	This library interfaces with the LTC2942 gas gauge which allows to measure charge flowing through the battery.
	
	*Dependencies*
	
	* I2C library
	
	*Key functions*
	
	This library provides immediate functions which immediately carry out an action, 
	and functions which initiate background reads and return data read after these background reads.
	
	In order to use background functions, ltc2942_backgroundgetstate must be called at regular intervals, typically
	from a timer interrupt. 
		
	Prior to using any functions ltc2942_init must be called.
	
	The key immediate functions are:
	
	* ltc2942_init:					Initialise the LTC2942 to default settings.
	* ltc2942_getcharge:			Returns the charge counter in uAh.
	* ltc2942_getchargectr:			Returns the 16-bit accumulated charge counter.
	* ltc2942_setchargectr:			Sets the 16-bit accumulated charge counter.
	* ltc2942_getvoltage:			Returns the voltage on the battery lead in millivolts.
	* ltc2942_gettemperature:		Returns the LTC2942 temperature in decidegrees (1 unit=0.1°celsius).
	* ltc2942_setadc:				Setups the LTC2942 conversion of voltage and temperature and the charge prescaler. 	
	* ltc2942_setprescaler:			Setups the LTC2942 charge prescaler. 
	* ltc2942_printreg:				Prints the LTC2942 register states to a file.
	* ltc2942_getavgpower: 			Computes the average power consumption in mW

	The key background functions are:
	
	* ltc2942_backgroundgetstate:	Initiates a background read of the LTC2942 charge, voltage and temperature registers.
									This function can be called from a timer interrupt to perform transparent background read of the battery status
	* ltc2942_last_charge:			Returns the charge counter in uAh.
	* ltc2942_last_chargectr:		Returns the 16-bit accumulated charge counter.
	* ltc2942_last_mV:				Returns the voltage on the battery lead in millivolts.
	* ltc2942_last_mA:				Returns the average current between two background reads in mA.
	* ltc2942_last_mW:				Returns the average power between two background reads in mW.
	* ltc2942_last_temperature:		Returns the LTC2942 temperature in decidegrees (1 unit=0.1°celsius).
	* ltc2942_last_strstatus		Returns a status string describing the battery mV, mA and mW.
	
	*Usage in interrupts*
	
	None of the immediate functions are suitable to be called from interrupts and will result in a crash, with exception of ltc2942_getavgpower. 
		
	The function ltc2942_backgroundgetstate can be called from an interrupt (typically a timer interrupt) to initiate a background
	read of the data. All the background functions can be called from interrupts.
	
	*Recommended usage*
	
	Register ltc2942_backgroundgetstate in a periodic timer interrupt (e.g. every 10 seconds) and use the functions ltc2942_last_* in the 
	main application.
*/



unsigned char __ltc2942_prescaler=0;						// This variable mirrors the prescaler P set with ltc2942_setprescaler
unsigned char __ltc2942_prescalerM=1;						// This variable mirrors the prescaler M set with ltc2942_setprescaler (M=2^P)
I2C_TRANSACTION __ltc2942_trans_selreg;						// I2C transaction to select register for background read
I2C_TRANSACTION __ltc2942_trans_read;						// I2C transaction to read registers for background read
I2C_TRANSACTION __ltc2942_trans_setctr;						// I2C transaction to set counter to midrange for background read
volatile unsigned long int _ltc2942_last_updatetime=0;		// Background read: time of read
volatile unsigned long _ltc2942_last_charge=0;				// Background read: charge (uAh)
volatile unsigned short _ltc2942_last_chargectr=0;			// Background read: charge counter (raw)
volatile unsigned short _ltc2942_last_mV=0;					// Background read: voltage 
volatile signed short _ltc2942_last_mW=0;					// Background read: average power in mW between two reads
volatile signed short _ltc2942_last_mA=0;					// Background read: average current in mA between two reads
volatile short _ltc2942_last_temperature;					// Background read: temperature
unsigned char _ltc2942_previousreadexists=0;				// Flag used to indicate whether a previous background read was performed; used to compute mA and mW when two reads are available.
char _ltc2924_batterytext[42];								// Holds a text description of the battery status.
volatile signed short _ltc2942_last_mWs[LTC2942NUMLASTMW];		// Holds the last LTC2942NUMLASTMW mW

LTC2942_BATSTAT _ltc2942_batstat[LTC2942NUMLONGBATSTAT];	// Circular buffer holding battery status on long time scales (typically updated every ~150 seconds or more)
volatile unsigned char _ltc2942_batstat_rdptr=0,_ltc2942_batstat_wrptr=0;	// Read/write pointers. This is used for fast removal of old entries avoiding memory copy
unsigned long _ltc2942_batstat_lastupdate=0;
const unsigned long _ltc2942_batstat_updateevery=LTC2942NUMLONGBATSTAT_UPDATEEVERY;	// Store every 3mn. 
volatile unsigned _ltc2942_backgroundgetstate_ongoing=0;	// Mutex to ensure only one background read at any time


/******************************************************************************
	function: ltc2942_init
*******************************************************************************	
	Initialise the LTC2942 to default settings: automatic conversion of voltage
	and temperature, highest charge resolution, charge counter set at mid-point 
	suitable to measure battery charge or discharge.
	
	With these settings the maximum charge/discharge that can be measured is 21mAh.
	
	Note: 
	no error checking on the I2C interface.
	
*******************************************************************************/
void ltc2942_init(void)
{
	ltc2942_setadc(0b11);			// Enable autoconversion of voltage and temperature
	ltc2942_setprescaler(0);		// Highest resolution P=0, M=1
	//ltc2942_setprescaler(3);		// P=3, M=8
	ltc2942_setchargectr(32768);	// Set charge counter at mid-point
	// Initialise the I2C transactions for background reads
	// Register selection transaction from register 2, which is accumulated charge MSB
	i2c_transaction_setup(&__ltc2942_trans_selreg,LTC2942_ADDRESS,I2C_WRITE,0,1);
	__ltc2942_trans_selreg.data[0]=0x02;			
	// Register read transaction for 12 registers
	i2c_transaction_setup(&__ltc2942_trans_read,LTC2942_ADDRESS,I2C_READ,1,12);	
	__ltc2942_trans_read.callback=__ltc2942_trans_read_done;
	// Transaction to set counter to midrange
	i2c_transaction_setup(&__ltc2942_trans_setctr,LTC2942_ADDRESS,I2C_WRITE,1,3);
	__ltc2942_trans_setctr.data[0] = 0x02; 	// Charge register
	__ltc2942_trans_setctr.data[1] = 0x80;
	__ltc2942_trans_setctr.data[2] = 0x00;
	_ltc2942_previousreadexists=0;
	for(unsigned char i=0;i<LTC2942NUMLASTMW;i++)
		_ltc2942_last_mWs[i]=0;
		
	ltc2942_clear_longbatstat();
	
	_ltc2942_backgroundgetstate_ongoing=0;
}
/******************************************************************************
	function: ltc2942_isok
*******************************************************************************
	Check if LTC responds.

	Do not call from interrupts.

	Note:
	no error checking on the I2C interface.

*******************************************************************************/
unsigned char ltc2942_isok(void)
{
	unsigned char control;
	unsigned char rv = i2c_readreg(LTC2942_ADDRESS,0x01,&control);
	if(rv)
		return 0;
	return 1;
}

/******************************************************************************
	function: ltc2942_getchargectr
*******************************************************************************	
	Returns the 16-bit accumulated charge counter.
	
	Note: 
	no error checking on the I2C interface.
	
	Returns:
		Charge	-	16-bit accumulated charge 
*******************************************************************************/
unsigned short ltc2942_getchargectr(void)
{
	unsigned char v[2];
	unsigned short charge;
	
	i2c_readregs(LTC2942_ADDRESS,0x02,v,2);
	//printf("charge: %02x %02x\n",v[0],v[1]);
	charge = v[0];
	charge<<=8;
	charge|=v[1];
	return charge;
}

/******************************************************************************
	function: ltc2942_getcharge
*******************************************************************************	
	Returns the charge counter in uAh.
	This function takes into account the prescaler of the LTC2942 and converts
	the charge counter into uAh.
	
	The maximum value that this function returns is 5'570'475.
	

	
	Note: 
	no error checking on the I2C interface.
	
	Returns:
		Charge	-	Charge in uAh
*******************************************************************************/
unsigned long ltc2942_getcharge(void)
{
	unsigned short charge;	// charge counter
	
	charge = ltc2942_getchargectr();
	return ltc2942_chargectr_to_charge(charge,__ltc2942_prescalerM);

}
/******************************************************************************
	function: ltc2942_chargectr_to_charge
*******************************************************************************	
	Converts a charge count into a charge in uAh.
	
	The maximum value that this function returns is 5'570'475.
	
	Parameters:
		chargectr		-	Charge counter
		prescaler		-	Prescaler M (M=2^P)
	Returns:
		Charge	-	Charge in uAh
*******************************************************************************/
unsigned long ltc2942_chargectr_to_charge(unsigned long chargectr,unsigned char prescalerm)
{
	// Conversion formula:
	// qLSB=0.085mAh*M/128 with M the prescaler	
	unsigned long charge = (85l*prescalerm*(unsigned long)chargectr)/128;
	return charge;
}

/******************************************************************************
	function: ltc2942_setchargectr
*******************************************************************************	
	Sets the 16-bit accumulated charge counter.
	
	Returns:
		0			-	Success
		nonzero		-	Error
*******************************************************************************/
unsigned char ltc2942_setchargectr(unsigned short charge)
{
	unsigned char rv;
	unsigned char v[2];
	
	v[0] = charge>>8;
	v[1] = (unsigned char) charge;
	
	rv = i2c_writeregs(LTC2942_ADDRESS,0x02,v,2);
		
	return rv;	
}

/******************************************************************************
	function: ltc2942_getvoltage
*******************************************************************************	
	Returns the voltage on the battery lead in millivolts.
	
	Note: 
	no error checking on the I2C interface.
	
	Returns:
		Voltage 	-	Voltage in millivolts
*******************************************************************************/
unsigned short ltc2942_getvoltage(void)
{
	unsigned char v[2];
	unsigned long voltage;
	i2c_readregs(LTC2942_ADDRESS,0x08,v,2);
	//printf("voltage: %02x %02x\n",v[0],v[1]);
	voltage = v[0];
	voltage<<=8;
	voltage|=v[1];
	voltage*=6000;
	//voltage>>=16;			// WARNING: should divide by 65535, but approximate by 65536
	voltage/=65535;
		
	return voltage;
}
/******************************************************************************
	function: ltc2942_gettemperature
*******************************************************************************	
	Returns the LTC2942 temperature in decidegrees (1 unit=0.1°celsius).

	Note: 
	no error checking on the I2C interface.
	
	Returns:
		Temperature[dd] -	Temperature in decidegrees (1 unit=0.1°celsius)
*******************************************************************************/

short ltc2942_gettemperature(void)
{
	unsigned char v[2];
	long temperature;
	i2c_readregs(LTC2942_ADDRESS,0x0C,v,2);
	//printf("temperature: %02x %02x\n",v[0],v[1]);
	temperature = v[0];
	temperature<<=8;
	temperature|=v[1];
	temperature *= 600 * 10;		// *10 to get decimal kelvin
	temperature/=65535;
	temperature-=2721;			// to decimal degree
	return temperature;
}


/******************************************************************************
	function: ltc2942_setadc
*******************************************************************************	
	Setups the LTC2942 ADC conversion of voltage and temperature between single, automatic or disabled.
	
	Parameters:
		adc			-	2-bits indicating: 
						11:temperature&voltage conversion every second;
						10: single voltage conversion;
						01: single temperature conversion;
						00: no conversion (sleep).
					
	
	
	Returns:
		0			-	Success
		nonzero		-	Error
*******************************************************************************/
unsigned char ltc2942_setadc(unsigned char adc)
{
	unsigned char rv;
	unsigned char control;
	
	rv = i2c_readreg(LTC2942_ADDRESS,0x01,&control);
	if(rv)
		return rv;
	adc&=0b11;
	control&=0b00111000;	// Shutdown=0, AL#/CC disabled, preserve prescaler
	control|=adc<<6;
	rv = i2c_writereg(LTC2942_ADDRESS,0x01,control);
	return rv;
}

/******************************************************************************
	function: ltc2942_setprescaler
*******************************************************************************	
	Setups the LTC2942 charge prescaler. 
	
	Parameters:				
		prescaler	-	3-bits indicating the prescaler value M between 1 and 128. M = 2^prescaler.
		
	
	Returns:
		0			-	Success
		nonzero		-	Error
*******************************************************************************/
unsigned char ltc2942_setprescaler(unsigned char prescaler)
{
	unsigned char rv;
	unsigned char control;
	
	rv = i2c_readreg(LTC2942_ADDRESS,0x01,&control);
	if(rv)
		return rv;
	prescaler&=0b111;
	__ltc2942_prescaler=prescaler;
	__ltc2942_prescalerM=(1<<__ltc2942_prescaler);
	control&=0b11000000;	// Shutdown=0, AL#/CC disabled, preserve ADC
	control|=prescaler<<3;
	rv = i2c_writereg(LTC2942_ADDRESS,0x01,control);
	return rv;
}
/******************************************************************************
	function: ltc2942_getprescaler
*******************************************************************************	
	Returns the 3-bit prescaler P value.
	The actual prescaler value M is betweeen 1 and 128: M=2^prescaler.
	
	This function returns a cached version of the prescaler set with 
	ltc2942_setprescaler.
	
	
	Returns:
		prescaler	-	3-bit prescaler value
*******************************************************************************/
unsigned char ltc2942_getprescaler(void)
{
	return __ltc2942_prescaler;
}

/*unsigned char ltc2942_setcontrol(unsigned char control)
{
	unsigned char rv;
	rv = i2c_writereg(LTC2942_ADDRESS,0x01,control);
	return rv;
}*/




/******************************************************************************
	function: ltc2942_printreg
*******************************************************************************	
	Prints the LTC2942 register states to a file.
	
	Parameters:				
		file	-	File on which the registers must be printed
		
	
	Returns:
		void
*******************************************************************************/
void ltc2942_printreg(FILE *file)
{
	unsigned char rv,v;
	fprintf(file,"LTC2942:\n");
	for(unsigned int r=0;r<=0xf;r++)
	{
		rv = i2c_readreg(LTC2942_ADDRESS,r,&v);
		fprintf(file,"\t%02Xh: ",r);
		if(rv)
		{
			fprintf(file,"Error\n");
		}
		fprintf(file,"%02Xh\n",v);
	}
}

//unsigned short ltc2942_start


/******************************************************************************
	ltc2942_deltaQ
*******************************************************************************	
	Computes the charge delta between two readings of the accumulated charge
	counter, taking into account the prescaler.
	
	Parameters:				
		q1			-	First reading of the accumulated charge counter
		q2			-	Second reading of the accumulated charge counter
	
	Returns:
		dq			-	Delta charge in uAh
*******************************************************************************/
/*signed long ltc2942_deltaQ(unsigned short q1,unsigned short q2)
{
	signed long dq;

	dq = ((signed long)q2)-((signed long)q1);	// dq: uncalibrated charge difference
	dq = (85*(1<<__ltc2942_prescaler)*dq)/128;			// dq: charge difference in uAh
	return dq;
}*/



/******************************************************************************
	function: ltc2942_getavgpower
*******************************************************************************	
	Computes the average power consumption in mW between two readings of 
	the charge done after a time interval ms. 
	This function assumes that the battery voltage is approximately constant
	between the first and second charge measure. 
	
	Parameters:				
		c1			-	First charge reading in uAh (at a previous time)
		c2			-	Second charge reading in uAh (at current time)
		voltage		-	Voltage in millivolts
		ms			-	Time interval between first and second charge readings
	
	Returns:
		Average power in mW
*******************************************************************************/
signed short ltc2942_getavgpower(unsigned long c1,unsigned long c2,unsigned short voltage,unsigned long ms)
{
#warning Modify to use start and end voltage for higher accuracy on longer measurements
	signed long dq = c2-c1;	// delta uAh

	signed long den;
	signed long pwr;
	
	den = dq*voltage;						// delta energy nWh
	pwr = den*36/10/((signed long)ms);		// power in mW
	
	return (signed short)pwr;
}

/******************************************************************************
	function: ltc2942_getavgpower2
*******************************************************************************	
	Computes the average power consumption in uW between two readings of 
	the charge and voltage done after a time interval s.
	
	This function is designed for long-duration intervals and low power (uW),
	such as measuring current during off or long-duration sleeps. 
	
	This function assumes the voltage changes linearly between the two
	measurement points.
	
	Parameters:				
		c1			-	First charge reading in uAh (at a previous time)
		c2			-	Second charge reading in uAh (at current time)
		voltage1	-	First voltage reading in millivolts
		voltage2	-	Second voltage reading in millivolts
		s			-	Time interval between first and second charge readings
	
	Returns:
		Average power in uW
*******************************************************************************/
signed long ltc2942_getavgpower2(unsigned long c1,unsigned long c2,unsigned short voltage1,unsigned short voltage2,unsigned long s)
{
	unsigned short voltage=(voltage1+voltage2)/2;		// Average voltage, assumes linear discharge
	
	signed long dq = c2-c1;	// delta uAh

	signed long den;
	signed long pwr;
	
	den = dq*voltage;						// delta energy nWh
	pwr = den*36/10/((signed long)s);		// power in uW
	
	return pwr;
}

/******************************************************************************
	function: ltc2942_backgroundgetstate
*******************************************************************************	
	Call from user code or a timer interrupt to initiate a background read
	of the LTC2942 state registers (accumulated charge, voltage, temperature).
	
	The background read is completed when the callback __ltc2942_trans_read_done
	is called by the I2C engine.
	
	The results are available through the ltc2942_last_xxx functions.
	
	Parameters:
		None / not used
	Returns:
		Nothing.
	
******************************************************************************/
unsigned char ltc2942_backgroundgetstate(unsigned char unused)
{
	(void)unused;
	//fprintf(file_pri,"ltc\n");
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(_ltc2942_backgroundgetstate_ongoing)
		{
			//fprintf(file_pri,"ltc f1\n");
			return 1;
		}
		_ltc2942_backgroundgetstate_ongoing=1;
	}
	unsigned char r = i2c_transaction_queue(2,0,&__ltc2942_trans_selreg,&__ltc2942_trans_read);
	if(r)
	{
		//fprintf(file_pri,"ltc f 2\n");
		// Failed to queue the transactions
		_ltc2942_backgroundgetstate_ongoing=0;
		return 1;
	}	
	return 0;
}
/******************************************************************************
	function: ltc2942_last_updatetime
*******************************************************************************	
	Returns the time in ms when the last background read occurred.
	
	Parameters:
		None 
	Returns:
		Time in ms when a background read last occurred.
	
******************************************************************************/
unsigned long ltc2942_last_updatetime(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_updatetime;
	}
	return 0;
}
/******************************************************************************
	function: ltc2942_last_charge
*******************************************************************************	
	Returns the charge counter in uAh obtained during the last background read.
	
	This function takes into account the prescaler of the LTC2942 and converts
	the charge counter into uAh.
	
	The maximum value that this function returns is 5'570'475.

	Returns:
		Charge	-	Charge in uAh
*******************************************************************************/
unsigned long ltc2942_last_charge(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_charge;
	}
	return 0;
}
/******************************************************************************
	function: ltc2942_last_chargectr
*******************************************************************************	
	Returns the 16-bit accumulated charge counter obtained during the last 
	background read.

	Returns:
		Charge	-	16-bit accumulated charge 
*******************************************************************************/
unsigned short ltc2942_last_chargectr(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_chargectr;
	}
	return 0;
}
/******************************************************************************
	function: ltc2942_last_mV
*******************************************************************************	
	Returns the voltage on the battery lead in millivolts obtained during the last 
	background read.
	
	Returns:
		Voltage 	-	Voltage in millivolts
*******************************************************************************/
unsigned short ltc2942_last_mV(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_mV;
	}
	return 0;
}
/******************************************************************************
	function: ltc2942_last_temperature
*******************************************************************************	
	Returns the LTC2942 temperature in decidegrees (1 unit=0.1°celsius) 
	obtained during the last  background read.

	Returns:
		Temperature[dd] -	Temperature in decidegrees (1 unit=0.1°celsius)
*******************************************************************************/
short ltc2942_last_temperature(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_temperature;
	}
	return 0;
}
/******************************************************************************
	function: ltc2942_last_mW
*******************************************************************************	
	Returns the power used between two subsequent background reads in mW.

	Returns:
		Power	-	Power in mW
*******************************************************************************/
signed short ltc2942_last_mW(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_mW;
	}
	return 0;
}
/******************************************************************************
	function: ltc2942_last_mA
*******************************************************************************	
	Returns the power used between two subsequent background reads in mW.

	Returns:
		Power	-	Power in mW
*******************************************************************************/
signed short ltc2942_last_mA(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _ltc2942_last_mA;
	}
	return 0;
}

/******************************************************************************
	__ltc2942_trans_read_done
*******************************************************************************	
	Callback called by the I2C engine when an interrupt-read completes.
	Do not call from user code.
	
	This function has not been benchmarked.
	
	
	Parameters:
		t		-	Current transaction (which will be __ltc2942_trans_read
	Returns:
		Battery information is stored in global variables.
	
******************************************************************************/
unsigned char __ltc2942_trans_read_done(I2C_TRANSACTION *t)
{
	(void)t;

	//fprintf(file_pri,"read done\n");

	// Store when the data was read
	unsigned long lasttime = _ltc2942_last_updatetime;
	_ltc2942_last_updatetime = timer_ms_get();
	
	// Mirror the data in user accessible variables
	// Voltage: requires a 32-bit temp
	unsigned long voltage=__ltc2942_trans_read.data[6];
	voltage<<=8;
	voltage|=__ltc2942_trans_read.data[7];
	voltage*=6000;
	voltage>>=16;			// WARNING: should divide by 65535, but approximate by 65536
	//voltage/=65535;	
	_ltc2942_last_mV=voltage;
	
	// Charge counter
	_ltc2942_last_chargectr = __ltc2942_trans_read.data[0];
	_ltc2942_last_chargectr<<=8;
	_ltc2942_last_chargectr|=__ltc2942_trans_read.data[1];
	
	// Charge counter to uAh
	unsigned long lastcharge=_ltc2942_last_charge;
	_ltc2942_last_charge = ltc2942_chargectr_to_charge(_ltc2942_last_chargectr,__ltc2942_prescalerM);		// _ltc2942_last_charge: charge in uAh
	
	
	// Temperature: requires a 32-bit temp
	unsigned long temperature=__ltc2942_trans_read.data[10];
	temperature<<=8;
	temperature|=__ltc2942_trans_read.data[11];	
	temperature *= 600 * 10;		// *10 to get decimal kelvin
	temperature>>=16;				// WARNING: should divide by 65535, but approximate by 65536
	//temperature/=65535;	
	temperature-=2721;				// to decimal degree
	_ltc2942_last_temperature = temperature;
	
	// Only convert to power/current if a previous measurement of charge exists, otherwise leave unchanged the current/power.
	if(_ltc2942_previousreadexists)		
	{
		// Convert into current and power since last read. Negative: discharge
		signed long deltams = _ltc2942_last_updatetime-lasttime;
		signed long deltauAh = _ltc2942_last_charge-lastcharge;
		signed long mA;
		mA = deltauAh * 3600l / deltams;		// mA	
		_ltc2942_last_mA = (signed short)mA;
		_ltc2942_last_mW = ltc2942_getavgpower(lastcharge,_ltc2942_last_charge,_ltc2942_last_mV,deltams);
		
		//
		for(unsigned char i=0;i<LTC2942NUMLASTMW-1;i++)
			_ltc2942_last_mWs[i]=_ltc2942_last_mWs[i+1];
		_ltc2942_last_mWs[LTC2942NUMLASTMW-1]=_ltc2942_last_mW;
	}

	_ltc2942_previousreadexists=1;	// Indicate we have made a previous measurement of charge
	
	// Check whether enough time elapsed to store in long battery status. BUG: does not handle wraparound.
	if(_ltc2942_batstat_lastupdate+_ltc2942_batstat_updateevery<=_ltc2942_last_updatetime)
	{
		LTC2942_BATSTAT batstat;
		batstat.t = _ltc2942_last_updatetime;
		batstat.mW=_ltc2942_last_mW;
		batstat.mA=_ltc2942_last_mA;
		batstat.mV=_ltc2942_last_mV;
		_ltc2942_add_longbatstat(&batstat);
		
		_ltc2942_batstat_lastupdate=_ltc2942_last_updatetime;	// Schedule next push
	}
	
	// Reset charge counter to midrange if getting close to top/bot
	if(_ltc2942_last_chargectr<8192 || _ltc2942_last_chargectr>57343)
	{
		unsigned char r = i2c_transaction_queue(1,0,&__ltc2942_trans_setctr);
		if(r)
		{
			// Failed to queue the transactions
		}	
		else
			_ltc2942_previousreadexists=0;			// Indicate there is no valid previous measurement of charge
	}
	
	
	_ltc2942_backgroundgetstate_ongoing=0;
	
	return 0;
}

/******************************************************************************
	function: ltc2942_last_strstatus
*******************************************************************************	
	Returns a status string describing the battery mV, mA and mW.
	
	Maximum string buffer length: "V=-65535 mV; I=-65535 mA; P=-65535 mW"
	or 38 bytes + 1 null. 42 bytes.

	Returns:
		Power	-	Power in mW
*******************************************************************************/
char *ltc2942_last_strstatus(void)
{
	sprintf(_ltc2924_batterytext,"V=%d mV; I=%d mA; P=%d mW",ltc2942_last_mV(),ltc2942_last_mA(),ltc2942_last_mW());
	return _ltc2924_batterytext;
}

/******************************************************************************
	function: ltc2942_last_mWs
*******************************************************************************	
	Returns the mW power estimated during the last battery read.
	
	This is updated each time the battery status is read and thus offers a short
	term view of battery usage.

	For a longer term view of battery usage, use ltc2942_get_longbatstat.
	
	Parameters:
		idx		-	Index of the last battery power; 0 means oldest, LTC2942NUMLASTMW-1 means most recent.
		
	Returns:
		Power	-	Power in mW
*******************************************************************************/
signed short ltc2942_last_mWs(unsigned char idx)
{
	if(idx<LTC2942NUMLASTMW)
	{
		ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
		{
			return _ltc2942_last_mWs[idx];
		}
	}
	return 0;
}


/******************************************************************************
Battery statistics notes


*******************************************************************************/

/******************************************************************************
	function: ltc2942_get_numlongbatstat
*******************************************************************************	
	Returns how many long battery statistics are available.
	
	The number is zero, when the system is initialised, up to 
	LTC2942NUMLONGBATSTAT-1.
	
	As the battery statistics are stored in an interrupt routine, there may be
	more battery statistics available that the value returned, but there 
	can never be less.
	

	Returns:
		Number of battery statistics available at the time of query
*******************************************************************************/
unsigned char ltc2942_get_numlongbatstat()
{
	signed short n;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		n = _ltc2942_batstat_wrptr-_ltc2942_batstat_rdptr;
	}
	// n can be negative if one of the pointer wraps around. In which case correct this
	if(n<0)
		n+=LTC2942NUMLONGBATSTAT;
	return n;	
}
/******************************************************************************
	function: ltc2942_get_longbatstat
*******************************************************************************	
	Returns the long battery statistics specified by idx.
	
	idx should be an index between 0 and ltc2942_get_numlongbatstat()-1. 
	If ltc2942_get_numlongbatstat() returns 0 then no battery statistics is 
	available and ltc2942_get_longbatstat should not be called.
	
	As the battery statistics are stored in an interrupt routine in a circular 
	buffer which overwrites the oldest entries, successive calls to 
	ltc2942_get_longbatstat with incrementing indices may skip one battery
	statistics between two successive calls.
	Example:
	ltc2942_get_longbatstat(0) should return the oldest battery statistic, say 
	at t=1000 seconds. ltc2942_get_longbatstat(1) should return the one after 
	that, say at 1=1100 seconds if battery statistics are updated every 100s.
	However if the long battery statistics are updated between the two calls
	and the circular buffer is full then the oldest entry is dropped and 
	ltc2942_get_longbatstat(1) will return the battery statistics at 
	t=1200 seconds.
	
	This is not deemed an issue as the battery statistics comprises a timestamp.
	
	Note that idx is not checked to point to a valid battery entry.

	Parameters
		idx				-	Index of the battery statistics to return
		batstat			-	Pointer to the _LTC2942_BATSTAT structure to receive the statistics
	Returns:
		-
*******************************************************************************/
void ltc2942_get_longbatstat(unsigned char idx,LTC2942_BATSTAT *batstat)
{
	signed short n;
	
	if(idx>LTC2942NUMLONGBATSTAT-2)		// Clamp to avoid reads to other memory locations; there is a maximum of LTC2942NUMLONGBATSTAT-1 entries, from 0 to LTC2942NUMLONGBATSTAT-2
		idx=LTC2942NUMLONGBATSTAT-2;
		
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)	// Get the actual index and copy the data
	{
		n=_ltc2942_batstat_rdptr+idx;
		if(n>=LTC2942NUMLONGBATSTAT)
			n-=LTC2942NUMLONGBATSTAT;
		*batstat=_ltc2942_batstat[n];
	}	
}

/******************************************************************************
	function: ltc2942_clear_longbatstat
*******************************************************************************	
	Clear the stored long battery statistics

	Returns:
		-
*******************************************************************************/
void ltc2942_clear_longbatstat()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_ltc2942_batstat_wrptr=_ltc2942_batstat_rdptr=0;
	}
	_ltc2942_batstat_lastupdate=0;
}
/******************************************************************************
	function: _ltc2942_add_longbatstat
*******************************************************************************	
	Adds an entry to the battery stat, erasing the oldest one if needed.
	
	This function is used internally in interrupts, hence no ATOMIC_BLOCK is not needed.

	Returns:
		-
*******************************************************************************/
void _ltc2942_add_longbatstat(LTC2942_BATSTAT *batstat)
{
	// Data can always be added at the write pointer
	_ltc2942_batstat[_ltc2942_batstat_wrptr]=*batstat;
	// Increment the write pointer and check for wraparounds
	_ltc2942_batstat_wrptr++;
	if(_ltc2942_batstat_wrptr>=LTC2942NUMLONGBATSTAT)
		_ltc2942_batstat_wrptr-=LTC2942NUMLONGBATSTAT;
	// Now check whether wrptr and rdptr equal - if yes the buffer is full and rdptr must be incremented
	if(_ltc2942_batstat_wrptr==_ltc2942_batstat_rdptr)
	{
		// Increment rdptr and wrap around
		_ltc2942_batstat_rdptr++;
		if(_ltc2942_batstat_rdptr>=LTC2942NUMLONGBATSTAT)
			_ltc2942_batstat_rdptr-=LTC2942NUMLONGBATSTAT;
	}	
}

// For debugging
void _ltc2942_dump_longbatstat()
{
	printf("dump longbatstat: rd: %d wr: %d\n",_ltc2942_batstat_rdptr,_ltc2942_batstat_wrptr);
	printf("\tnum: %d\n",ltc2942_get_numlongbatstat());
	for(unsigned char i=0;i<LTC2942NUMLONGBATSTAT;i++)
		printf("\t%d: %lu %d %d %d\n",i,_ltc2942_batstat[i].t,_ltc2942_batstat[i].mV,_ltc2942_batstat[i].mA,_ltc2942_batstat[i].mW);	
}
// For debugging
void ltc2942_print_longbatstat(FILE *f)
{
	LTC2942_BATSTAT b;
	//printf("print longbatstat: rd: %d wr: %d\n",_ltc2942_batstat_rdptr,_ltc2942_batstat_wrptr);
	fprintf(f,"Battery info:\n");
	fprintf(f,"\tT[ms]\t\tmV\tmA\tmW\n");
	for(unsigned char i=0;i<ltc2942_get_numlongbatstat();i++)
	{
		ltc2942_get_longbatstat(i,&b);
		fprintf(f,"\t%010lu\t%04d\t%-3d\t%-3d\n",b.t,b.mV,b.mA,b.mW);
	}
}



