/*
	Changes:
		- Remove the "busy" and rely on the underlying spi-usart0 busy
		- Either: give up on fixing the first byte and document it should be ignored, or fix the read/interrupt logic to not write the 1st byte.
		-- In this case, exchange n bytes means sending n bytes and receiving n-1
*/

#include <stdio.h>
#include <stmspi.h>
#include <string.h>

#include "main.h"
#include "serial.h"
#include "i2c.h"
//#include "mpu.h"
//#include "mpu_test.h"
#include "wait.h"
//#include "init.h"
//#include "uiconfig.h"
#include "helper.h"
//#include "spi-usart0.h"
#include "system.h"
#include "mpu-spi.h"

/*
	File: mpu-spi.c
	
	Low-level register access functions to the MPU registers over the SPI interface (using dspi).
	
	The key functions are:
	
	* mpu_writereg:	writes one register. Waits for peripheral available before start, waits for completion. Do not use in interrupts.
	* mpu_readreg: reads one register. Waits for peripheral available before start, waits for completion. Do not use in interrupts.
	* mpu_readreg16: reads a 16-bit register. Waits for peripheral available before start, waits for completion. Do not use in interrupt.
	* mpu_readregs: reads several registers. Waits for peripheral available before start, waits for completion. Do not use in interrupts.
	* mpu_readregs_int: reads several registers using interrupts (faster). Waits for peripheral available before start, waits for completion. Do not use in interrupts.
	* mpu_readregs_int_cb: reads several registers using interrupts. Returns if peripheral not available, calls a callback on transfer completion. Interrupt safe.
	
	All functions will wait for the peripheral to be available, except mpu_readregs_int_cb which will return immediately with an error if the peripheral is used. This
	allows calls to all functions to be mixed from main code or interrupts (within interrupts only mpu_readregs_int_cb is allowed). 
	
*/

#define ATOMIC_BLOCK(ATOMIC_RESTORESTATE)


#define _MPU_FASTREAD_LIM 33
unsigned char _mpu_tmp[_MPU_FASTREAD_LIM+1];
unsigned char *_mpu_tmp_reg = _mpu_tmp+1;
void (*_mpu_cb)(void);
volatile unsigned char _mpu_ongoing=0;				// _mpu_ongoing is used to block access to _mpu_tmp and other static/global variables which can only be used by a single transaction at a time.
unsigned char *_mpu_result;
unsigned short _mpu_n;



/******************************************************************************
	function: mpu_writereg
*******************************************************************************	
	Writes an MPU register.
	
	Do not call from an interrupt.
	
	Parameters:
		reg		-		Register to write data to
		v		-		Data to write in register		
******************************************************************************/
void mpu_writereg_wait_poll_block(unsigned char reg,unsigned char v)
{
	_mpu_waitavailandreserve();

	unsigned char buf[2];
	buf[0] = reg;
	buf[1] = v;
	spiusart0_rwn_wait_poll_block(buf,2);
	_mpu_ongoing=0;
}


/******************************************************************************
	function: mpu_readreg
*******************************************************************************	
	Reads an 8-bit MPU register.
	
	Do not call from an interrupt.
	
	Parameters:
		reg		-		Register to read data from 
******************************************************************************/
unsigned char mpu_readreg_wait_poll_block(unsigned char reg)
{
	_mpu_waitavailandreserve();

	unsigned char buf[2];
	buf[0] = 0x80|reg;
	spiusart0_rwn_wait_poll_block(buf,2);
	_mpu_ongoing=0;
	return buf[1];
}
/******************************************************************************
	function: mpu_readreg16
*******************************************************************************	
	Reads a 16-bit MPU register.
	
	Do not call from an interrupt.
	
	Parameters:
		reg		-		First register to read data from; the next 8 bits are read
						from register reg+1
******************************************************************************/
/*unsigned short mpu_readreg16(unsigned char reg)
{
	if(_mpu_waitvailandreserve())
		return;
	unsigned short v;
	unsigned char buf[3];
	buf[0] = 0x80|reg;
	spiusart0_rwn_wait_poll_block(buf,3);
	v=(buf[1]<<8)|buf[2];
	_mpu_ongoing=0;
	return v;
}*/

/******************************************************************************
	function: mpu_readregs
*******************************************************************************	
	Reads several MPU registers.
	
	Do not call from an interrupt.
	
	Parameters:
		d		-		Buffer receiving the register data
		reg		-		First register to read data from
		n		-		Number of registers to read		
	Returns:
		Bla
******************************************************************************/
void mpu_readregs_wait_poll_block(unsigned char *d,unsigned char reg,unsigned char n)
{
	for(unsigned char i=0;i<n;i++)
	{
		d[i] = mpu_readreg_wait_poll_block(reg+i);
	}
}
/******************************************************************************
	function: mpu_readregs_wait_poll_or_int_block
*******************************************************************************	
	Reads several MPU registers using interrupt transfer. 
	Waits until transfer completion. 
	
	This is faster than calling mpu_readregs for transfer size smaller or equal
	to _MPU_FASTREAD_LIM; for larger transfer the function calls mpu_readregs.
	
	Do not call from an interrupt.
	
	Parameters:
		d		-		Buffer receiving the register data
		reg		-		First register to read data from
		n		-		Number of registers to read		
******************************************************************************/
void mpu_readregs_wait_poll_or_int_block(unsigned char *d,unsigned char reg,unsigned char n)
{
	if(n<=_MPU_FASTREAD_LIM)
	{
		_mpu_waitavailandreserve();

		// Allocate enough memory for _MPU_FASTREAD_LIM+1 exchange
		unsigned char tmp[_MPU_FASTREAD_LIM+1],*tmp2;
		tmp[0]=0x80|reg;
		tmp2=tmp+1;
		
		spiusart0_rwn_wait_int_block(tmp,n+1);		// Waits for peripheral available, and blocks until transfer complete
		// Discard the first byte which is meaningless and copy to return buffer
		for(unsigned char i=0;i<n;i++)
			d[i]=tmp2[i];

		_mpu_ongoing=0;
	}
	else
	{
		mpu_readregs_wait_poll_block(d,reg,n);
	}
}
/******************************************************************************
	function: mpu_readregs_try_poll_block_raw
*******************************************************************************	
	Reads several MPU registers using polling. Waits until transfer completion. 
	
	Suitable for calls from interrupts.
	
	Note that d must be an n+1 buffer, and the first register read will be
	stored at d[1].
	
	Parameters:
		d		-		Buffer receiving the register data; this must be a n+1 bytes register
		reg		-		First register to read data from
		n		-		Number of registers to read		
	
	Returns:
		0		-		Success
		1		-		Error: transaction too large or peripheral busy
	
******************************************************************************/
unsigned char mpu_readregs_try_poll_block_raw(unsigned char *d,unsigned char reg,unsigned char n)
{
	if(n>=_MPU_FASTREAD_LIM)
		return 1;

	if(_mpu_tryavailandreserve())
		return 1;

	d[0]=0x80|reg;
	unsigned r = spiusart0_rwn_try_poll_block(d,n+1);
	_mpu_ongoing=0;
	return r;
}

/******************************************************************************
	function: _mpu_waitavailandreserve
*******************************************************************************
	Waits until no MPU transaction occurs (_mpu_ongoing=0) and
	immediately indicates a transaction is ongoing (_spiusart0_ongoing=1).
	Interrupts are managed to ensure no interrupt-initiated transaction can sneak
	in.
******************************************************************************/
void _mpu_waitavailandreserve(void)
{
	unsigned cnt = 0;
	// Wait for end of interrupt transfer if any transfer is ongoing
	mpu_waitavailandreserve_wait:
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(!_mpu_ongoing)
		{
			_mpu_ongoing=1;
			goto mpu_waitavailandreserve_start;
		}
	}
	// Ensures opportunities to manage interrupts
	// TOFIX: alternative to _delay_us needed
	//_delay_us(1);				// 1 byte at 1MHz is 1uS.
	cnt++;						// hack - ideally should not be optimised out
	goto mpu_waitavailandreserve_wait;

	// Start of transmission
	mpu_waitavailandreserve_start:
	return;
}
/******************************************************************************
	function: _mpu_tryavailandreserve
*******************************************************************************
	Checks availability of MPU peripheral and reserves it, otherwise returns error.

	Checks that no MPU transaction occurs (_spiusart0_ongoing=0) and
	immediately indicates a transaction is ongoing (_spiusart0_ongoing=1) and returns
	success. Otherwise returns error.

	Interrupts are managed to ensure no interrupt-initiated transaction can sneak
	in.

	Return:
		0:		OK, peripheral reserved
		1:		Error reserving peripheral
******************************************************************************/
unsigned char _mpu_tryavailandreserve(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if any transfer is ongoing: if yes, return error
		if(_mpu_ongoing)
		{
			return 1;
		}
		// No transfer ongoing: reserve and return success.
		_mpu_ongoing=1;
	}
	return 0;
}

/******************************************************************************
	function: mpu_readregs_try_int_cb
*******************************************************************************	
	Read several MPU registers using interrupt transfer. 
	The maximum transfer size must be smaller or equal to _MPU_FASTREAD_LIM.
	
	The function returns immediately if another mpu_readregs_int_cb transaction
	is ongoing. It calls a callback upon completion.
	
	This function is safe to be called from an interrupt.
	
	The maximum number of exchanged bytes is _MPU_FASTREAD_LIM.
	
	Parameters:
		d		-		Buffer receiving the register data.
						
						If d is null, then the received data will be held
						in the temporary buffer _mpu_tmp_reg.
						The data in _mpu_tmp_reg must be used within the callback
						as subsequent transactions will overwrite the results.
		reg		-		First register to read data from
		n		-		Number of registers to read		
		cb		-		User callback to call when the transaction is completed.
						The result of the transaction is available in the 
						unsigned char array _mpu_result, which is set to point to
						d.
	Returns:
		0		-		Success
		1		-		Error
******************************************************************************/
unsigned char mpu_readregs_try_int_cb(unsigned char *d,unsigned char reg,unsigned char n,void (*cb)(void))
{
	// Transaction too large
	if(n>=_MPU_FASTREAD_LIM)
		return 1;

	if(_mpu_tryavailandreserve())	// Attempt acquiring MPU
		return 1;
		
	_mpu_result = d;
	_mpu_n = n;	
	_mpu_cb = cb;
	_mpu_tmp[0]=0x80|reg;
	unsigned char r = spiusart0_rwn_try_int_cb(_mpu_tmp,n+1,__mpu_readregs_int_cb_cb);
	_mpu_ongoing=0;
	return r;
}
/******************************************************************************
	__mpu_readregs_int_cb_cb
*******************************************************************************	
	Callback called when a transfer initiated by mpu_readregs_int_cb is
	completed.
	
******************************************************************************/
void __mpu_readregs_int_cb_cb(void)
{
	//dbg_fputchar_nonblock('y',0);
	// Copy data to user buffer, if a buffer was provided
	if(_mpu_result)
	{
		for(unsigned char i=0;i<_mpu_n;i++)
			_mpu_result[i] = _mpu_tmp_reg[i];	// _mpu_tmp_reg is equal to _mpu_tmp+1
	}
	if(_mpu_cb)
		_mpu_cb();
	//dbg_fputchar_nonblock('Y',0);
	_mpu_ongoing=0;
}
/******************************************************************************
	function: mpu_readregs_try_int_cb_raw
*******************************************************************************	
	Read several MPU registers using interrupt transfer. 
	
	This is a "raw" variant of mpu_readregs_int_cb which is slightly faster.
	
	The result of the register read is in _mpu_tmp_reg and must be used within the callback
	as subsequent transactions will overwrite the results.
	
	The callback must set _mpu_ongoing=0, otherwise no further transactions are possible.	
	
	The maximum transfer size must be smaller or equal to _MPU_FASTREAD_LIM.
	
	The function returns immediately if another mpu_readregs_int_cb transaction
	is ongoing. It calls a callback upon completion.
	
	This function is safe to be called from an interrupt.
	
	The maximum number of exchanged bytes is _MPU_FASTREAD_LIM.
	
	Parameters:
		reg		-		First register to read data from
		n		-		Number of registers to read		
		cb		-		User callback to call when the transaction is completed.
						The result of the transaction is available in the 
						unsigned char array _mpu_result, which is set to point to
						d.
	Returns:
		0		-		Success
		1		-		Error
		
******************************************************************************/
unsigned char mpu_readregs_try_int_cb_raw(unsigned char reg,unsigned char n,void (*cb)(void))
{
	// Transaction too large
	if(n>=_MPU_FASTREAD_LIM)
		return 1;

	if(_mpu_tryavailandreserve())	// Attempt acquiring MPU
		return 1;
		
	_mpu_n = n;	
	_mpu_cb = cb;
	_mpu_tmp[0]=0x80|reg;
	unsigned char r = spiusart0_rwn_try_int_cb(_mpu_tmp,n+1,cb);
	_mpu_ongoing=0;
	return r;
}




