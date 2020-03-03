#include <stdio.h>
#include <string.h>

#include "global.h"
#include "mpu.h"
#include "wait.h"
#include "main.h"
#include "mpu_config.h"
#include "helper.h"
#include "mpu_geometry.h"
#include "stmspi.h"
#include "mpu-spi.h"
#include "mpu-reg.h"
#include "mpu_geometry.h"
#include "atomicop.h"
#include "wait.h"
#include "math.h"
#include "eeprom-m24.h"


/*
	File: mpu
	
	ICM20948 functions

	Operating principles:
		* All functions are using polling operations, waiting for the peripheral to be available and blocking until completion (except interrupt-reading routines)
		* Interrupt reading must only be activated after configuration is completed to avoid contending for the resource
		* All functions accessing registers in address banks other than 0 will restore the current address bank to 0. Address bank 0 can be assumed.

	
	The key functions are:
	
	* mpu_init:					initialise the mpu
	* mpu_config_motionmode:	selects the acquisition mode (which sensors are acquired and the sample rate) and whether "automatic read" into a memory buffer is performed.
	* mpu_get_a:				blocking read of acceleration when not in automatic read (can also be used in automatic read).
	* mpu_get_g:				blocking read of gyroscope when not in automatic read (can also be used in automatic read).
	* mpu_get_agt:				blocking read of acceleration, gyroscope and temperature when not in automatic read (can also be used in automatic read).
	* mpu_get_agmt:				blocking read of acceleration, acceleration, magnetic field and temperature when not in automatic read (can also be used in automatic read).
	
	
	*Automatic read*
	The recommended mode of operation is to use the "automatic read" in mpu_config_motionmode. In this mode, the MPU data is read
	at the indicated rate in background (interrupt routine) and made available in memory buffers.
	In automatic read mode, the following functions and variables are used to access the data in the buffer:
	
	* mpu_data_level:			Function indicating how many samples are in the buffer
	* mpu_data_getnext_raw:		Returns the next data in the buffer (when automatic read is active).
	* mpu_data_getnext:			Returns the next raw and geometry data (when automatic read is active).
	
	
	In non automatic read, the functions mpu_get_a, mpu_get_g, mpu_get_agt or mpu_get_agmt must be used to acquire the MPU data. These functions can also be called in automatic
	read, however this is suboptimal and increases CPU load as the data would be acquired in the interrupt routine and 	through this function call.
	
	
	*Statistics*
	The following counters are available to monitor the interrupt-driven automatic read. These counters are cleared upon calling mpu_config_motionmode with a new acquisition mode:
	
	* mpu_cnt_int:				counts how often the MPU isr has been triggered
	* mpu_cnt_sample_tot:		counts how many samples occurred, independently of whether successful or not (this is equal or lower than mpu_cnt_int due to possible software downsampling)
	* mpu_cnt_sample_succcess:	counts how many samples were acquired and successfully stored in the buffer; mpu_cnt_sample_succcess-mpu_cnt_sample_tot indicates the number of errors that occurred
	* mpu_cnt_sample_errbusy:	counts how many samples were skipped because the interface was busy (e.g. from an ongoing transfer)
	* mpu_cnt_sample_errfull: 	counts how many samples were skipped because the buffer was full 
	
	
	*Units*
	The variable mpu_gtorps can be used to convert the gyroscope readings to radians per second. This variable is updated when mpu_setgyroscale is called.
	Depending on the compiler it is a fixed-point (_Accum) or float variable. Convert the gyroscope reading to radians per second as follows: 
	
	_Accum gx_rps = mpu_data_gx[mpu_data_rdptr] * mpu_gtorps;
	or
	float gx_rps = mpu_data_gx[mpu_data_rdptr] * mpu_gtorps;
	
	Gyroscope sensitivity: 
		full scale 250:			131.072LSB/dps
		full scale 500:			65.536LSB/dps
		full scale 1000:		32.768LSB/dps
		full scale 2000:		16.384LSB/dps
	
	
	*Magnetometer*	
	
	Functions to handle the magnetometer - prefixed by mpu_mag_... - can only be used when the MPU is configured
	in a mode where the magnetic field sensor is turned on; otherwise the function time-out due to the magnetic field
	sensor being inaccessible via the internal I2C interface.
	
	*Interrupt Service Routine (ISR) Internal notes*
	
	The MPU can be configured with register 55d (MPU_R_INTERRUPTPIN):
		- Logic low or logic high
		- Open-drain or push-pull
		- Level held until cleared or generate a 50uS pulse
		- Interrupt cleared after any read operation or after reading the INT_STATUS register
		
	Here "logic high", "push pull" and "50uS" is used. 
	
	Todo: 
	
	
*/

#define _delay_ms HAL_Delay


// Various
const char _str_mpu[] = "MPU: ";

unsigned char __mpu_sample_softdivider_ctr=0,__mpu_sample_softdivider_divider=0;

// Data buffers
MPUMOTIONDATA mpu_data[MPU_MOTIONBUFFERSIZE];
volatile unsigned long __mpu_data_packetctr_current;
volatile unsigned char mpu_data_rdptr,mpu_data_wrptr;
volatile MPUMOTIONDATA _mpumotiondata_test;
unsigned long __mpu_acqtime;							// Updated by ISR when dataready interrupt
// Magnetometer Axis Sensitivity Adjustment
//unsigned char _mpu_mag_asa[3];
signed short _mpu_mag_calib_max[3];
signed short _mpu_mag_calib_min[3];
signed short _mpu_mag_bias[3];
signed short _mpu_mag_sens[3];
unsigned char _mpu_mag_correctionmode;

// Automatic read statistic counters
unsigned long mpu_cnt_int, mpu_cnt_sample_tot, mpu_cnt_sample_succcess, mpu_cnt_sample_errbusy, mpu_cnt_sample_errfull;

unsigned long mpu_cnt_spurious;

unsigned char __mpu_autoread=0;

unsigned char _mpu_current_motionmode=0;

unsigned char _mpu_kill=0;
unsigned short _mpu_samplerate;
float _mpu_beta;

// Motion ISR
void (*isr_motionint)(void) = 0;

// Conversion from Gyro readings to rad/sec
#ifdef __cplusplus
float mpu_gtorps=3.14159665/180.0/131.072;
#else
_Accum mpu_gtorps=3.14159665k/180.0k/131.0k;
#endif



// 
unsigned char sample_mode;

/******************************************************************************
*******************************************************************************
MPU ISR   MPU ISR   MPU ISR   MPU ISR   MPU ISR   MPU ISR   MPU ISR   MPU ISR   
*******************************************************************************
*******************************************************************************/
/*
Benchmarks at SPIclk=1.25MHz CPUclk=20MHz

	mpu_isr_partblocking():
		perfbench_withreadout: 451501 perf (9996 intclk ms)
		New performance: 451501
		Benchmarking mode 25: 562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz
		perfbench_withreadout: 411698 perf (10058 intclk ms)
		MPU acquisition statistics:
		 MPU interrupts: 5771
		 Samples: 5771
		 Samples success: 5771
		 Errors: MPU I/O busy=0 buffer=0
		 Buffer level: 2/64
		 Spurious ISR: 0
			Mode 25: 562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz: perf: 411698 (instead of 451501). CPU load 9 %

	mpu_isr_blocking:
		perfbench_withreadout: 451500 perf (9996 intclk ms)
		New performance: 451500
		Benchmarking mode 25: 562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz
		perfbench_withreadout: 384842 perf (10058 intclk ms)
		MPU acquisition statistics:
		 MPU interrupts: 5770
		 Samples: 5770
		 Samples success: 5770
		 Errors: MPU I/O busy=0 buffer=0
		 Buffer level: 2/64
		 Spurious ISR: 0
			Mode 25: 562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz: perf: 384842 (instead of 451500). CPU load 15 %

Benchmarks at SPIclk=2.5MHz CPUclk=20MHz
	mpu_isr_partblocking():
		perfbench_withreadout: 451500 perf (9996 intclk ms)
		New performance: 451500
		Benchmarking mode 25: 562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz
		perfbench_withreadout: 414249 perf (10021 intclk ms)
		MPU acquisition statistics:
		 MPU interrupts: 5749
		 Samples: 5749
		 Samples success: 5749
		 Errors: MPU I/O busy=0 buffer=0
		 Buffer level: 2/64
		 Spurious ISR: 0
			Mode 25: 562.5Hz Gyro (BW=197Hz) + Acc  (BW=246Hz) + Mag 100Hz: perf: 414249 (instead of 451500). CPU load 9 %

*/
void mpu_isr(void)
{
	//mpu_isr_blocking();
	mpu_isr_partblocking();
}

// Blocking SPI read within this interrupt
void mpu_isr_blocking(void)
{
	// ICM20948
	// Read the status of the 4 interrupt flags. This this a blocking read
	unsigned char s[5];
	unsigned char r=mpu_readregs_try_poll_block_raw(s,ICM_R_INT_STATUS,4);
	if(r)		// Failed to read - e.g. peripheral busy
	{
		mpu_cnt_spurious++;
		return;
	}

	// Print information about interrupts
	//fprintf(file_pri,"I %02X %02X %02X %02X\n",s[1],s[2],s[3],s[4]);

	if( (s[2]&1) == 0)		// Check INT_STATUS_1 bit 0 (RAW_DATA_
	{
		mpu_cnt_spurious++;
		return;
	}
	

	// Statistics
	mpu_cnt_int++;

	// Software sample rate divider. Not used with ICM
	__mpu_sample_softdivider_ctr++;
	if(__mpu_sample_softdivider_ctr>__mpu_sample_softdivider_divider)
	{
		__mpu_sample_softdivider_ctr=0;

		// Statistics
		mpu_cnt_sample_tot++;
		
			
		// Automatically read data
		if(__mpu_autoread)
		{
			__mpu_data_packetctr_current=mpu_cnt_sample_tot;
			// Initiate readout: 3xA + 3*G + 1*T + 3*M+Mtmp+Mstatus = 22 bytes

			__mpu_acqtime = timer_ms_get();

			// Registers start at ICM_R_ACCEL_XOUT_H.
			unsigned char spibuf[32];
			unsigned char r = mpu_readregs_try_poll_block_raw(spibuf,ICM_R_ACCEL_XOUT_H,22);
			if(r)
			{
				mpu_cnt_sample_errbusy++;
				return;
			}
			__mpu_addnewdata(spibuf,__mpu_acqtime);
		}	// autoread
	} // __mpu_sample_softdivider_ctr
}
// Partially blocking SPI read within this interrupt: blocks to get status (5 bytes transfer), callback to get data (22 bytes transfer)
void mpu_isr_partblocking(void)
{
	// ICM20948
	// Read the status of the 4 interrupt flags. This this a blocking read
	unsigned char s[5];
	unsigned char r=mpu_readregs_try_poll_block_raw(s,ICM_R_INT_STATUS,4);
	if(r)		// Failed to read - e.g. peripheral busy
	{
		mpu_cnt_spurious++;
		return;
	}

	// Print information about interrupts
	//fprintf(file_pri,"I %02X %02X %02X %02X\n",s[1],s[2],s[3],s[4]);

	if( (s[2]&1) == 0)		// Check INT_STATUS_1 bit 0 (RAW_DATA_
	{
		mpu_cnt_spurious++;
		return;
	}


	// Statistics
	mpu_cnt_int++;

	// Software sample rate divider. Not used with ICM
	__mpu_sample_softdivider_ctr++;
	if(__mpu_sample_softdivider_ctr>__mpu_sample_softdivider_divider)
	{
		__mpu_sample_softdivider_ctr=0;

		// Statistics
		mpu_cnt_sample_tot++;


		// Automatically read data
		if(__mpu_autoread)
		{
			__mpu_data_packetctr_current=mpu_cnt_sample_tot;
			// Initiate readout: 3xA + 3*G + 1*T + 3*M+Mtmp+Mstatus = 22 bytes

			__mpu_acqtime = timer_ms_get();

			// Registers start at ICM_R_ACCEL_XOUT_H.
			unsigned char r = mpu_readregs_try_int_cb_raw(ICM_R_ACCEL_XOUT_H,22,__mpu_read_cb);
			if(r)
			{
				mpu_cnt_sample_errbusy++;
			}
			// Function returns - follow-up processing in __mpu_read_cb
		} 	// Autoread
	}	// Software divider
}
/******************************************************************************
	__mpu_addnewdata
*******************************************************************************
	Add data from raw buffer spibuf to MOTIONDATA.
	The first data read from the MPU is in spibuf[1]. spibuf[0] is the register read command.
*******************************************************************************/
void __mpu_addnewdata(unsigned char *spibuf,unsigned long acqtime)
{
	// Discard oldest data and store new one
	if(mpu_data_isfull())
	{
		_mpu_data_rdnext();
		mpu_cnt_sample_errfull++;
		mpu_cnt_sample_succcess--;		// This plays with the increment of mpu_cnt_sample_succcess on the last line of this function; i.e. mpu_cnt_sample_succcess does not change.
	}

	// Pointer to memory structure
	MPUMOTIONDATA *mdata = &mpu_data[mpu_data_wrptr];

	mdata->time=acqtime;

	//__mpu_copy_spibuf_to_mpumotiondata_asm(spibuf+1,mdata);			// Copy and conver the spi buffer to MPUMOTIONDATA; if this function is used, the correction must be manually done as below.
	//__mpu_copy_spibuf_to_mpumotiondata_magcor_asm(spibuf+1,mdata);		// Copy and conver the spi buffer to MPUMOTIONDATA including changing the magnetic coordinate system (mx <= -my; my<= -mx) (Dan's version)
	// TOFIX: ARM __mpu_copy_spibuf_to_mpumotiondata_magcor_asm_mathias
	__mpu_copy_spibuf_to_mpumotiondata_magcor_c_mathias(spibuf+1,mdata);
	//__mpu_copy_spibuf_to_mpumotiondata_magcor_asm_mathias(spibuf+1,mdata);		// Copy and conver the spi buffer to MPUMOTIONDATA including changing the magnetic coordinate system (mx <= my; my<= mx; mz<=-mz) (Mathias's version)


	// TOFIX: __mpu_copy_spibuf_to_mpumotiondata_magcor_asm_mathias in C - what does this do??


	// Alternative to __mpu_copy_spibuf_to_mpumotiondata_magcor_asm: manual change
	/*signed t = mdata->mx;
	mdata->mx=-mdata->my;
	mdata->my=-t;*/


	// Mathias - changes where is the yaw=0 by 180 (not needed if __mpu_copy_spibuf_to_mpumotiondata_magcor_asm_mathias is used)
	/*signed t = mdata->mx;
	mdata->mx=mdata->my;
	mdata->my=t;
	mdata->mz=-mdata->mz;*/

	//mdata->mz=0;

	/*mdata->gx=0;
	mdata->gy=0;
	mdata->gz=0;*/

	//mdata->mx=0;
	//mdata->my=0;
	//mdata->mz=0;








	mdata->packetctr=__mpu_data_packetctr_current;

	// correct the magnetometer
	//if(_mpu_mag_correctionmode==1)
		//mpu_mag_correct1(mdata->mx,mdata->my,mdata->mz,&mdata->mx,&mdata->my,&mdata->mz);		// This call to be used with __mpu_copy_spibuf_to_mpumotiondata_asm
		//mpu_mag_correct1(mdata->my,mdata->mx,mdata->mz,&mdata->my,&mdata->mx,&mdata->mz);		// This call to be used with __mpu_copy_spibuf_to_mpumotiondata_magcor_asm: swap mx and my to ensure the right ASA coefficients are applied. This was the last active code. Not used with ICM
	if(_mpu_mag_correctionmode==2)
		mpu_mag_correct2_inplace(&mdata->mx,&mdata->my,&mdata->mz);								// Call identical regardless of __mpu_copy_spibuf_to_mpumotiondata_asm or __mpu_copy_spibuf_to_mpumotiondata_magcor_asm as calibration routine uses corrected coordinate system.


	// Implement the channel kill
	if(_mpu_kill&1)
	{
		mdata->mx=mdata->my=mdata->mz=0;
	}
	if(_mpu_kill&2)
	{
		mdata->gx=mdata->gy=mdata->gz=0;
	}
	if(_mpu_kill&4)
	{
		mdata->ax=mdata->ay=mdata->az=0;
	}

	// Magnetic filter

	/*mdata->mx = (mdata->mx+7*mxo)/8;
	mdata->my = (mdata->my+7*myo)/8;
	mdata->mz = (mdata->mz+7*mzo)/8;
	mxo = mdata->mx;
	myo = mdata->my;
	mzo = mdata->mz;*/




	// Next buffer
	_mpu_data_wrnext();

	// Statistics
	mpu_cnt_sample_succcess++;



}


/******************************************************************************
	__mpu_read_cb
*******************************************************************************
	Callback called when the interrupt-driven MPU data acquisition completes.
	Copies the data in the temporary _mpu_tmp_reg into the mpu_data_xx buffers.
*******************************************************************************/
void __mpu_read_cb(void)
{
	__mpu_addnewdata(_mpu_tmp,__mpu_acqtime);		// __mpu_addnewdata excepts the first byte to be useless.
	_mpu_ongoing=0;									// Required when using mpu_readregs_int_cb_raw otherwise no further transactions possible
}




void __mpu_copy_spibuf_to_mpumotiondata_magcor_c_mathias(unsigned char *spibuf,MPUMOTIONDATA *mpumotiondata)
{
	// ICM20948 codepath
	// Acceleration (big endian)
	mpumotiondata->ax=(((short)spibuf[0])<<8)|spibuf[1];
	mpumotiondata->ay=(((short)spibuf[2])<<8)|spibuf[3];
	mpumotiondata->az=(((short)spibuf[4])<<8)|spibuf[5];
	// Gyro (big endian)
	mpumotiondata->gx=(((short)spibuf[6])<<8)|spibuf[7];
	mpumotiondata->gy=(((short)spibuf[8])<<8)|spibuf[9];
	mpumotiondata->gz=(((short)spibuf[10])<<8)|spibuf[11];
	// Temp (big endian)
	mpumotiondata->temp=(((short)spibuf[12])<<8)|spibuf[13];
	// Magnetic (little endian)
	mpumotiondata->my=(((short)spibuf[15])<<8)|spibuf[14];
	mpumotiondata->mx=(((short)spibuf[17])<<8)|spibuf[16];
	mpumotiondata->mz=-( (((short)spibuf[19])<<8)|spibuf[18] );
	mpumotiondata->ms=(spibuf[21]>>3)&1;

}


/******************************************************************************
	function: mpu_clearstat
*******************************************************************************	
	Clear the MPU acquisition statistics.
	
	Returns:
		-
*******************************************************************************/
void mpu_clearstat(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		mpu_cnt_int=0;
		mpu_cnt_sample_tot=0;
		mpu_cnt_sample_succcess=0;
		mpu_cnt_sample_errbusy=0;
		mpu_cnt_sample_errfull=0;
		mpu_cnt_spurious=0;
	}
}
/******************************************************************************
	function: mpu_getstat
*******************************************************************************	
	Get the MPU acquisition statistics.

	All parameters are pointer to a variable holding the return values. 
	If the pointer is null, the corresponding parameter is not returned.
	
	Parameters:
		cnt_int					-	Number of MPU ISR calls
		cnt_sample_tot			-	Number of MPU samples so far, regardless of successfull or not
		cnt_sample_succcess		-	Number of successful MPU samples acquired
		cnt_sample_errbusy		-	Number of unsuccessful MPU sample acquisition due to the MPU interface being busy
		cnt_sample_errfull		-	Number of unsuccessful MPU samples acquisition due to the MPU sample buffer being full
	
	Returns:
		-
*******************************************************************************/
void mpu_getstat(unsigned long *cnt_int, unsigned long *cnt_sample_tot, unsigned long *cnt_sample_succcess, unsigned long *cnt_sample_errbusy, unsigned long *cnt_sample_errfull)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if(cnt_int)
			*cnt_int=mpu_cnt_int;
		if(cnt_sample_tot)
			*cnt_sample_tot=mpu_cnt_sample_tot;
		if(cnt_sample_succcess)
			*cnt_sample_succcess=mpu_cnt_sample_succcess;
		if(cnt_sample_errbusy)
			*cnt_sample_errbusy=mpu_cnt_sample_errbusy;
		if(cnt_sample_errfull)
			*cnt_sample_errfull=mpu_cnt_sample_errfull;
	}
}
/******************************************************************************
	function: mpu_stat_totframes
*******************************************************************************
	Total number of motion frames that should have been acquired

	Returns:
		Number of motion frames
*******************************************************************************/
unsigned long mpu_stat_totframes()
{
	return mpu_cnt_sample_tot;
}
/******************************************************************************
	function: mpu_stat_lostframes
*******************************************************************************
	Number of frames which were lost, either due to buffer full, or due to
	interface busy.

	Returns:
		-
*******************************************************************************/
unsigned long mpu_stat_lostframes()
{
	return mpu_cnt_sample_errbusy+mpu_cnt_sample_errfull;
}

/******************************************************************************
	function: mpu_clearbuffer
*******************************************************************************	
	Clears all sensor data held in the MPU buffers.
	
	Parameters:
		-
	
	Returns:
		-
*******************************************************************************/
void mpu_data_clear(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		mpu_data_rdptr=mpu_data_wrptr=0;
		mpu_clearstat();
	}	
}

void _mpu_enableautoread(void)
{
	_mpu_disableautoread();		// Temporarily disable the interrupts to allow clearing the old statistics+buffer
	_delay_ms(1);				// Wait that the last potential interrupt transfer completes
	// Clear the software divider counter
	__mpu_sample_softdivider_ctr=0;
	// Clear data buffers and statistics counter
	mpu_data_clear();
	// Enable automatic read
	__mpu_autoread=1;
	// Enable motion interrupts
	//mpu_set_interrutenable(0,0,0,1);			// MPU9250

	mpu_set_interrutenable_icm20948(0,0,0,1,0,0,0,0);		// ICM20948
}

void _mpu_disableautoread(void)
{
	// Disable motion interrupts
	mpu_set_interrutenable_icm20948(0,0,0,0,0,0,0,0);
	// Disable automatic read
	__mpu_autoread=0;
}



/******************************************************************************
	function: mpu_data_isfull
*******************************************************************************	
	Returns 1 if the buffer is full, 0 otherwise.
*******************************************************************************/
unsigned char mpu_data_isfull(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( ((mpu_data_wrptr+1)&(MPU_MOTIONBUFFERSIZE-1)) == mpu_data_rdptr )
			return 1;
		return 0;
	}
	return 0;	// To prevent compiler from complaining
}
/*unsigned char mpu_data_isempty(void)
{
	if(mpu_data_rdptr==mpu_data_wrptr)
		return 1;
	return 0;
}*/
/******************************************************************************
	function: mpu_data_level
*******************************************************************************	
	Returns how many samples are in the buffer, when automatic read is active.
*******************************************************************************/
unsigned char mpu_data_level(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return (mpu_data_wrptr-mpu_data_rdptr)&(MPU_MOTIONBUFFERSIZE-1);
	}
	return 0;	// To avoid compiler warning
}
/******************************************************************************
	function: mpu_data_getnext_raw
*******************************************************************************	
	Returns the next data in the buffer, when automatic read is active and data
	is available.	
	This function returns raw reads, without applying the calibration to take into 
	account the accelerometer and gyroscope scale.
	
	This function removes the data from the automatic read buffer and the next call 
	to this function will return the next available data.
	
	If no data is available, the function returns an error.
	
	Returns:
		0	-	Success
		1	-	Error (no data available in the buffer)
*******************************************************************************/
//unsigned char mpu_data_getnext_raw(MPUMOTIONDATA &data)
unsigned char mpu_data_getnext_raw(MPUMOTIONDATA *data)
{
	//return (mpu_data_wrptr-mpu_data_rdptr)&(MPU_MOTIONBUFFERSIZE-1);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is empty
		if(mpu_data_wrptr==mpu_data_rdptr)
			return 1;
		// Copy the data
		//memcpy((void*)&data,(void*)&mpu_data[mpu_data_rdptr],sizeof(MPUMOTIONDATA));
		//data = *(MPUMOTIONDATA*)&mpu_data[mpu_data_rdptr];
		*data = mpu_data[mpu_data_rdptr];
		//data = mpu_data[mpu_data_rdptr];
		// Increment the read pointer
		_mpu_data_rdnext();
		return 0;
	}
	return 1;	// To avoid compiler warning
}
/******************************************************************************
	function: mpu_data_getnext
*******************************************************************************	
	Returns the next raw and geometry data, when automatic read is active and data
	is available.
	This function returns the raw reads and also computes the geometry (e.g. quaternions)
	from the raw data.
	
	This function removes the data from the automatic read buffer and the next call 
	to this function will return the next available data.
	
	If no data is available, the function returns an error.
	
	Returns:
		0	-	Success
		1	-	Error (no data available in the buffer)
*******************************************************************************/
unsigned char mpu_data_getnext(MPUMOTIONDATA *data,MPUMOTIONGEOMETRY *geometry)
{
	//return (mpu_data_wrptr-mpu_data_rdptr)&(MPU_MOTIONBUFFERSIZE-1);
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is empty
		if(mpu_data_wrptr==mpu_data_rdptr)
			return 1;
		// Copy the data
		//memcpy((void*)&data,(void*)&mpu_data[mpu_data_rdptr],sizeof(MPUMOTIONDATA));
		*data = mpu_data[mpu_data_rdptr];
		//data = mpu_data[mpu_data_rdptr];
		// Increment the read pointer
		_mpu_data_rdnext();
	}
	// Compute the geometry
	mpu_compute_geometry(data,geometry);
	
	return 0;
}


/******************************************************************************
	_mpu_data_wrnext
*******************************************************************************	
	Advances the write pointer. Do not call if the buffer is full.
*******************************************************************************/
void _mpu_data_wrnext(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		mpu_data_wrptr = (mpu_data_wrptr+1)&(MPU_MOTIONBUFFERSIZE-1);
	}
}
/******************************************************************************
	function: _mpu_data_rdnext
*******************************************************************************	
	Advances the read pointer to access the next sample in the data buffer
	at index mpu_data_rdptr. 
	Do not call if the buffer is empty.
*******************************************************************************/
void _mpu_data_rdnext(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		mpu_data_rdptr = (mpu_data_rdptr+1)&(MPU_MOTIONBUFFERSIZE-1);
	}
}

/******************************************************************************
*******************************************************************************
ACC GYRO CONFIG   ACC GYRO CONFIG   ACC GYRO CONFIG   ACC GYRO CONFIG   ACC GYR
*******************************************************************************
*******************************************************************************/


/******************************************************************************
	function: mpu_init
*******************************************************************************	
	Initialise the MPU, the interrupt driven read, the magnetometer shadowing
	and puts the MPU and magnetometer in off mode.
	
	Loads the non-volatile parameters from EEPROM which are: 
	- magnetic correction mode and user magnetic correction coefficient
	- sensitivity of the accelerometer
	- sensitivity of the gyroscope
	
*******************************************************************************/
void mpu_init(void)
{
	//fprintf(file_pri,"Checking mpu_getwhoami

	// Inititialize the interrupt-based mpu read
	//system_led_set(0b111); _delay_ms(800);
	//mpu_get_agt_int_init();

	// Reset the MPU
	//system_led_set(0b110); _delay_ms(800);
	mpu_reset();

	_mpu_wakefromsleep();		// Added 2019.12.14


	// Deactivates the I2C interface (only SPI communication from now on)
	//system_led_set(0b101); _delay_ms(800);
	mpu_writereg_wait_poll_block(ICM_R_USR_CTRL,0x10);

	// Optionally print register content
	//mpu_printreg(file_pri);
	//mpu_printreg2(file_pri);

	// Initialize the mpu interrupt 'style'	
	//system_led_set(0b100); _delay_ms(800);
	mpu_set_interruptpin(0x00);			// Active high; push-pull; pulse; status read clears int; fsync active high; fsync doesn't cause interrupts; no bypass
	// Disable interrupt
	//system_led_set(0b011); _delay_ms(800);
	mpu_set_interrutenable_icm20948(0,0,0,0,0,0,0,0);
	// Set the DLPF for temperature sensing
	mpu_selectbank(2);
	mpu_writereg_wait_poll_block(ICM_R_TEMP_CONFIG,6);
	mpu_selectbank(0);

	// Calibration
	//system_led_set(0b010); _delay_ms(800);
	mpu_calibrate();

	// Read magnetic field parameters
	//system_led_set(0b001); _delay_ms(800);

	mpu_config_motionmode(MPU_MODE_102HZ_ACCGYROMAG10,0);

	fprintf(file_pri,"Checking magnetometer... ");
	if(mpu_mag_isok())
		fprintf(file_pri,"Ok\n");
	else
		fprintf(file_pri,"Error\n");

	//system_led_set(0b111); _delay_ms(800);
	//_mpu_mag_readasa();
	//system_led_set(0b110); _delay_ms(800);
	//fprintf(file_pri,"Mag ASA: %02X %02X %02X\n",_mpu_mag_asa[0],_mpu_mag_asa[1],_mpu_mag_asa[2]);
	//system_led_set(0b101); _delay_ms(800);
	mpu_mag_loadcalib();	
	//system_led_set(0b100); _delay_ms(800);
	mpu_mag_printcalib(file_pri);

	// Load the non-volatile acceleration and gyroscope scales
	unsigned char scale;
	scale = mpu_LoadAccScale();
	mpu_setaccscale(scale);
	fprintf(file_pri,"%sAcc scale: %d\n",_str_mpu,scale);
	scale = mpu_LoadGyroScale();
	mpu_setgyroscale(scale);
	fprintf(file_pri,"%sGyro scale: %d\n",_str_mpu,scale);
	// Load beta
	mpu_LoadBeta();
	fprintf(file_pri,"%sBeta: %f\n",_str_mpu,_mpu_beta);
	// Dump status
	//system_led_set(0b010); _delay_ms(800);
	//mpu_printregdesc(file_pri);	
	// Turn off
	//system_led_set(0b011); _delay_ms(800);
	mpu_config_motionmode(MPU_MODE_OFF,0);

	// Clear possible auto-acquire buffers, if mpu_init is called multiple times.
	mpu_data_clear();
	// Don't kill any channel
	_mpu_kill=0;
}

/******************************************************************************
	Function: mpu_mode_accgyro
*******************************************************************************
	Set the MPU9250 in normal gyroscope and accelerometer mode
	- Gyroscope: on
	- Temperature: on
	- Accelerometer: on
	- clock: PLL
	
	MPU: ICM20948

	Parameters:
		gdlpe		-		1 to enable gyroscope DLP.
							if enabled, gdlpbw specifies the bandwidth.
		gdlpbw		-		when DLP on, set the DLP low pass filter. 
							Possible values: MPU_GYR_LPF_250, 184, 92, 41, 20, 10, 5, 3600
		adlpe		-		1 to enable the accelerometer DLP
		adlpbw		-		bandwidth of the accelerometer DLP filter (MPU_ACC_LPF_460... MPU_ACC_LPF_5)
		divider		-		divide the output of the DLP filter block by 1/(1+divider)
	
******************************************************************************/
void mpu_setdefaults()
{
	fprintf(file_pri,"Setting default parameters\n");
	fprintf(file_pri,"\tDefault acceleration scale: +/-16G\n");
	mpu_setandstoreaccscale(MPU_ACC_SCALE_16);
	fprintf(file_pri,"\tDefault gyroscope scale: +/-2000dps\n");
	mpu_setandstoregyrocale(MPU_GYR_SCALE_2000);
	float beta=0.35;
	fprintf(file_pri,"\tDefault beta: %f\n",beta);
	mpu_setandstorebeta(beta);
	fprintf(file_pri,"\tDefault magnetic calibration to user mode\n");
	mpu_mag_setandstorecorrectionmode(2);
	fprintf(file_pri,"\t->Run magnetic calibration now\n");
}

/******************************************************************************
	Function: mpu_mode_accgyro
*******************************************************************************
	Set the MPU9250 in normal gyroscope and accelerometer mode
	- Gyroscope: on
	- Temperature: on
	- Accelerometer: on
	- clock: PLL

	MPU: ICM20948

	Parameters:
		gdlpe		-		1 to enable gyroscope DLP.
							if enabled, gdlpbw specifies the bandwidth.
		gdlpbw		-		when DLP on, set the DLP low pass filter.
							Possible values: MPU_GYR_LPF_250, 184, 92, 41, 20, 10, 5, 3600
		adlpe		-		1 to enable the accelerometer DLP
		adlpbw		-		bandwidth of the accelerometer DLP filter (MPU_ACC_LPF_460... MPU_ACC_LPF_5)
		divider		-		divide the output of the DLP filter block by 1/(1+divider)

******************************************************************************/
//void mpu_mode_accgyro(unsigned char gdlpe,unsigned char gdlpoffhbw,unsigned char gdlpbw,unsigned char adlpe,unsigned char adlpbw,unsigned char divider)
void mpu_mode_accgyro(unsigned char gdlpe,unsigned char gdlpbw,unsigned char adlpe,unsigned char adlpbw,unsigned char divider)
{
#if 0
	// MPU9250 codepath
	unsigned char conf,gconf;
	unsigned char gfchoice_b;
	
	//printf_P(PSTR("mpu_mode_accgyro: %d %d %d %d %d %d\n"),gdlpe,gdlpoffhbw,gdlpbw,adlpe,adlpbw,divider);
	
	// Sanitise
	gdlpe=gdlpe?1:0;
	adlpe=adlpe?0:1;		// convert to fchoice
	gdlpoffhbw=gdlpoffhbw?1:0;
	if(gdlpbw>MPU_GYR_LPF_3600) gdlpbw=MPU_GYR_LPF_3600;
	if(adlpbw>MPU_ACC_LPF_5) adlpbw=MPU_ACC_LPF_5;
		
	// Gyro fchoice
	if(gdlpe)
		gfchoice_b=0b00;
	else
		if(gdlpoffhbw)
			gfchoice_b=0b01;
		else
			gfchoice_b=0b10;
	
	
	_mpu_wakefromsleep();
	mpu_clksel(0b001);												// PLL
	mpu_temp_enable(1);												// Temperature
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_2,0b00000000);						// Enable accel, enable gyro
	mpu_writereg_wait_poll_block(MPU_R_ACCELCONFIG2,(adlpe<<3)|adlpbw);				// Accel DLP and bandwidth
	conf = mpu_readreg_wait_poll_block(MPU_R_CONFIG);
	gconf = mpu_readreg_wait_poll_block(MPU_R_GYROCONFIG);	
	mpu_writereg_wait_poll_block(MPU_R_CONFIG,(conf&0b11111000)|gdlpbw);			// Gyro lp filter
	mpu_writereg_wait_poll_block(MPU_R_GYROCONFIG,(gconf&0b11111100)|gfchoice_b);	// Gyro dlp
	mpu_setsrdiv(divider);	
#endif
	// ICM20948 codepath
	//fprintf(file_pri,"mpu_mode_accgyro: Gyro: %d %d Acc: %d %d Div: %d\n",gdlpe,gdlpbw,adlpe,adlpbw,divider);

	// Sanitise
	gdlpe=gdlpe?1:0;
	adlpe=adlpe?1:0;
	gdlpbw&=0b111;
	adlpbw&=0b111;

	_mpu_wakefromsleep();
	mpu_clksel(0b001);												// PLL
	mpu_temp_enable(1);												// Temperature
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_2,0b00000000);						// Enable gyro, enable accel
	// Accel settings
	unsigned char cb = mpu_selectbank(2);							// Registers in bank 2
	unsigned char r = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG);
	r&=0b00000110;													// Preserve full scale
	r|=adlpe;													// Set fchoice if dlpenable
	r|=(adlpbw<<3);
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG,r);								// DLP parameters

	// Gyro settings
	r = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_1);
	r&=0b00000110;													// Preserve full scale
	r|=gdlpe;														// Set fchoice if gdlpe
	r|=(gdlpbw<<3);
	//fprintf(file_pri,"setting r: %02Xn",r);
	mpu_writereg_wait_poll_block(ICM_R_GYRO_CONFIG_1,r);							// DLP parameters

	mpu_selectbank(cb);												// Restore to previous bank

	mpu_set_accdivider(divider);									// Set accelerometer divider (possibly unnecessary as gyro dominates)
	mpu_set_gyrodivider(divider);									// Set gyroscope divider


}

/******************************************************************************
	mpu_mode_gyro
*******************************************************************************
	Set the ICM20948 in normal gyroscope-only mode
	- Gyroscope: on
	- Temperature: on
	- Accelerometer: off

	MPU: ICM20948
	
	Parameters:

	gdlpe	-		1 to enable gyroscope DLPF
	gdlpbw:     	when DLP on, set the DLP low pass filter (3 bits).
	divider:    	divide the ODR of the DLP filter block by 1/(1+divider)

******************************************************************************/
//void mpu_mode_gyro(unsigned char gdlpe,unsigned char gdlpoffhbw,unsigned char gdlpbw,unsigned char divider)
void mpu_mode_gyro(unsigned char gdlpe,unsigned char gdlpbw,unsigned char divider)
{
#if 0
	// MPU9250
	unsigned char conf,gconf;
	unsigned char gfchoice_b;
	
	//printf_P(PSTR("mpu_mode_gyro: %d %d %d %d\n"),gdlpe,gdlpoffhbw,gdlpbw,divider);
	
	// Sanitise
	gdlpe=gdlpe?1:0;
	gdlpoffhbw=gdlpoffhbw?1:0;
	if(gdlpbw>MPU_GYR_LPF_3600) gdlpbw=MPU_GYR_LPF_3600;
	
	// Gyro fchoice
	if(gdlpe)
		gfchoice_b=0b00;
	else
		if(gdlpoffhbw)
			gfchoice_b=0b01;
		else
			gfchoice_b=0b10;
	
	_mpu_wakefromsleep();
	mpu_clksel(0b001);												// PLL
	mpu_temp_enable(1);												// Temperature
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_2,0b00111000);						// Enable gyro, disable accel
	conf = mpu_readreg_wait_poll_block(MPU_R_CONFIG);									
	gconf = mpu_readreg_wait_poll_block(MPU_R_GYROCONFIG);	
	mpu_writereg_wait_poll_block(MPU_R_CONFIG,(conf&0b11111000)|gdlpbw);			// Gyro lp filter
	mpu_writereg_wait_poll_block(MPU_R_GYROCONFIG,(gconf&0b11111100)|gfchoice_b);	// Gyro dlp	
	mpu_setsrdiv(divider);
#endif
	// ICM20948
	//fprintf(file_pri,"mpu_mode_gyro: %d %d %d\n",gdlpe,gdlpbw,divider);
	// Sanitise
	gdlpe=gdlpe?1:0;											// gdlpe is gyro_fchoice: 1 is enable DLPF
	gdlpbw&=0b111;

	_mpu_wakefromsleep();
	mpu_clksel(0b001);												// PLL
	mpu_temp_enable(1);												// Temperature
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_2,0b00111000);						// Enable gyro, disable accel

	unsigned char cb = mpu_selectbank(2);							// Registers in bank 2
	unsigned char r = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_1);
	r&=0b00000110;													// Preserve full scale
	r|=gdlpe;														// Set fchoice if gdlpe
	r|=(gdlpbw<<3);
	//fprintf(file_pri,"setting r: %02Xn",r);
	mpu_writereg_wait_poll_block(ICM_R_GYRO_CONFIG_1,r);							// DLP parameters
	mpu_selectbank(cb);												// Restore to previous bank
	mpu_set_gyrodivider(divider);


}


/******************************************************************************
	mpu_mode_acc
*******************************************************************************
	Set the MPU9250 in normal accelerometer-only mode.
	- Gyroscope: off
	- Temperature: on
	- Accelerometer: on
	
	dlpenable: if enabled, dlpbw specifies the bandwidth of the digital low-pass filter
	dlpbw:     bandwidth of the DLP filter (MPU_ACC_LPF_460... MPU_ACC_LPF_5)
	divider:   divide the output of the DLP filter block by 1/(1+divider)
	
	The output data rate is 4KHz when dlpenable is deactivated; 1KHz/(1+divider) when dlp is enabled
******************************************************************************/
void mpu_mode_acc(unsigned char dlpenable,unsigned char dlpbw,unsigned short divider)
{	
#if 0
	// MPU9250 codepath

	// Sanitise
	dlpenable=dlpenable?0:1;		// Convert to fchoice_b
	if(dlpbw>MPU_ACC_LPF_5) dlpbw=MPU_ACC_LPF_5;
	
	
	_mpu_wakefromsleep();
	mpu_clksel(0b000);												// Internal oscillator
	mpu_temp_enable(1);												// Temperature
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_2,0b00000111);						// Enable accel, disable gyro
	mpu_writereg_wait_poll_block(MPU_R_ACCELCONFIG2,(dlpenable<<3)|dlpbw);
	mpu_setsrdiv(divider);
#endif
	// ICM20948 codepath
	//fprintf(file_pri,"mpu_mode_acc: %d %d %d\n",dlpenable,dlpbw,divider);
	// Sanitise
	dlpenable=dlpenable?1:0;								// dlpenable is accel_fchoice: 1 means DLP enabled
	dlpbw&=0b111;

	_mpu_wakefromsleep();
	mpu_clksel(0b000);												// Internal oscillator
	mpu_temp_enable(1);												// Temperature
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_2,0b00000111);						// Enable accel, disable gyro

	unsigned char cb = mpu_selectbank(2);							// Registers in bank 2
	unsigned char r = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG);
	r&=0b00000110;													// Preserve full scale
	r|=dlpenable;													// Set fchoice if dlpenable
	r|=(dlpbw<<3);
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG,r);								// DLP parameters
	mpu_selectbank(cb);												// Restore to previous bank
	mpu_set_accdivider(divider);									// Set accelerometer divider
}


/******************************************************************************
	mpu_mode_lpacc
*******************************************************************************
	Set the ICM20948 in low-power accelerometer-only mode.
	- Gyroscope: off
	- Temperature: off
	- Accelerometer: duty-cycled (low power)

	The output data rate is defined as: ODR = 1.125KHz/(1+divider)

******************************************************************************/
//void mpu_mode_lpacc(unsigned char lpodr)
void mpu_mode_lpacc_icm20948(unsigned int divider)
{
	fprintf(file_pri,"mpu_mode_lpacc: %d\n",divider);
#if 0
	// MPU9250

	_mpu_wakefromsleep();
	mpu_clksel(0b000);												// Internal oscillator
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_2,0b00000111);						// Enable accel, disable gyro
	// TODO: CHECK whether fchoice and DLP influence LP operation
	mpu_writereg_wait_poll_block(MPU_R_ACCELCONFIG2,0b00001000);					// fchoice_b=1: DLP disabled; LP=x
	mpu_writereg_wait_poll_block(MPU_R_LPODR,lpodr);								// LP ODR
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_1,0b00101000);						// Enable cycle mode (LP), deactivate temp
#endif
	// ICM20948
	_mpu_wakefromsleep();
	mpu_clksel(0b000);												// Internal oscillator
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_2,0b00000111);						// Accel all axes on, gyro all axes off
	// Following registers are in bank 2
	unsigned char cb = mpu_selectbank(2);
	unsigned char r = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG);
	r&=0b00000110;													// Preserve full scale, DLP disabled, DLPCFG=0 (unused)
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG,r);								// DLP parameters
	mpu_selectbank(cb);												// Restore to previous bank
	mpu_set_accdivider(divider);									// Set divider
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,0b00101000);						// Enable LP_EN; deactivate temp; clock source internal (ICM)
	mpu_writereg_wait_poll_block(ICM_R_LP_CONFIG,0b00100000);						// Enable ACCEL_CYCLE


}

/******************************************************************************
	function: mpu_mode_off
*******************************************************************************
	Turns of as many things to lower power consumtion.
	- Gyroscope: off
	- Temperature: off
	- Accelerometer: off
	- Magnetometer off
	
	Updated for ICM20948

	Note / FIX: LP_EN in PWR_MGMT_1 is not activated; verify if this is correct.

	This function can be called anytime. 
	First, the communication with the magnetometer is shortly enabled 
	(by activating the motion processor) in order to turn it off. Then the 
	accelerometer, gyroscope and temperature sensors are turned off.
******************************************************************************/
void mpu_mode_off(void)
{
#if 0
	unsigned char gconf;
	
	// Enable the motion processor for magnetometer communication
	_mpu_defaultdlpon();
	// Shut down magnetometer sampling
	_mpu_mag_mode(0,0);
	
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_2,0b00111111);							// Disable accel + gyro
	mpu_writereg_wait_poll_block(MPU_R_ACCELCONFIG2,0b00001000);						// Disable accel DLP
	gconf = mpu_readreg_wait_poll_block(MPU_R_GYROCONFIG);	
	mpu_writereg_wait_poll_block(MPU_R_GYROCONFIG,(gconf&0b11111100)|0b01);				// Disable gyro dlp
	mpu_writereg_wait_poll_block(MPU_R_PWR_MGMT_1,0b01001000);							// Sleep, no cycle, no gyro stby, disable temp, internal osc
#endif

	unsigned char aconf,gconf;


	// Enable the motion processor for magnetometer communication
	_mpu_defaultdlpon();
	// Shut down magnetometer sampling
	_mpu_mag_mode(0,0);

	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_2,0b00111111);							// Disable accel + gyro (ICM20948 OK)

	// Following registers are in bank 2
	mpu_selectbank(2);
	aconf = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG);			// Get accel config
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG,aconf&0b11111110);					// Disable accel DLP (clear accel_fchoice) without changing other settings (ICM20948)
	gconf = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_1);			// Get gyro config ICM20948
	mpu_writereg_wait_poll_block(ICM_R_GYRO_CONFIG_1,gconf&0b11111110);					// Disable gyro DLP (clear gyro_fchoice) without changing other settings (ICM20948)
	// Following registers are in bank 0
	mpu_selectbank(0);
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,0b01001000);							// Sleep; LP not enabled; temp disabled; internal oscillator (ICM20948)


}



/******************************************************************************
	function: mpu_get_agt
*******************************************************************************
	Blocking reads of the 14 registers comprising acceleration, gyroscope and temperature.

	Updated for ICM20948.
******************************************************************************/
void mpu_get_agt(signed short *ax,signed short *ay,signed short *az,signed short *gx,signed short *gy,signed short *gz,signed short *temp)
{
	unsigned char v[14];
	unsigned short s1,s2,s3;
	
	mpu_readregs_wait_poll_or_int_block(v,ICM_R_ACCEL_XOUT_H,14);
	
	s1=v[0]; s1<<=8; s1|=v[1];
	s2=v[2]; s2<<=8; s2|=v[3];
	s3=v[4]; s3<<=8; s3|=v[5];
	*ax = s1; *ay = s2; *az = s3;
	s1=v[6]; s1<<=8; s1|=v[7];
	s2=v[8]; s2<<=8; s2|=v[9];
	s3=v[10]; s3<<=8; s3|=v[11];
	*gx = s1; *gy = s2; *gz = s3;
	s1=v[12]; s1<<=8; s1|=v[13];
	*temp = s1;
	signed int t = s1;
	// Correction: tcelsius = temp/333.87 + 21 -> temp*100/333.87 + 2100
	t = t * 2995 / 10000 + 2100;
	*temp = t;
}
/******************************************************************************
	mpu_get_agmt
*******************************************************************************
	Blocking read of acceleration, gyroscope, magnetometer and temperature.

	Updated for ICM20948.
******************************************************************************/
void mpu_get_agmt(signed short *ax,signed short *ay,signed short *az,signed short *gx,signed short *gy,signed short *gz,signed short *mx,signed short *my,signed short *mz,unsigned char *ms,signed short *temp)
{
	unsigned char v[21];
	unsigned short s1,s2,s3;

	mpu_readregs_wait_poll_or_int_block(v,ICM_R_ACCEL_XOUT_H,22);

	s1=v[0]; s1<<=8; s1|=v[1];
	s2=v[2]; s2<<=8; s2|=v[3];
	s3=v[4]; s3<<=8; s3|=v[5];
	*ax = s1; *ay = s2; *az = s3;
	s1=v[6]; s1<<=8; s1|=v[7];
	s2=v[8]; s2<<=8; s2|=v[9];
	s3=v[10]; s3<<=8; s3|=v[11];
	*gx = s1; *gy = s2; *gz = s3;
	s1=v[12]; s1<<=8; s1|=v[13];
	// Temp
	*temp = s1;
	signed int t = s1;
	// Correction: tcelsius = temp/333.87 + 21 -> temp*100/333.87 + 2100
	t = t * 2995 / 10000 + 2100;
	*temp = t;
	// Magn
	s1=v[15]; s1<<=8; s1|=v[14];
	s2=v[17]; s2<<=8; s2|=v[16];
	s3=v[19]; s3<<=8; s3|=v[18];
	*mx = s1; *my = s2; *mz = s3;
	*ms=(v[21]>>3)&1;
}
/******************************************************************************
	mpu_get_g
*******************************************************************************	
	Blocking reads of the 6 registers comprising gyroscope.
	
******************************************************************************/
void mpu_get_g(signed short *gx,signed short *gy,signed short *gz)
{
	unsigned char v[6];
	unsigned short s1,s2,s3;
	
	mpu_readregs_wait_poll_or_int_block(v,67,6);
	
	s1=v[0]; s1<<=8; s1|=v[1];
	s2=v[2]; s2<<=8; s2|=v[3];
	s3=v[4]; s3<<=8; s3|=v[5];
	*gx = s1; *gy = s2; *gz = s3;
}
/******************************************************************************
	mpu_get_a
*******************************************************************************	
	Blocking reads of the 6 registers comprising gyroscope.
******************************************************************************/
void mpu_get_a(signed short *ax,signed short *ay,signed short *az)
{
	unsigned char v[6];
	unsigned short s1,s2,s3;
	
	mpu_readregs_wait_poll_or_int_block(v,59,6);
	
	s1=v[0]; s1<<=8; s1|=v[1];
	s2=v[2]; s2<<=8; s2|=v[3];
	s3=v[4]; s3<<=8; s3|=v[5];
	*ax = s1; *ay = s2; *az = s3;
}


/******************************************************************************
	mpu_getfifocnt
*******************************************************************************
	Returns the FIFO level

	MPU: ICM20948
******************************************************************************/
unsigned short mpu_getfifocnt(void)
{
	unsigned char fch = mpu_readreg_wait_poll_block(ICM_R_FIFO_COUNTH);
	unsigned char fcl = mpu_readreg_wait_poll_block(ICM_R_FIFO_COUNTL);
	unsigned short fc = fch; fc<<=8; fc|=fcl;

	return fc;
}
/******************************************************************************
	function: mpu_fifoenable
*******************************************************************************
	Sets the FIFO flags (register FIFO Enable=35d), sets the FIFO enable bit
	(register User Control=106d) and sets the FIFO reset bit (register User Control=106d).
	
	MPU: ICM20948

	Parameters:
		flags	-	FIFO enable flags. This bitmask is composed of the following bits:
					SLV3 SLV2 SLV1 SLV0 ACC GZ GY GX TMP
		en		-	1 to enable the FIFO (reg 106d)
		rst		-	1 to reset the FIFO (reg 106d)
******************************************************************************/
void mpu_fifoenable(unsigned short flags,unsigned char en,unsigned char reset)
{
#if 0
	// MPU9250
	en=en?1:0;
	reset=reset?1:0;
	mpu_writereg_wait_poll_block(MPU_R_FIFOEN,flags);
	unsigned char usr = mpu_readreg_wait_poll_block(MPU_R_USR_CTRL);
	usr = usr&0b10111011;
	usr = usr|(en<<6)|(reset<<2);	
	mpu_writereg_wait_poll_block(MPU_R_USR_CTRL,usr);
#endif
	// ICM20948
	//fprintf(file_pri,"FIFO control: flags: %04Xh en: %d reset: %d\n",flags,en,reset);
	// Sanitise
	en=en?1:0;
	reset=reset?1:0;
	flags&=0b111111111;
	// Set channels to enable
	mpu_writereg_wait_poll_block(ICM_R_FIFO_EN_1,flags>>5);
	mpu_writereg_wait_poll_block(ICM_R_FIFO_EN_2,flags&0b11111);
	// Enable FIFO and reset
#if 0
	// Here assume that ICM20948 SRAM_RESET in USR_CTRL is similar to FIFO RST in MPU9250
	// Verified experimentally to behave identically as MPU9250
	unsigned char usr = mpu_readreg_wait_poll_block(ICM_R_USR_CTRL);
	usr = usr&0b10111011;					// Clear enable and sram_reset
	usr = usr|(en<<6)|(reset<<2);			// ICM20948 bit 2 is SRAM_RST whereas MPU9250 is FIFO reset. ICM20948 FIFO reset is in dedicated reg
	mpu_writereg_wait_poll_block(ICM_R_USR_CTRL,usr);
#else
	// Here assume that ICM20948 reset is in FIFO_RST, but documentation unclear about how the 5 bits must be set.
	// Experimentally behaves the same the codepath using SRAM_RST
	unsigned char usr = mpu_readreg_wait_poll_block(ICM_R_USR_CTRL);
	usr = usr&0b10111111;					// Clear enable
	usr = usr|(en<<6);						// ICM20948 bit 2 is SRAM_RST which is left unchanged
	mpu_writereg_wait_poll_block(ICM_R_USR_CTRL,usr);
	// Reset FIFO (ICM20948)
	mpu_writereg_wait_poll_block(ICM_R_FIFO_RST,(reset<<4)|(reset<<3)|(reset<<2)|(reset<<1)|(reset));		// Doc unclear about which bits are reset, so set/clear all
#endif
}

/******************************************************************************
	mpu_fiforead
*******************************************************************************	
	Read n bytes from the FIFO

	MPU: ICM20948
******************************************************************************/
void mpu_fiforead(unsigned char *fifo,unsigned short n)
{
	for(unsigned short i=0;i<n;i++)
	{
		fifo[i] = mpu_readreg_wait_poll_block(ICM_R_FIFO_RW);			// ICM20948
	}
}
/******************************************************************************
	mpu_fiforeadshort
*******************************************************************************	
	Read n 16-bit shorts from the FIFO
******************************************************************************/
void mpu_fiforeadshort(short *fifo,unsigned short n)
{
	unsigned char *t1=((unsigned char*)fifo)+1;
	unsigned char *t2=((unsigned char*)fifo)+0;
	for(unsigned short i=0;i<n;i++)
	{
		*t1=mpu_readreg_wait_poll_block(ICM_R_FIFO_RW);
		*t2=mpu_readreg_wait_poll_block(ICM_R_FIFO_RW);
		t1+=2;
		t2+=2;
	}
}

/******************************************************************************
	Function: mpu_setgyrobias
*******************************************************************************	
	Sets the gyro bias registers.

	Gyro bias is added to gyro readout by MPU.
	In ICM20948 the bias is referred to the full scale 2 (500dps). If another
	scale is used it must be scaled accordingly.

	MPU: ICM20948
******************************************************************************/
void mpu_setgyrobias(short bgx,short bgy,short bgz)
{
#if 0
	// MPU9250 codepath
	mpu_writereg_wait_poll_block(19,bgx>>8);
	mpu_writereg_wait_poll_block(20,bgx&0xff);
	mpu_writereg_wait_poll_block(21,bgy>>8);
	mpu_writereg_wait_poll_block(22,bgy&0xff);
	mpu_writereg_wait_poll_block(23,bgz>>8);
	mpu_writereg_wait_poll_block(24,bgz&0xff);
#endif
	// ICM20948 codepath
	unsigned char pb = mpu_selectbank(2);
	mpu_writereg_wait_poll_block(ICM_R_XG_OFFS_USRH,bgx>>8);
	mpu_writereg_wait_poll_block(ICM_R_XG_OFFS_USRL,bgx&0xff);
	mpu_writereg_wait_poll_block(ICM_R_YG_OFFS_USRH,bgy>>8);
	mpu_writereg_wait_poll_block(ICM_R_YG_OFFS_USRL,bgy&0xff);
	mpu_writereg_wait_poll_block(ICM_R_ZG_OFFS_USRH,bgz>>8);
	mpu_writereg_wait_poll_block(ICM_R_ZG_OFFS_USRL,bgz&0xff);
	mpu_selectbank(pb);
}
/******************************************************************************
	Function: mpu_setgyroscale
*******************************************************************************	
	Sets the gyro scale.
	
	If the MPU is off it is turned on and then back off to get the parameter.
	
	Parameters:
		scale	-	One of MPU_GYR_SCALE_250, MPU_GYR_SCALE_500, MPU_GYR_SCALE_1000 or MPU_GYR_SCALE_2000
******************************************************************************/
void mpu_setgyroscale(unsigned char scale)
{
#if 0
	scale&=0b11;					// Sanitise	
	unsigned char oldmode,oldautoread;
	
	//printf("set gyro scale\n");
	
	// Save mode
	oldmode=mpu_get_motionmode(&oldautoread);
	mpu_config_motionmode(MPU_MODE_100HZ_ACC_BW41_GYRO_BW41_MAG_8,0);
	
	unsigned char gconf = mpu_readreg_wait_poll_block(MPU_R_GYROCONFIG);	
	mpu_writereg_wait_poll_block(MPU_R_GYROCONFIG,(gconf&0b11100111)|(scale<<3));
	
	switch(scale)
	{
		case MPU_GYR_SCALE_250:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/131.072;
			#else
			mpu_gtorps=3.14159665k/180.0k/131.072k;
			#endif			
			break;
		case MPU_GYR_SCALE_500:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/65.536;
			#else
			mpu_gtorps=3.14159665k/180.0k/65.536k;
			#endif
			break;
		case MPU_GYR_SCALE_1000:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/32.768;
			#else
			mpu_gtorps=3.14159665k/180.0k/32.768k;
			#endif
			break;
		default:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/16.384;
			#else
			mpu_gtorps=3.14159665k/180.0k/16.384k;
			#endif
			break;
	}
	// Restore
	mpu_config_motionmode(oldmode,oldautoread);
#endif
	scale&=0b11;					// Sanitise
	unsigned char oldmode,oldautoread;

	//printf("set gyro scale\n");

	// Save mode
	oldmode=mpu_get_motionmode(&oldautoread);
	mpu_config_motionmode(MPU_MODE_102HZ_ACCGYROMAG10,0);

	unsigned char pb = mpu_selectbank(2);
	unsigned char gconf = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_1);
	gconf=(gconf&0b11111001)|(scale<<1);
	mpu_writereg_wait_poll_block(ICM_R_GYRO_CONFIG_1,gconf);
	mpu_selectbank(pb);

	// Restore
	mpu_config_motionmode(oldmode,oldautoread);

	switch(scale)
	{
		case MPU_GYR_SCALE_250:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/131.072;
			#else
			mpu_gtorps=3.14159665k/180.0k/131.072k;
			#endif
			break;
		case MPU_GYR_SCALE_500:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/65.536;
			#else
			mpu_gtorps=3.14159665k/180.0k/65.536k;
			#endif
			break;
		case MPU_GYR_SCALE_1000:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/32.768;
			#else
			mpu_gtorps=3.14159665k/180.0k/32.768k;
			#endif
			break;
		default:
			#ifdef __cplusplus
			mpu_gtorps=3.14159665/180.0/16.384;
			#else
			mpu_gtorps=3.14159665k/180.0k/16.384k;
			#endif
			break;
	}


}
/******************************************************************************
	Function: mpu_getgyroscale
*******************************************************************************	
	Sets the gyro scale.
	
	If the MPU is off it is turned on and then back off to get the parameter.
	
	Returns:
		One of MPU_GYR_SCALE_250, MPU_GYR_SCALE_500, MPU_GYR_SCALE_1000 or MPU_GYR_SCALE_2000
******************************************************************************/
unsigned char mpu_getgyroscale(void)
{
#if 0
	unsigned char scale;
	unsigned char oldmode,oldautoread;
	
	//printf("get gyro scale\n");
	
	// Save mode
	oldmode=mpu_get_motionmode(&oldautoread);
	mpu_config_motionmode(MPU_MODE_100HZ_ACC_BW41_GYRO_BW41_MAG_8,0);
	
	unsigned char gconf = mpu_readreg_wait_poll_block(MPU_R_GYROCONFIG);	
	scale = (gconf>>3)&0b11;
	
	// Restore
	mpu_config_motionmode(oldmode,oldautoread);
	
	return scale;
#else
	// ICM20948
	unsigned char pb = mpu_selectbank(2);
	unsigned char scale = (mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_1)>>1)&0b11;
	mpu_selectbank(pb);
	return scale;
#endif
}

/******************************************************************************
	Function: mpu_setaccscale
*******************************************************************************	
	Sets the accelerometer scale.
	
	If the MPU is off it is turned on and then back off to set the parameter.
	
	Parameters:
		scale	-	One of MPU_ACC_SCALE_2, MPU_ACC_SCALE_4, MPU_ACC_SCALE_8, MPU_ACC_SCALE_16
******************************************************************************/
void mpu_setaccscale(unsigned char scale)
{
#if 0
	scale&=0b11;					// Sanitise
	unsigned char oldmode,oldautoread;
	
	//printf("set acc scale\n");
	
	// Save mode
	oldmode=mpu_get_motionmode(&oldautoread);
	mpu_config_motionmode(MPU_MODE_100HZ_ACC_BW41_GYRO_BW41_MAG_8,0);

	// Set the parameter
	unsigned char aconf = mpu_readreg_wait_poll_block(MPU_R_ACCELCONFIG);	
	aconf=(aconf&0b11100111)|(scale<<3);
	mpu_writereg_wait_poll_block(MPU_R_ACCELCONFIG,aconf);
	
	// Restore
	mpu_config_motionmode(oldmode,oldautoread);
	
	/*
	// Must complete code for acceleration
	switch(scale)
	{
		case MPU_ACC_SCALE_2:
			//mpu_gtorps=3.14159665k/180.0k/131.072k;
			break;
		case MPU_ACC_SCALE_4:
			//mpu_gtorps=3.14159665k/180.0k/65.536k;
			break;
		case MPU_ACC_SCALE_8:
			//mpu_gtorps=3.14159665k/180.0k/32.768k;
			break;
		default:
			//mpu_gtorps=3.14159665k/180.0k/16.384k;
			break;
	}*/
#endif
	// ICM20948 codepath
	scale&=0b11;					// Sanitise
	unsigned char oldmode,oldautoread;

	//printf("set acc scale\n");

	// Save mode
	oldmode=mpu_get_motionmode(&oldautoread);
	mpu_config_motionmode(MPU_MODE_102HZ_ACCGYROMAG10,0);

	// Set the parameter
	unsigned char pb = mpu_selectbank(2);
	unsigned char aconf = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG);
	aconf=(aconf&0b11111001)|(scale<<1);
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG,aconf);
	mpu_selectbank(pb);

	// Restore
	mpu_config_motionmode(oldmode,oldautoread);


	// Must complete code for acceleration
	/*switch(scale)
	{
		case MPU_ACC_SCALE_2:
			//mpu_gtorps=3.14159665k/180.0k/131.072k;
			break;
		case MPU_ACC_SCALE_4:
			//mpu_gtorps=3.14159665k/180.0k/65.536k;
			break;
		case MPU_ACC_SCALE_8:
			//mpu_gtorps=3.14159665k/180.0k/32.768k;
			break;
		default:
			//mpu_gtorps=3.14159665k/180.0k/16.384k;
			break;
	}*/
}
/******************************************************************************
	Function: mpu_getaccscale
*******************************************************************************	
	Gets the acceleromter scale.
	
	If the MPU is off it is turned on and then back off to get the parameter.
	
	Returns:
		One of MPU_ACC_SCALE_2, MPU_ACC_SCALE_4, MPU_ACC_SCALE_8, MPU_ACC_SCALE_16
******************************************************************************/
unsigned char mpu_getaccscale(void)
{
#if 0
	// MPU9250 codepath
	unsigned char scale;
	unsigned char oldmode,oldautoread;
	
	//printf("get acc scale\n");
	
	// Save mode
	oldmode=mpu_get_motionmode(&oldautoread);
	mpu_config_motionmode(MPU_MODE_100HZ_ACC_BW41_GYRO_BW41_MAG_8,0);
	
	//printf("turn on\n");
	mpu_config_motionmode(MPU_MODE_100HZ_ACC_BW41_GYRO_BW41_MAG_8,0);
		
	unsigned char aconf = mpu_readreg_wait_poll_block(MPU_R_ACCELCONFIG);	
	scale = (aconf>>3)&0b11;
	
	//printf("aconf: %02X\n",aconf);
	
	// Restore
	mpu_config_motionmode(oldmode,oldautoread);
		
	return scale;
#else
	// ICM20948
	unsigned char pb = mpu_selectbank(2);
	unsigned char scale = (mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG)>>1)&0b11;
	mpu_selectbank(pb);
	return scale;
#endif
}
/******************************************************************************
	Function: mpu_setandstoreaccscale
*******************************************************************************	
	Sets the accelerometer scale and store the parameter in non-volatile memory.
	
	If the MPU is off it is turned on and then back off to set the parameter.
	
	Parameters:
		scale	-	One of MPU_ACC_SCALE_2, MPU_ACC_SCALE_4, MPU_ACC_SCALE_8, MPU_ACC_SCALE_16
******************************************************************************/
void mpu_setandstoreaccscale(unsigned char scale)
{
	mpu_setaccscale(scale);
//#warning "mpu_setandstoreaccscale must be fixed: eeprom_write_byte"
	eeprom_write_byte(CONFIG_ADDR_ACC_SCALE,scale&0b11);
}
/******************************************************************************
	Function: mpu_setandstoregyroscale
*******************************************************************************	
	Sets the gyro scale and store the parameter in non-volatile memory.
	
	If the MPU is off it is turned on and then back off to set the parameter.
	
	Parameters:
		scale	-	One of MPU_ACC_SCALE_2, MPU_ACC_SCALE_4, MPU_ACC_SCALE_8, MPU_ACC_SCALE_16
******************************************************************************/
void mpu_setandstoregyrocale(unsigned char scale)
{
	mpu_setgyroscale(scale);
	eeprom_write_byte(CONFIG_ADDR_GYRO_SCALE,scale&0b11);
}
/******************************************************************************
	mpu_setaccodr
*******************************************************************************	
	Sets low-power accelerometer output data rate (register 30d)
******************************************************************************/
/*void mpu_setaccodr(unsigned char odr)
{
#warning fix
#if 0
	mpu_writereg(MPU_R_LPODR,odr);
#endif
}*/
/******************************************************************************
	mpu_setacccfg2
*******************************************************************************	
	Sets accelerometer configuration 2 register (register 29d)
******************************************************************************/
/*void mpu_setacccfg2(unsigned char cfg)
{
#warning fix
#if 0
	mpu_writereg(MPU_R_ACCELCONFIG2,cfg);
#endif
}*/

/******************************************************************************
	function: mpu_isok
*******************************************************************************
	Returns MPU WHOAMI register

	Parameters:
		-
	Returns
		Who am I value, 0x71 for the MPU9250
******************************************************************************/
unsigned char mpu_isok(void)
{
	if(mpu_getwhoami()==0xEA)
		return 1;
	return 0;
}

/******************************************************************************
	function: mpu_getwhoami
*******************************************************************************	
	Returns MPU WHOAMI register
	
	Parameters:
		-	
	Returns
		0x71 	-	MPU9250
		0XEA	-	ICM20948
******************************************************************************/
unsigned char mpu_getwhoami(void)
{
	mpu_writereg_wait_poll_block(ICM_R_REG_BANK_SEL,ICM_B_WHOAMI);
	return mpu_readreg_wait_poll_block(ICM_R_WHOAMI);
}
/******************************************************************************
	function: mpu_selectbank
*******************************************************************************
	Select register bank 0...3

	Parameters:
		bank	-	0, 1, 2 or 3
	Returns
		previous regbank (0,1,2 or 3)
******************************************************************************/
unsigned char mpu_selectbank(unsigned char bank)
{
	unsigned char cb = (mpu_readreg_wait_poll_block(ICM_R_REG_BANK_SEL)>>4) & 0b11;
	mpu_writereg_wait_poll_block(ICM_R_REG_BANK_SEL,(bank&0b11)<<4);
	return cb;
}
/******************************************************************************
	function: mpu_reset
*******************************************************************************	
	Resets the MPU. 
	
	Set bit 7 (device reset) OR default reseet values (0x41).

	Default values:
		Auto select best available clock, PLL or internal oscillator
		Temperature enabled
		Turn off low power feature
		Sleep mode on

	Updated for ICM20948
	
	Parameters:
		-
	Returns:
		-
******************************************************************************/
void mpu_reset(void)
{
	// Set the reset bit
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,0x41 | 0b10000000);

	// Loop until register takes default reset values
	unsigned ctr=0;
	while(mpu_readreg_wait_poll_block(ICM_R_PWR_MGMT_1)!=0x41)
		ctr++;

	fprintf(file_pri,"Waited for reset %u cycles\n",ctr);

	// Add some extra delay
	_delay_ms(10);
}
/******************************************************************************
	function: mpu_setsrdiv
*******************************************************************************	
	Set sample rate divider register (register 25d).
	Sample rate = internal sample rate / (div+1)
	Only effective when fchoice=11 (fchoice_b=00) and 0<dlp_cfg<7
******************************************************************************/
/*void mpu_setsrdiv(unsigned char div)
{
	mpu_writereg(MPU_R_SMPLRT_DIV,div);
}*/
/******************************************************************************
	mpu_setgyrosamplerate
*******************************************************************************	
	Set the gyro sample rate and the digital low pass filter settings.

	fchoice:	2 bits
						x0: DLP disabled, BW=8800Hz, Fs=32KHz
						01: DLP disabled, BW=3600Hz, Fs=32KHz
						11: DLP enabled
	dlp:			3 bits
						0: BW=250Hz, Fs=8KHz
						1: BW=184Hz, FS=1KHz
						...
						7: BW=3600Hz, FS=8KHz

******************************************************************************/
/*void mpu_setgyrosamplerate(unsigned char fchoice,unsigned char dlp)
{
	unsigned char config,gconfig;
	
	// Sanitise inputs
	fchoice=~fchoice;			// Convert to fchoice_b for register
	fchoice&=0b11;
	dlp&=0b111;
	
	// Get register values	
	config = mpu_readreg(MPU_R_CONFIG);
	gconfig = mpu_readreg(MPU_R_GYROCONFIG);
	
	// Modify register values
	config = (config&0b11111000)+dlp;
	gconfig = (gconfig&11111100)+fchoice;
	
	// Write register values
	mpu_writereg(MPU_R_CONFIG,config);
	mpu_writereg(MPU_R_GYROCONFIG,gconfig);	
}*/

/******************************************************************************
	mpu_setaccsamplerate
*******************************************************************************	
	Sets accelerometer sample rate and digital low pass filter
******************************************************************************/
/*void mpu_setaccsamplerate(unsigned char fchoice,unsigned char dlp)
{
	unsigned char aconfig;
	
	// Sanitise inputs
	fchoice=~fchoice;			// Convert to fchoice_b for register
	fchoice&=0b1;
	dlp&=0b111;
	
	// Get register values	
	aconfig = mpu_readreg(MPU_R_ACCELCONFIG2);
	
	// Modify register values
	aconfig = (aconfig&0b11110000)+(fchoice<<3)+dlp;
	
	// Write register values
	mpu_writereg(MPU_R_ACCELCONFIG2,aconfig);
}*/


/******************************************************************************
	function: mpu_set_interrutenable
*******************************************************************************
	Enable the interrupts of the MPU. The parameters take a binary value to 
	activate the corresponding interrupt source (1) or deactivate it (0).
	
	Parameters:
		wom			-	wake on motion
		fifo		-	FIFO overflow (5 bits)
		fsync		-	fsync interrupt
		datardy		-	new data acquired
		pllrdy 		-	PLL ready
		dmp_int1	-	DMP interrupt 1
		i2c_mst		-	I2C master interrupt
		fifowatermark	-	FIFO watermark (5 bits)
******************************************************************************/
//void mpu_set_interrutenable(unsigned char wom,unsigned char fifo,unsigned char fsync,unsigned char datardy)
void mpu_set_interrutenable_icm20948(unsigned char wom,unsigned char fifooverflow,unsigned char fsync,unsigned char datardy, unsigned char pll_rdy,
									unsigned char dmp_int1,unsigned char i2c_mst,unsigned char fifowatermark)
{
#if 0
	// MPU9250 code path
	unsigned char v;
	v = (wom<<6)|(fifo<<4)|(fsync<<3)|datardy;
	mpu_writereg_wait_poll_block(MPU_R_INTERRUPTENABLE,v);
#endif
	// ICML20948 code path
	/*
		When LP_EN=1 in PWR_MGMT1 then INTERRUPTENABLE_1/2/3 is not accessible
		Therefore temporarily deactivate LP and restore to whichever state it was afterwards.
	 */
	unsigned char lp = mpu_get_lpen();
	//fprintf(file_pri,"Current lp: %d\n",lp);
	mpu_set_lpen(0);

	unsigned char v;
	// Reg 0
	v = ((fsync?1:0)<<7) | ((wom?1:0)<<3) | ((pll_rdy?1:0)<<2) | ((dmp_int1?1:0)<<1) | ((i2c_mst?1:0)<<0);
	mpu_writereg_wait_poll_block(ICM_R_INTERRUPTENABLE,v);
	// Reg 1
	v = datardy?1:0;
	mpu_writereg_wait_poll_block(ICM_R_INTERRUPTENABLE_1,v);
	//unsigned char tmp = mpu_readreg_wait_poll_block(ICM_R_INTERRUPTENABLE_1);
	//fprintf(file_pri,"set interrupt int1 is: %d\n",tmp);

	// Reg 2
	v = (fifooverflow&0b11111);
	mpu_writereg_wait_poll_block(ICM_R_INTERRUPTENABLE_2,v);
	// Reg 3
	v = (fifowatermark&0b11111);
	mpu_writereg_wait_poll_block(ICM_R_INTERRUPTENABLE_3,v);

	mpu_set_lpen(lp);			// Restore low power mode

}

/******************************************************************************
	function: mpu_get_lpen
*******************************************************************************
	Get LP_EN in PWR_MGMT_1

	Parameters:

	Returns:
		1			Digital circuitry in low power
		0			Digital circuitry active

******************************************************************************/
unsigned char mpu_get_lpen()
{
	unsigned char r = mpu_readreg_wait_poll_block(ICM_R_PWR_MGMT_1);
	return (r>>5)&0b1;
}
/******************************************************************************
	function: mpu_set_lpen
*******************************************************************************
	Set LP_EN in PWR_MGMT_1

	Parameters:
		lpen	-	1 to put digital circuitry in low power; 0 otherwise.
	Returns:


******************************************************************************/
void mpu_set_lpen(unsigned char lpen)
{
	lpen=lpen?1:0;
	unsigned char r = mpu_readreg_wait_poll_block(ICM_R_PWR_MGMT_1);
	r&=0b11011111;				// Clear lpen
	r|=(lpen<<5);

	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,r);
	_delay_ms(1);				// Must delay otherwise write to registers which are disabled in LP mode fails
}

/******************************************************************************
	function: mpu_get_accdivider
*******************************************************************************
	Get accelerometer sample rate divider

	Parameters:
		-
	Return
		divider
******************************************************************************/
unsigned int mpu_get_accdivider()
{
	unsigned char pb = mpu_selectbank(2);
	unsigned int m = mpu_readreg_wait_poll_block(ICM_R_ACCEL_SMPLRT_DIV_1);
	unsigned int l = mpu_readreg_wait_poll_block(ICM_R_ACCEL_SMPLRT_DIV_2);
	mpu_selectbank(pb);

	unsigned d = (m<<8)|l;
	return d;
}
/******************************************************************************
	function: mpu_set_accdivider
*******************************************************************************
	Set accelerometer sample rate divider.

	ODR = 1125/(1+divider)

	MPU: ICM20948

	Parameters:
		divider		-		12 bit divider.
	Return
		-
******************************************************************************/
void mpu_set_accdivider(unsigned divider)
{
	unsigned char pb = mpu_selectbank(2);
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_SMPLRT_DIV_1,divider>>8);
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_SMPLRT_DIV_2,divider&0xff);
	mpu_selectbank(pb);
}
/******************************************************************************
	function: mpu_set_gyrodivider
*******************************************************************************
	Set gyroscope sample rate divider

	ODR = 1125/(1+divider)

	MPU: ICM20948

	Parameters:
		divider		-		8 bit divider.
	Return
		-
******************************************************************************/
void mpu_set_gyrodivider(unsigned char divider)
{
	unsigned char pb = mpu_selectbank(2);
	mpu_writereg_wait_poll_block(ICM_R_GYRO_SMPLRT_DIV,divider);
	mpu_selectbank(pb);
}


/******************************************************************************
	function: mpu_temp_enable
*******************************************************************************
	Toggles the TEPM_DIS bit in PWR_MGMT_1; i.e. enables or disables the
	temperature conversion. 
	Does not affect any other setting.

	Updated: ICM20948

******************************************************************************/
void mpu_temp_enable(unsigned char enable)
{
	// Assume regbank 0
	unsigned char pwr1 = mpu_readreg_wait_poll_block(ICM_R_PWR_MGMT_1);
	if(enable)
		pwr1&=0b11110111;
	else
		pwr1|=0b00001000;
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,pwr1);
}



/******************************************************************************
	function: _mpu_wakefromsleep
*******************************************************************************
	This function wakes up the MPU from sleep mode and waits until ready.

	This function modifies the clock selection, enables temperature, and
	disables low-power mode.

	Updated for ICM20948

	Change in behaviour:
******************************************************************************/
void _mpu_wakefromsleep(void)
{
	// Clear SLEEP, clear LP_EN, enable temperature, internal 20MHz oscillator
	//fprintf(file_pri,"cur bank: %02Xh\n",mpu_readreg_wait_poll_block(ICM_R_REG_BANK_SEL));
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,0x00);
	// Wait to wake up
	_delay_ms(1);
}



/******************************************************************************
	function: mpu_clksel
*******************************************************************************
	Changes the clksel bits in PWR_MGMT_1. Does not affect any other setting.
	
	Parameters:
		clk	-	Value between 0 and 7 inclusive corresponding to the clock source.
				
				0 Internal 20MHz oscillator
				
				1 Auto selects the best available clock source (Gyro X) – PLL if ready, else use the Internal oscillator. 
				
				2 Auto selects the best available clock source – PLL if ready, else use the Internal oscillator
				
				3 Auto selects the best available clock source – PLL if ready, else use the Internal oscillator
				
				4 Auto selects the best available clock source – PLL if ready, else use the Internal oscillator
				
				5 Auto selects the best available clock source – PLL if ready, else use the Internal oscillator
				
				6 Internal 20MHz oscillator

				7 Stops the clock and keeps timing generator in reset
******************************************************************************/
void mpu_clksel(unsigned char clk)
{
	unsigned char pwr1;

	clk&=0b111;
	pwr1 = mpu_readreg_wait_poll_block(ICM_R_PWR_MGMT_1);
	pwr1&=0b11111000;
	pwr1|=clk;
	mpu_writereg_wait_poll_block(ICM_R_PWR_MGMT_1,pwr1);

}


signed short mpu_convtemp(signed short t)
{
	// Return the temperature in *100;
	signed long int t2;
	signed long int ct;
	
	//ct = (t-0)/333.87 + 21;
	
	t2 = t;
	t2 *=10000l;
	ct = t2/33387 + 2100;
	//ct /= 100;
	
	return (signed short)ct;
}
/******************************************************************************
	function: mpu_set_interruptpin
*******************************************************************************
	Set the MPU interrupt style.

	Updated for: ICM-20948

	Parameters:
	p	-	| INT1_ACTL | INT1_OPEN | INT1_LATCH_INT_EN | INT_ANYRD_2CLEAR | ACTL_FSYNC | FSYNC_INT_MODE_EN | BYPASS_EN | - |

	Returns:
		-
*******************************************************************************/
void mpu_set_interruptpin(unsigned char p)
{
	mpu_writereg_wait_poll_block(ICM_R_INTERRUPTPIN,p);
}




/*void mpu_testsleep()
{
	unsigned char r;
	while(1)
	{
		mpu_writereg(MPU_R_PWR_MGMT_1,0b01001000);							// Sleep, no cycle, no gyro stby, disable temp, internal osc
		printf("Sleep\n");
		_delay_ms(1);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		_delay_ms(1);
		mpu_writereg(MPU_R_ACCELCONFIG2,1);
		_delay_ms(1);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		_delay_ms(1);
		mpu_writereg(MPU_R_ACCELCONFIG2,2);
		_delay_ms(1);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		_delay_ms(1);
		mpu_writereg(MPU_R_ACCELCONFIG2,3);
		_delay_ms(1);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		printf("Wakeup\n");
		_delay_ms(500);
		
		mpu_writereg(MPU_R_PWR_MGMT_1,0b00001000);							// Sleep, no cycle, no gyro stby, disable temp, internal osc
		_delay_us(50);
		mpu_writereg(MPU_R_ACCELCONFIG2,4);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		mpu_writereg(MPU_R_ACCELCONFIG2,5);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		mpu_writereg(MPU_R_ACCELCONFIG2,6);
		r = mpu_readreg(MPU_R_ACCELCONFIG2);
		printf("Config 2: %d\n",r);
		//mpu_writereg(MPU_R_ACCELCONFIG2,1);		
		_delay_ms(2000);
	}
	
}*/


/******************************************************************************
	function: _mpu_defaultdlpon
*******************************************************************************	
	Enables the MPU in a default mode. Typically it:
	- activates the accelerometer and gyroscope at 225Hz with gyro LPF at 119.5Hz and accel LPF at 111Hz
	- selects the PLL oscillator
	
	This mode allows to communicate with the magnetometer via the internal I2C
	interface.
	
	MPU: ICM20948

	Note: do not use in user code.	
*******************************************************************************/
void _mpu_defaultdlpon(void)
{
	// 225Hz Gyro (BW=119.5Hz) + Acc (BW=111Hz)
	// { MPU_MODE_GYR,        1,         0, ICM_GYRO_LPF_120,    1, ICM_ACC_LPF_111,      4,             0,     0,     0,		0,			225},
	mpu_mode_accgyro(1,ICM_GYRO_LPF_120, 1,ICM_ACC_LPF_111,4);
}
/******************************************************************************
	function: _mpu_acquirecalib
*******************************************************************************	
	Acquires calibration data in the FIFO.
	
	Assumes the acceleration and gyroscope sensitivity are 2G and 250dps and
	that the gyro bias registers are 0.
	
	Caution: scale is changed

*******************************************************************************/
void _mpu_acquirecalib(unsigned char clearbias)
{
	// ICM20948 codepath


	// Clear the gyro bias
	if(clearbias)
		mpu_setgyrobias(0,0,0);

	// The gyro takes about 40 milliseconds to stabilise from waking up; wait 80ms.
	_delay_ms(80);

	// fifo channel select: SLV3 SLV2 SLV1 SLV0 ACC GZ GY GX TMP
	mpu_fifoenable(0b11110,1,1);					// Set FIFO for accel+gyro, enable FIFO, assert FIFO reset
	mpu_fifoenable(0b11110,1,0);					// Set FIFO for accel+gyro, enable FIFO, deassert FIFO - start logging
	// In Acc+Gyro: output is 12 bytes per samples. In Gyro: output is 6 bytes per samples.
	// FIFO is 512 bytes.
	// Sample rate is 562.5Hz -> floor(floor(512/12)/562.5) = 74.6ms. Room for 1 sample spare: floor(floor(512/12-1)/562.5) = 72.8ms -> 72ms
	// 72ms: expected samples should be: 40 samples = 480 bytes. Experimentally: 504 bytes (42 samples)
	// 71ms: experimentally 492 or 504 bytes
	// 70ms: experimentally always 492 bytes
	_delay_ms(70);			// Exp. 492 bytes = 41 samples
	//_delay_ms(130);
	// Keep FIFO enabled but stop logging
	mpu_fifoenable(0x00,1,0);

}
void mpu_calibrate(void)
{
	unsigned short n;
	unsigned char numtries=5;
	signed short ax,ay,az,gx,gy,gz;
	long acc_mean[3];
	long acc_std[3];
	
	long acc_totstd[numtries];
	long gyro_bias_best[numtries][3];

	short maxns = 42;		// Max number of samples in the sharedbuffer (512 bytes/12)

	// Change the motion sensing modes and scale
	unsigned char curautoread;
	unsigned char curmode = mpu_get_motionmode(&curautoread);	// Save current mode
	mpu_config_motionmode(MPU_MODE_562HZ_ACCGYRO,0);			// Enable high speed
	// Sensitivity should be 250dps and 2G
	unsigned char curaccscale = mpu_getaccscale();
	unsigned char curgyroscale = mpu_getgyroscale();
	mpu_setaccscale(MPU_ACC_SCALE_2);
	mpu_setgyroscale(MPU_GYR_SCALE_250);


	fprintf(file_pri,"MPU calibration:\n");
	
	// Repeat calibration multiple times
	for(unsigned char tries=0;tries<numtries;tries++)
	{
		// Acquire calibration clearing the bias register
		_mpu_acquirecalib(1);
		// Transfer the data into a shared buffer in order to compute standard deviation
		signed short *buffer = (signed short*)sharedbuffer;
		n = mpu_getfifocnt();
		unsigned short ns = n/12;
		fprintf(file_pri,"\tCalibration #%d: %02d samples. ",tries,n);

		// ICM
		for(unsigned short i=0;i<ns;i++)
		{
			// Order of samples in ICM20948 FIFO: first ACC, then GYRO, same as MPU9250
			mpu_fiforeadshort(&ax,1);
			mpu_fiforeadshort(&ay,1);
			mpu_fiforeadshort(&az,1);
			mpu_fiforeadshort(&gx,1);
			mpu_fiforeadshort(&gy,1);
			mpu_fiforeadshort(&gz,1);
			if(i<maxns)		// Only store the samples for which space in shared buffer, as FIFO could have acquired more
			{
				buffer[i*6+0] = ax;
				buffer[i*6+1] = ay;
				buffer[i*6+2] = az;
				buffer[i*6+3] = gx;
				buffer[i*6+4] = gy;
				buffer[i*6+5] = gz;
			}
		}
		// Clamp ns to maxns
		if(ns>maxns)
			ns = maxns;
		
#if 0
		// Print in user friendly format
		for(int i=0;i<ns;i++)
		{
			fprintf(file_pri,"%d: %d %d %d   %d %d %d\n",i,buffer[i*6+0],buffer[i*6+1],buffer[i*6+2],buffer[i*6+3],buffer[i*6+4],buffer[i*6+5]);
		}
#endif

		// Init variables for statistics
		for(unsigned char axis=0;axis<3;axis++)
		{
			acc_mean[axis]=0;
			acc_std[axis]=0;
			gyro_bias_best[tries][axis]=0;
		}
		// Compute mean
		for(unsigned short i=0;i<ns;i++)
		{
			for(unsigned char axis=0;axis<3;axis++)
			{
				acc_mean[axis]+=buffer[i*6+axis];
				gyro_bias_best[tries][axis]+=buffer[i*6+3+axis];
			}		
		}
		for(unsigned char axis=0;axis<3;axis++)
		{
			acc_mean[axis]/=ns;
			gyro_bias_best[tries][axis]/=ns;
		}
		// Compute standard deviation
		for(unsigned short i=0;i<ns;i++)
		{
			for(unsigned char axis=0;axis<3;axis++)
				acc_std[axis]+=(buffer[i*6+axis]-acc_mean[axis])*(buffer[i*6+axis]-acc_mean[axis]);
		}
		for(unsigned char axis=0;axis<3;axis++)
			acc_std[axis]=sqrt(acc_std[axis]/(ns-1));
			
		//system_led_set(0b011); _delay_ms(800);
		//fprintf(file_pri,"Mean(Acc): %ld %ld %ld "),acc_mean[0],acc_mean[1],acc_mean[2]);
		//fprintf(file_pri,"Std(Acc): %ld %ld %ld "),acc_std[0],acc_std[1],acc_std[2]);
		//fprintf(file_pri,"Mean(Gyro): %ld %ld %ld "),gyro_bias_best[tries][0],gyro_bias_best[tries][1],gyro_bias_best[tries][2]);
		// Factor 4 comes from the bias scale defined at full scale 2, while calib data acquired at full scale 0.
		for(unsigned char axis=0;axis<3;axis++)
			gyro_bias_best[tries][axis]=-gyro_bias_best[tries][axis]/4;
		
		//system_led_set(0b010); _delay_ms(800);
		fprintf(file_pri,"Bias: %3ld %3ld %3ld ",gyro_bias_best[tries][0],gyro_bias_best[tries][1],gyro_bias_best[tries][2]);
		
		long totstd = acc_std[0]+acc_std[1]+acc_std[2];
		acc_totstd[tries]=totstd;
		fprintf(file_pri,"(std: %lu)\n",totstd);
	}
	// Find best
	long beststd = acc_totstd[0];
	unsigned char best=0;
	for(unsigned char tries=1;tries<numtries;tries++)
	{
		if(acc_totstd[tries]<beststd)
		{
			beststd=acc_totstd[tries];
			best=tries;
		}
	}
	mpu_setgyrobias(gyro_bias_best[best][0],gyro_bias_best[best][1],gyro_bias_best[best][2]);
	fprintf(file_pri,"\tCalibrated with #%d: %ld %ld %ld\n",best,gyro_bias_best[best][0],gyro_bias_best[best][1],gyro_bias_best[best][2]);
	if(beststd>200)
	{
		fprintf(file_pri," **TOO MUCH MOVEMENT-RISK OF MISCALIBRATION**\n");
	}

	// Restore previous scale
	mpu_setaccscale(curaccscale);
	mpu_setgyroscale(curgyroscale);
	mpu_config_motionmode(curmode,curautoread);
}
/******************************************************************************
	function: mpu_selftest
*******************************************************************************
	Accelerometer and gyro self test

	Manual section 4.13:
	The self-test response is defined as follows:
	SELF-TEST RESPONSE = SENSOR OUTPUT WITH SELF-TEST ENABLED – SENSOR OUTPUT WITHOUT SELF-TEST ENABLED

*******************************************************************************/
void mpu_selftest()
{
	mpu_gyroselftest();
	mpu_accselftest();
}
void mpu_accselftest()
{
	unsigned char asx,asy,asz;
	fprintf(file_pri,"Accel self-test\n");
	unsigned char pb = mpu_selectbank(1);
	asx=mpu_readreg_wait_poll_block(ICM_R_SELF_TEST_X_ACCEL);
	asy=mpu_readreg_wait_poll_block(ICM_R_SELF_TEST_Y_ACCEL);
	asz=mpu_readreg_wait_poll_block(ICM_R_SELF_TEST_Z_ACCEL);
	mpu_selectbank(0);

	fprintf(file_pri,"\tAccel manufacturing self-test: %d %d %d\n",asx,asy,asz);
	// Activate a motion mode
	unsigned char curautoread;
	unsigned char curmode = mpu_get_motionmode(&curautoread);

	mpu_config_motionmode(MPU_MODE_562HZ_ACCGYRO,0);
	_delay_ms(50);

	int accn[3]={0,0,0},accall[3],accst[3]={0,0,0};
	int navg=100;
	signed short ax,ay,az,gx,gy,gz,t;
	signed short *aall[3]={&ax,&ay,&az};

	// Get default values
	for(int i=0;i<navg;i++)
	{
		mpu_get_agt(&ax,&ay,&az,&gx,&gy,&gz,&t);
		for(int i=0;i<3;i++)
			accn[i]+=*aall[i];
		HAL_Delay(2);
	}
	for(int i=0;i<3;i++)
		accn[i]/=navg;
	for(int a=0;a<3;a++)
	{
		// Clear the data
		for(int i=0;i<3;i++)
			accall[i]=0;

		fprintf(file_pri,"\tSelf-testing accel %c... ",'X'+a);

		// Activate self test
		mpu_selectbank(2);
		unsigned char ac2 = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG_2);
		ac2&=0b00000011;
		ac2|=(0b10000>>a);				// Start testing X
		mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG_2,ac2);
		mpu_selectbank(0);

		// Wait for settling
		_delay_ms(50);

		// Read data
		for(int avg=0;avg<navg;avg++)
		{
			mpu_get_agt(&ax,&ay,&az,&gx,&gy,&gz,&t);
			//fprintf(file_pri,"gall 0 1 2 %d %d %d. gx gy gz: %d %d %d\n",*gall[0],*gall[1],*gall[2],gx,gy,gz);
			for(int i=0;i<3;i++)
				accall[i] += *aall[i];
			_delay_ms(2);
		}

		for(int i=0;i<3;i++)
			accall[i]/=navg;

		accst[a]+=accall[a];		// Keep self-test reading

		fprintf(file_pri,"%d %d %d\n",accall[0],accall[1],accall[2]);
	}
	// Stop self test
	mpu_selectbank(2);
	unsigned char ac2 = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG_2);
	ac2&=0b00000011;					// Deactivate self test
	mpu_writereg_wait_poll_block(ICM_R_ACCEL_CONFIG_2,ac2);

	fprintf(file_pri,"\tAccel out w/o self test: %d %d %d\n",accn[0],accn[1],accn[2]);
	fprintf(file_pri,"\tAccel out w/ self test: %d %d %d\n",accst[0],accst[1],accst[2]);


	mpu_selectbank(pb);
	mpu_config_motionmode(curmode,curautoread);

}
void mpu_gyroselftest()
{
	unsigned char gsx,gsy,gsz;
	fprintf(file_pri,"Gyro self-test\n");
	unsigned char pb = mpu_selectbank(1);
	gsx=mpu_readreg_wait_poll_block(ICM_R_SELF_TEST_X_GYRO);
	gsy=mpu_readreg_wait_poll_block(ICM_R_SELF_TEST_Y_GYRO);
	gsz=mpu_readreg_wait_poll_block(ICM_R_SELF_TEST_Z_GYRO);
	mpu_selectbank(0);

	fprintf(file_pri,"\tGyro manufacturing self-test: %d %d %d\n",gsx,gsy,gsz);

	// Activate a motion mode
	unsigned char curautoread;
	unsigned char curmode = mpu_get_motionmode(&curautoread);

	mpu_config_motionmode(MPU_MODE_562HZ_ACCGYRO,0);
	_delay_ms(50);

	int gyron[3]={0,0,0},gyroall[3],gyrost[3]={0,0,0};
	int navg=100;
	signed short ax,ay,az,gx,gy,gz,t;
	signed short *gall[3]={&gx,&gy,&gz};

	// Get default values
	for(int i=0;i<navg;i++)
	{
		mpu_get_agt(&ax,&ay,&az,&gx,&gy,&gz,&t);
		for(int i=0;i<3;i++)
			gyron[i]+=*gall[i];
		HAL_Delay(2);
	}
	for(int i=0;i<3;i++)
		gyron[i]/=navg;
	for(int a=0;a<3;a++)
	{
		// Clear the data
		for(int i=0;i<3;i++)
			gyroall[i]=0;

		fprintf(file_pri,"\tSelf testing gyro %c... ",'X'+a);

		// Activate self test
		mpu_selectbank(2);
		unsigned char gc2 = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_2);
		gc2&=0b00000111;
		gc2|=(0b100000>>a);				// Start testing X
		mpu_writereg_wait_poll_block(ICM_R_GYRO_CONFIG_2,gc2);
		mpu_selectbank(0);

		// Wait for settling
		_delay_ms(50);

		// Read data
		for(int avg=0;avg<navg;avg++)
		{
			mpu_get_agt(&ax,&ay,&az,&gx,&gy,&gz,&t);
			//fprintf(file_pri,"gall 0 1 2 %d %d %d. gx gy gz: %d %d %d\n",*gall[0],*gall[1],*gall[2],gx,gy,gz);
			for(int i=0;i<3;i++)
				gyroall[i] += *gall[i];
			_delay_ms(2);
		}

		for(int i=0;i<3;i++)
			gyroall[i]/=navg;

		gyrost[a]+=gyroall[a];		// Keep self-test reading

		fprintf(file_pri,"%d %d %d\n",gyroall[0],gyroall[1],gyroall[2]);
	}
	// Stop self test
	mpu_selectbank(2);
	unsigned char gc2 = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_2);
	gc2&=0b00000111;					// Deactivate self-test
	mpu_writereg_wait_poll_block(ICM_R_GYRO_CONFIG_2,gc2);

	fprintf(file_pri,"\tGyro out w/o self test: %d %d %d\n",gyron[0],gyron[1],gyron[2]);
	fprintf(file_pri,"\tGyro out w/ self test: %d %d %d\n",gyrost[0],gyrost[1],gyrost[2]);

	mpu_selectbank(pb);
	mpu_config_motionmode(curmode,curautoread);
}

/******************************************************************************
*******************************************************************************
MAGNETOMETER   MAGNETOMETER   MAGNETOMETER   MAGNETOMETER   MAGNETOMETER   MAGN
*******************************************************************************
*******************************************************************************
Slave 0 is used for shadowing.
Slave 4 is used for individual register access with mpu_mag_readreg and mpu_mag_writereg.
Slave 4 is only temporarily enabled during mpu_mag_readreg and mpu_mag_writereg calls, and 
then deactivated.

In all cases the magnetometer functions should only be called when the motion 
processor is active.
*******************************************************************************/

/******************************************************************************
	function: mpu_mag_readreg
*******************************************************************************	
	Reads the content of a magnotemeter register. 
	
	This function blocks until the data is read or a timeout occurs.
		
	In order for this function to work, the MPU I2C interface must be active
	(mpu_mag_interfaceenable) and the MPU must be configured to sample 
	acceleration or gyroscope data (not in low power mode).
	If not, the I2C interface is not active and the function will hang.
	
	Parameters:
		reg	-	Magnetometer register to read
	Returns:
		register 
*******************************************************************************/
unsigned char mpu_mag_readreg(unsigned char reg)
{
#if 0
	// MPU 9250 code path
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_ADDR,MAG_ADDRESS|0x80);			// Magnetometer read address
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_REG,reg);						// Register to read
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0b10000000);				// Enable slave 4
	unsigned long t1 = timer_ms_get();
	while( !(mpu_readreg_wait_poll_block(MPU_R_I2C_MST_STATUS)&0x40) )			// Wait for I2C_SLV4_DONE bit
	{
		if(timer_ms_get()-t1>250)
		{
			fprintf(file_pri,"mag rd err\n");
			mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0b00000000);		// Disable slave 4
			return 0xff;
		}
	}
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0b00000000);				// Disable slave 4
	return mpu_readreg_wait_poll_block(MPU_R_I2C_SLV4_DI);						// Return input data
#endif
	// ICM20948 code path
	unsigned char pb = mpu_selectbank(3);						// Bank 3: I2C control
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_ADDR,MAG_ADDRESS|0x80);			// Magnetometer read address
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_REG,reg);						// Register to read
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0b10000000);				// Enable slave 4
	mpu_selectbank(0);
	unsigned long t1 = timer_ms_get();
	while( !(mpu_readreg_wait_poll_block(ICM_R_I2C_MST_STATUS)&0x40) )			// Wait for I2C_SLV4_DONE bit
	{
		if(timer_ms_get()-t1>250)
		{
			fprintf(file_pri,"mag rd err\n");
			mpu_selectbank(3);
			mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0b00000000);		// Disable slave 4
			mpu_selectbank(pb);
			return 0xff;
		}
	}
	mpu_selectbank(3);
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0b00000000);				// Disable slave 4

	unsigned char data = mpu_readreg_wait_poll_block(ICM_R_I2C_SLV4_DI);	// Return input data
	mpu_selectbank(pb);
	return data;


}
/******************************************************************************
	function: mpu_mag_writereg
*******************************************************************************	
	Writes the content of a magnotemeter register. 
	
	This function blocks until the data is written or a timeout occurs.
	
	In order for this function to work, the MPU I2C interface must be active
	(mpu_mag_interfaceenable) and the MPU must be configured to sample 
	acceleration or gyroscope data (not in low power mode).
	If not, the I2C interface is not active and the function will hang.
	
	MPU: ICM20948

	Parameters:
		reg	-	Magnetometer register to write to
		val	-	Value to write in register
*******************************************************************************/
void mpu_mag_writereg(unsigned char reg,unsigned char val)
{	
#if 0
	//MPU9250 codepath
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_ADDR,MAG_ADDRESS);				// Magnetometer write address
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_REG,reg);						// Register to write
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_DO,val);						// Data to write
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0b10000000);				// Enable slave 4
	unsigned long t1 = timer_ms_get();
	while( !(mpu_readreg_wait_poll_block(MPU_R_I2C_MST_STATUS)&0x40))			// Wait for I2C_SLV4_DONE bit
	{
		if(timer_ms_get()-t1>250)
		{
			fprintf(file_pri,"mag rd err\n");
			break;
		}
	}
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0b00000000);				// Disable slave 4
#endif
	// ICM20948 codepath
	unsigned char pb = mpu_selectbank(3);						// Bank 3: I2C control
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_ADDR,MAG_ADDRESS);				// Magnetometer write address
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_REG,reg);						// Register to write
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_DO,val);						// Data to write
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0b10000000);				// Enable slave 4
	mpu_selectbank(0);
	unsigned long t1 = timer_ms_get();
	while( !(mpu_readreg_wait_poll_block(ICM_R_I2C_MST_STATUS)&0x40))			// Wait for I2C_SLV4_DONE bit
	{
		if(timer_ms_get()-t1>250)
		{
			fprintf(file_pri,"mag wr err\n");
			break;
		}
	}
	mpu_selectbank(3);
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0b00000000);		// Disable slave 4
	mpu_selectbank(pb);
}

/******************************************************************************
	function: mpu_mag_isok
*******************************************************************************
	Checks the magnetometer WHOAMI register

	Must only be called if the magnetic interface and DLP is enabled.

	Parameters:
		-
	Returns
		1		-		Magnetometer present
		0		-		Magnetometer error
******************************************************************************/
unsigned char mpu_mag_isok(void)
{
	unsigned char r0 = mpu_mag_readreg(0);
	unsigned char r1 = mpu_mag_readreg(1);
	fprintf(file_pri,"%02X %02X ",r0,r1);
	if( (r0==0x48) && (r1==0x09) )		// AsahiKasei AK09916 whoami
		return 1;
	return 0;
}


/******************************************************************************
	function: _mpu_mag_interfaceenable
*******************************************************************************	
	Enables the I2C communication interface between the magnetometer and the 
	I2C master within the MPU.
	
	In addition the MPU must be configured to sample acceleration or gyroscope data 
	(not in low power mode).
	
	Parameters:
		en	-	Enable (1) or disable (0) the interface
	
*******************************************************************************/
void _mpu_mag_interfaceenable(unsigned char en)
{
#if 0
	// MPU9250 code path
	// Ensures the slaves are disabled
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0);
	mpu_writereg_wait_poll_block(MPU_R_I2C_SLV0_CTRL,0);
	mpu_writereg_wait_poll_block(MPU_R_I2C_MST_DELAY_CTRL,0b10000000);		// Set DELAY_ES_SHADOW, do not enable periodic slave access (i.e. I2C enabled but not active)
	if(en)
	{
		unsigned char usr = mpu_readreg_wait_poll_block(MPU_R_USR_CTRL);
		mpu_writereg_wait_poll_block(MPU_R_USR_CTRL,usr|0b00100000);		// Set I2C_MST_EN			
		mpu_writereg_wait_poll_block(MPU_R_I2C_MST_CTRL,0b11000000);		// Set MULT_MST_EN, WAIT_FOR_ES. TODO: check if needed.
	}
	else
	{
		// Deactivates periodic slave access		
		mpu_writereg_wait_poll_block(MPU_R_I2C_MST_CTRL,0b00000000);		// Clear MULT_MST_EN, clear WAIT_FOR_ES. I2C 348KHz. TODO: check if needed.		(Default)
		//mpu_writereg(MPU_R_I2C_MST_CTRL,0b00001001);		// Clear MULT_MST_EN, clear WAIT_FOR_ES. I2C 500KHz. TODO: check if needed.		(Increasing the I2C clock does not seem to improve/change anything in the behaviour)
		unsigned char usr = mpu_readreg_wait_poll_block(MPU_R_USR_CTRL);
		mpu_writereg_wait_poll_block(MPU_R_USR_CTRL,usr&0b11011111);		// Clears I2C_MST_EN		
	}
#endif
	// ICM20948 code path
	// Ensures the slaves are disabled
	unsigned char pb = mpu_selectbank(3);						// Select bank 3, I2C control
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0);
	mpu_writereg_wait_poll_block(ICM_R_I2C_SLV0_CTRL,0);
	mpu_writereg_wait_poll_block(ICM_R_I2C_MST_DELAY_CTRL,0b10000000);			// Set DELAY_ES_SHADOW, do not enable periodic slave access (i.e. I2C enabled but not active)

	if(en)
	{
		mpu_writereg_wait_poll_block(ICM_R_I2C_MST_CTRL,0b10000000);		// ICM20948: Set MULT_MST_EN, clock frequency 370.29KHz
		//mpu_writereg(MPU_R_I2C_MST_CTRL,0b11000000);		// MPU9250: Set MULT_MST_EN, WAIT_FOR_ES. TODO: check if needed.
		mpu_selectbank(0);
		unsigned char usr = mpu_readreg_wait_poll_block(ICM_R_USR_CTRL);
		mpu_writereg_wait_poll_block(ICM_R_USR_CTRL,usr|0b00100000);		// Set I2C_MST_EN
	}
	else
	{
		// Deactivates periodic slave access
		//mpu_writereg(MPU_R_I2C_MST_CTRL,0b00000000);		// MPU9250: Clear MULT_MST_EN, clear WAIT_FOR_ES. I2C 348KHz. TODO: check if needed.		(Default)
		mpu_writereg_wait_poll_block(ICM_R_I2C_MST_CTRL,0b00000000);		// ICM20948: Clear MULT_MST_EN. clock frequency 370.29KHz
		mpu_selectbank(0);
		unsigned char usr = mpu_readreg_wait_poll_block(ICM_R_USR_CTRL);
		mpu_writereg_wait_poll_block(ICM_R_USR_CTRL,usr&0b11011111);		// Clears I2C_MST_EN
	}
	mpu_selectbank(pb);										// Restore initial bank
}

/******************************************************************************
	function: _mpu_mag_mode
*******************************************************************************	
	Enables or disable the conversion of the magnetometer sensors and.
	enables the shadowing of the magnetometer registers.

	This function must only be called if the communication interface is 
	enabled (mpu_mag_interfaceenable) and if the motion processor is active.
	Otherwise, timeout occur.
	
	Note: do not use in user code.

	MPU: ICM20948
		
	Parameters:
		en		-	0: sleep mode. 1: 8Hz conversion. 2: 100Hz conversion.
		magdiv	-	Shadowing frequency divider; shadows register at ODR/(1+magdiv)
	
*******************************************************************************/
void _mpu_mag_mode(unsigned char mode,unsigned char magdiv)
{
	// Always enable the interface, even if the mode is to sleep, as 
	// this could be called while the magnetometer is already sleeping
	_mpu_mag_interfaceenable(1);					
	switch(mode)
	{
		case 0:
			//mpu_mag_writereg(0x0a,0b00010000);	// MPU9250: Power down, 16 bit
			mpu_mag_writereg(0x31,0x00);			// ICM20948: Power down
			_mpu_mag_regshadow(0,0,0,0);			// Stop shadowing
			_mpu_mag_interfaceenable(0);			// Stop I2C interface
			break;
		case 1:
			//mpu_mag_writereg(0x0a,0b00010010);	// MPU9250: Continuous mode 1 (8Hz conversion), 16 bit
			mpu_mag_writereg(0x31,0b00000010);		// ICM20948: Continuous mode 1 (10Hz conversion)
			//_mpu_mag_regshadow(1,magdiv,3,7);		// MPU9250: Start shadowing
			_mpu_mag_regshadow(1,magdiv,0x11,8);	// ICM20948: Start shadowing
			break;
		case 2:
		default:
			//mpu_mag_writereg(0x0a,0b00010110);	// MPU9250: Continuous mode 2 (100Hz conversion), 16 bit
			mpu_mag_writereg(0x31,0b00001000);		// ICM20948: Continuous mode 4 (100Hz conversion)
			//_mpu_mag_regshadow(1,magdiv,3,7);		// MPU9250: Start shadowing
			_mpu_mag_regshadow(1,magdiv,0x11,8);	// ICM20948: Start shadowing
			break;
	}
}



/******************************************************************************
	function: _mpu_mag_regshadow
*******************************************************************************	
	Setups the MPU to shadow the registers for the magnetometer into the MPU
	external sensor memory (reg 73d to 96d).
	
	In order for this function to work, the MPU I2C interface must be active
	(mpu_mag_interfaceenable) and the MPU must be configured to sample 
	acceleration or gyroscope data (not in low power mode).
	
	In this mode, the MPU reads at periodic interval the magnetometer using the 
	on-chip I2C interface. This allows the application to transparently read the 
	magnetometer data using the mpu_readreg function, instead of the mpu_mag_readreg
	function. 
	
	The magnetometer must be configured separately for continuous conversion mode.
	
	This only works when the MPU is not in low-power acceleration mode.
	
	*Magnetometer read rate*
	
	The magnetometer is read at the output data rate ODR/(1+dly), where dly is 
	configurable. This is independent of the magnetometer ADC which is 8Hz or 100 Hz.
	
	When the MPU is in some modes with high internal ODR (e.g. 8KHz with the 
	gyro bandwidth is 250Hz in the MPU9250) then the dly parameter should be set to higher values,
	otherwise the internal I2C interface to the magnetometer will block the acquisition
	of the gyro and acceleration data, leading to an effective sample rate lower than desired.
	
	Note that the ODR is the sample rate (1KHz) divided by the acc/gyro sample rate divider (except
	in the gyro 250BW mode, where the ODR is 8KHz).
	
	MPU: ICM20948

	Parameters:
		enable		-	Enable (1) or disable (0) register shadowing
		dly			-	Read magnetometer at frequency ODR/(1+dly). dly e [0;31]
		regstart	-	First magnetometer register to mirror
		numreg		-	Number of magnetometer registers to mirror. numreg e [0;15]
	Returns:
		register 
*******************************************************************************/
void _mpu_mag_regshadow(unsigned char enable,unsigned char dly,unsigned char regstart,unsigned char numreg)
{
#if 0
	if(dly>31) dly=31;
	if(numreg>15) numreg=15;

	if(enable)
	{
		mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,dly);					// R52: slave 4 disabled (it is always disabled anyways); set I2C_MST_DLY	
		
		/*
		// 
		unsigned char usr = mpu_readreg(MPU_R_USR_CTRL);		// R106 USER_CTRL read current status
		usr = usr&0b11011111;									// Keep other settings
		usr = usr|0b00110000;									// Enable MST_I2C, disable I2C_IF
		mpu_writereg(MPU_R_USR_CTRL,usr);						// R106 USER_CTRL update*/
		
		//mpu_writereg(MPU_R_I2C_MST_CTRL,0b11000000);			// R36: enable multi-master (needed?), wait_for_es, set I2C clock 	// already setup when interface is enabled
		
		mpu_writereg_wait_poll_block(MPU_R_I2C_SLV0_ADDR,MAG_ADDRESS|0x80);		// SLV0 read mode
		mpu_writereg_wait_poll_block(MPU_R_I2C_SLV0_REG,regstart);				// SLV0 start register
		mpu_writereg_wait_poll_block(MPU_R_I2C_MST_DELAY_CTRL,0b10000001);		// R 103 DELAY_ES_SHADOW | I2C_SLV0_DLY_EN
		mpu_writereg_wait_poll_block(MPU_R_I2C_SLV0_CTRL,0x80 | numreg);		// R 39: SLV0_EN | number of registers to copy
	}
	else
	{
		mpu_writereg_wait_poll_block(MPU_R_I2C_SLV4_CTRL,0);					
		mpu_writereg_wait_poll_block(MPU_R_I2C_SLV0_CTRL,0);	
//		_delay_ms(10);
		mpu_writereg_wait_poll_block(MPU_R_I2C_MST_DELAY_CTRL,0b10000000);		
//		_delay_ms(10);
	}
#endif
	// ICM20948 code path
	if(dly>31) dly=31;
	if(numreg>15) numreg=15;


	unsigned char pb = mpu_selectbank(3);						// Bank 3 for I2C config
	if(enable)
	{

		mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,dly);					// Slave 4 disabled (it should be always disabled anyways), set I2C_SLV4_DLY (used by all slaves)

		mpu_writereg_wait_poll_block(ICM_R_I2C_SLV0_ADDR,MAG_ADDRESS|0x80);		// SLV0 read mode
		mpu_writereg_wait_poll_block(ICM_R_I2C_SLV0_REG,regstart);				// SLV0 start register
		mpu_writereg_wait_poll_block(ICM_R_I2C_MST_DELAY_CTRL,0b10000001);		// DELAY_ES_SHADOW | I2C_SLV0_DLY_EN
		mpu_writereg_wait_poll_block(ICM_R_I2C_SLV0_CTRL,0x80 | numreg);		// R 39: SLV0_EN | number of registers to copy
	}
	else
	{
		mpu_writereg_wait_poll_block(ICM_R_I2C_SLV4_CTRL,0);
		mpu_writereg_wait_poll_block(ICM_R_I2C_SLV0_CTRL,0);
//		_delay_ms(10);
		mpu_writereg_wait_poll_block(ICM_R_I2C_MST_DELAY_CTRL,0b10000000);
//		_delay_ms(10);
	}
	mpu_selectbank(pb);						// Restore previous bank
}


/******************************************************************************
	function: _mpu_mag_readasa
*******************************************************************************	
	Read the magnetometer axis sensitivity adjustment value.
	Assumes MPU is in a mode allowing communication with the magnetometer.	
*******************************************************************************/
#if 0
// MPU9250 codepath
// ASA does not exist in ICM20948
void _mpu_mag_readasa(void)
{
	for(unsigned i=0;i<3;i++)
		_mpu_mag_asa[i] = mpu_mag_readreg(0x10+i);
	/*
	// Datasheet indicates to enable "fuse mode", however readout of asa is identical
	printf("Mag asa: %d %d %d\n",_mpu_mag_asa[0],_mpu_mag_asa[1],_mpu_mag_asa[2]);
	// enable fuse mode
	mpu_mag_writereg(0x0a,0b00011111);		// Fuse mode, 16 bit
	for(unsigned i=0;i<3;i++)
		m[i] = mpu_mag_readreg(0x10+i);
	// power down
	mpu_mag_writereg(0x0a,0b00010000);		// Power down, 16 bit
	printf("Mag asa with fuse mode: %d %d %d\n",m[0],m[1],m[2]);
	*/
}
#endif

/******************************************************************************
	function: mpu_mag_correct1
*******************************************************************************	
	Magnetic correction using factory parameters in fuse rom.
	
	                                (ASA-128)x0.5
	The correction is: Hadj = H x (--------------- + 1)
	                                    128
	
	With ASA=128 the output is equal to the input. 
	Values of ASA higher than 128 imply |Hadj|>|H| (sign is preserved). 
	Values of ASA lower than 128 imply |Hadj|<|H| (sign in preserved).
*******************************************************************************/
#if 0
// MPU9250 correction not available in ICM20948
void mpu_mag_correct1(signed short mx,signed short my,signed short mz,volatile signed short *mx2,volatile signed short *my2,volatile signed short *mz2)
{
	signed long lmx,lmy,lmz;
	lmx=mx;
	lmy=my;
	lmz=mz;
	
	signed long ax=_mpu_mag_asa[0],ay=_mpu_mag_asa[1],az=_mpu_mag_asa[2];
	
	ax=ax-128;
	ay=ay-128;
	az=az-128;
	
	//printf("%ld %ld %ld  %ld %ld %ld\n",lmx,lmy,lmz,ax,ay,az);
	
	lmx=lmx+(lmx*ax)/256;
	lmy=lmy+(lmy*ay)/256;
	lmz=lmz+(lmz*az)/256;
	
	//printf("aft %ld %ld %ld\n",lmx,lmy,lmz);
	
	// Could wrap around as the operation is from H-0.5H to H+0.5H and the H can be up to +/-32768; however if measuring earth field the max values are ~450 so no overflow
	
	*mx2=lmx;
	*my2=lmy;
	*mz2=lmz;
}
#endif
/******************************************************************************
	function: mpu_mag_correct2
*******************************************************************************	
	Magnetic correction using bias/sensitivity parameters found during calibration.
	
	The sensitivity is a N.7 fixed point number.
	
	Code size is smallest with /127, and >>7 is smaller than /127:
		/256 123250
		/128 123236
		/64 123248
		/32 123260
		/16 123248
		
		>>8 123196
		>>7 123192
		>>6 123216
	
*******************************************************************************/
void mpu_mag_correct2(signed short mx,signed short my,signed short mz,signed short *mx2,signed short *my2,signed short *mz2)
{
	// For correct rounding a better version would do (expr +64)/128
	*mx2=(mx+_mpu_mag_bias[0])*_mpu_mag_sens[0]/128;
	*my2=(my+_mpu_mag_bias[1])*_mpu_mag_sens[1]/128;
	*mz2=(mz+_mpu_mag_bias[2])*_mpu_mag_sens[2]/128;
}
void mpu_mag_correct2_inplace(signed short *mx,signed short *my,signed short *mz)
{
	
	*mx=(*mx+_mpu_mag_bias[0])*_mpu_mag_sens[0]/128;
	*my=(*my+_mpu_mag_bias[1])*_mpu_mag_sens[1]/128;
	*mz=(*mz+_mpu_mag_bias[2])*_mpu_mag_sens[2]/128;
}
void mpu_mag_correct2b(signed short mx,signed short my,signed short mz,signed short *mx2,signed short *my2,signed short *mz2)
{
	
	/**mx2=(mx+_mpu_mag_bias[0])*_mpu_mag_sens[0]/128;
	*my2=(my+_mpu_mag_bias[1])*_mpu_mag_sens[1]/128;
	*mz2=(mz+_mpu_mag_bias[2])*_mpu_mag_sens[2]/128;*/
	
	*mx2=((mx+_mpu_mag_bias[0])*_mpu_mag_sens[0]+64)>>7;
	*my2=((my+_mpu_mag_bias[1])*_mpu_mag_sens[1]+64)>>7;
	*mz2=((mz+_mpu_mag_bias[2])*_mpu_mag_sens[2]+64)>>7;
	
	/**mx2=((mx+_mpu_mag_bias[0])*_mpu_mag_sens[0])>>7;
	*my2=((my+_mpu_mag_bias[1])*_mpu_mag_sens[1])>>7;
	*mz2=((mz+_mpu_mag_bias[2])*_mpu_mag_sens[2])>>7;*/
}
void mpu_mag_correct2c(signed short mx,signed short my,signed short mz,signed short *mx2,signed short *my2,signed short *mz2)
{
	
	/**mx2=(mx+_mpu_mag_bias[0])*_mpu_mag_sens[0]/128;
	*my2=(my+_mpu_mag_bias[1])*_mpu_mag_sens[1]/128;
	*mz2=(mz+_mpu_mag_bias[2])*_mpu_mag_sens[2]/128;*/
	
	/**mx2=((mx+_mpu_mag_bias[0])*_mpu_mag_sens[0]+64)>>7;
	*my2=((my+_mpu_mag_bias[1])*_mpu_mag_sens[1]+64)>>7;
	*mz2=((mz+_mpu_mag_bias[2])*_mpu_mag_sens[2]+64)>>7;*/
	
	*mx2=((mx+_mpu_mag_bias[0])*_mpu_mag_sens[0])>>7;
	*my2=((my+_mpu_mag_bias[1])*_mpu_mag_sens[1])>>7;
	*mz2=((mz+_mpu_mag_bias[2])*_mpu_mag_sens[2])>>7;
}
/******************************************************************************
	function: mpu_mag_calibrate
*******************************************************************************	
	Find the magnetometer calibration coefficients.

	Finds a bias term to add to the magnetic data to ensure zero average.
	Finds a sensitivity term to multiply the zero-average magnetic data to span the range [-128,128]
	The sensitivity is a N.7 fixed-point number to multiply the integer magnetic field.
	
	The earth magnetic field is up to ~0.65 Gauss=65 uT. The sensitivity of the AK8963 is .15uT/LSB. 
	Hence the maximum readout is: 65uT/.15uT=433LSB. 
	Therefore assume readout < 450 for earth magnetic field.
	
	The maximum value of sens is 128*128=16384.
		
	Parameters:
		-
	Returns:
		Stores calibration coefficients in _mpu_mag_sens
*******************************************************************************/
void mpu_mag_calibrate(void)
{

	// Store previous mode
	unsigned char curautoread;
	unsigned char curmode = mpu_get_motionmode(&curautoread);


	signed m[3];
	WAITPERIOD p=0;
	unsigned long t1;
	MPUMOTIONDATA data;

	// Deactivate the automatic correction
	unsigned char _mpu_mag_correctionmode_back = _mpu_mag_correctionmode;
	_mpu_mag_correctionmode=0;

	// Activate a magnetic mode
	mpu_config_motionmode(MPU_MODE_102HZ_ACCGYROMAG100,1);
	
	_mpu_mag_calib_max[0]=_mpu_mag_calib_max[1]=_mpu_mag_calib_max[2]=-32768;
	_mpu_mag_calib_min[0]=_mpu_mag_calib_min[1]=_mpu_mag_calib_min[2]=+32767;

	fprintf(file_pri,"Magnetometer calibration: far from any metal, move the sensor in all orientations until no new numbers appear on screen and then press a key\n");

	t1=timer_ms_get();
	while(1)
	{
		if( fgetc(file_pri) != -1)
			break;
		timer_waitperiod_ms(10,&p);

		//if(mpu_data_getnext_raw(data))
		if(mpu_data_getnext_raw(&data))
			continue;
			
		m[0] = data.mx;
		m[1] = data.my;
		m[2] = data.mz;
		
		unsigned char dirty=0;
		for(unsigned char i=0;i<3;i++)
		{
			if(m[i]<_mpu_mag_calib_min[i])
			{
				_mpu_mag_calib_min[i]=m[i];
				dirty=1;
			}
			if(m[i]>_mpu_mag_calib_max[i])
			{
				_mpu_mag_calib_max[i]=m[i];
				dirty=1;
			}
		}
		
		if(dirty)
		{
			fprintf(file_pri,"[%d %d %d] - [%d %d %d]\n",_mpu_mag_calib_min[0],_mpu_mag_calib_min[1],_mpu_mag_calib_min[2],_mpu_mag_calib_max[0],_mpu_mag_calib_max[1],_mpu_mag_calib_max[2]);
		}
		if(timer_ms_get()-t1>1000)
		{
			fprintf(file_pri,"%d %d %d\n",m[0],m[1],m[2]);
			t1=timer_ms_get();
		}
	}

	// Restore the automatic correction
	_mpu_mag_correctionmode=_mpu_mag_correctionmode_back;

	
	// compute the calibration coefficients
	for(unsigned char i=0;i<3;i++)
	{
		// Bias: term added to magnetic data to ensure zero mean
		_mpu_mag_bias[i] = -(_mpu_mag_calib_max[i]+_mpu_mag_calib_min[i])/2;
		// Sensitivity: N.7 number to obtain a range [-256;256]
		if(_mpu_mag_calib_max[i]-_mpu_mag_calib_min[i]==0)
			_mpu_mag_sens[i]=1;
		else
			_mpu_mag_sens[i] = (256*128l)/(_mpu_mag_calib_max[i]-_mpu_mag_calib_min[i]);
	}
	mpu_mag_printcalib(file_pri);	
	mpu_mag_storecalib();


	// Restore to previous mode
	mpu_config_motionmode(curmode,curautoread);


}

void mpu_mag_storecalib(void)
{
//#warning "mpu_mag_storecalib must be fixed: eeprom_write_byte"
	for(unsigned char i=0;i<3;i++)
	{
		fprintf(file_pri,"calib %d %d %d\n",i,_mpu_mag_bias[i],_mpu_mag_sens[i]);

		eeprom_write_word(CONFIG_ADDR_MAG_BIASXL+i*2,_mpu_mag_bias[i]);
		
		eeprom_write_word(CONFIG_ADDR_MAG_SENSXL+i*2,_mpu_mag_sens[i]);
	}
}
void mpu_mag_loadcalib(void)
{
//#warning "mpu_mag_loadcalib must be fixed: eeprom_read_byte"
	for(unsigned char i=0;i<3;i++)
	{
		_mpu_mag_bias[i] = eeprom_read_word(CONFIG_ADDR_MAG_BIASXL+i*2,1,1);
		_mpu_mag_sens[i] = eeprom_read_word(CONFIG_ADDR_MAG_SENSXL+i*2,1,1);
	}
	_mpu_mag_correctionmode=eeprom_read_byte(CONFIG_ADDR_MAG_CORMOD,1,0);
	if(_mpu_mag_correctionmode) _mpu_mag_correctionmode=2;			// Sanitise: must be 0, or 2 with ICM
}
void mpu_mag_setandstorecorrectionmode(unsigned char mode)
{
	_mpu_mag_correctionmode=mode;
//#warning "mpu_mag_correctionmode must be fixed: eeprom_write_byte"
	eeprom_write_byte(CONFIG_ADDR_MAG_CORMOD,mode);
}
unsigned char mpu_LoadAccScale(void)
{
	return eeprom_read_byte(CONFIG_ADDR_ACC_SCALE,1,0b11)&0b11;
}
unsigned char mpu_LoadGyroScale(void)
{
	return eeprom_read_byte(CONFIG_ADDR_GYRO_SCALE,1,0b11)&0b11;
}
/******************************************************************************
	function: mpu_mag_selftest
*******************************************************************************
	Run the magnetometer self test.

	Parameters:
		-
	Returns:
		0		-	Magnetic self-test OK
		1		-	Fail
*******************************************************************************/
unsigned char mpu_mag_selftest(void)
{
	unsigned char mag[8],stok;
	signed short mx,my,mz;

	// Store previous mode
	unsigned char curautoread;
	unsigned char curmode = mpu_get_motionmode(&curautoread);

	// Activate a magnetic mode
	mpu_config_motionmode(MPU_MODE_102HZ_ACCGYROMAG100,0);

	// REMOVE SHADOWING
	_mpu_mag_regshadow(0,0,0,0);			// Stop shadowing

	fprintf(file_pri,"Magnetic self-test... ");

	// Self-test procedure
	// (1) Set Power-down mode. (MODE[3:0]=“0000”)
	//fprintf(file_pri,"Power down\n");
	mpu_mag_writereg(ICM_R_M_CNTL2,0x00);		// Power down

	// (2) Read ST2 to clear the DRDY flag
	mpu_mag_readreg(ICM_R_M_ST2);

	// (3) Set self-test mode
	//fprintf(file_pri,"Self-test mode\n");
	mpu_mag_writereg(ICM_R_M_CNTL2,0b00010000);		// Mode 4 (self-test)
	//mpu_mag_writereg(ICM_R_M_CNTL2,0b00000001);		// Mode 1 (single read) - this is meant to fail the test

	// (4) Check Data Ready or not by polling DRDY bit of ST1 register
	//fprintf(file_pri,"Wait DRDY\n");
	unsigned long t1 = timer_ms_get();
	while( !(mpu_mag_readreg(ICM_R_M_ST1)&0b1) )			// Wait for DRDY
	{
		if(timer_ms_get()-t1>250)
		{
			fprintf(file_pri,"DRDY timeout\n");

			mpu_config_motionmode(curmode,curautoread);
			return 0;
		}
	}

	// (5) Read measurement data (HXL to HZH) and TMPS, ST2
	//fprintf(file_pri,"Read H\n");
	for(unsigned i=0;i<8;i++)
		mag[i]=mpu_mag_readreg(ICM_R_M_HXL+i);

	// (6) Return to mode 0
	//fprintf(file_pri,"End self test\n");
	mpu_mag_writereg(ICM_R_M_CNTL2,0x00);		// Mode 0

	// Process data
	mx=mag[1]; mx<<=8; mx|=mag[0];
	my=mag[3]; my<<=8; my|=mag[2];
	mz=mag[5]; mz<<=8; mz|=mag[4];

	fprintf(file_pri,"%d %d %d: ",mx,my,mz);

	if(mx>-200 && mx<200 && my>-200 && my<200 && mz>-1000 && mz<-200)
		stok = 1;
	else
		stok = 0;

	if(stok)
		fprintf(file_pri,"Ok\n");
	else
		fprintf(file_pri,"Fail\n");



	// Restore to previous mode
	mpu_config_motionmode(curmode,curautoread);

	return stok;
}
void mpu_LoadBeta(void)
{
	float defbeta=0.35;
	unsigned defbetai = *(unsigned*)&defbeta;
	float beta;

	unsigned b = eeprom_read_dword(CONFIG_ADDR_BETA,1,defbetai);

	beta=*((float*)&b);

	_mpu_beta = beta;
}
void mpu_setandstorebeta(float beta)
{
	_mpu_beta = beta;
	mpu_StoreBeta(beta);
}
void mpu_StoreBeta(float beta)
{
//#warning "mpu_StoreBeta must be fixed: eeprom_write_dword"

	unsigned long b;	
	b=*((unsigned long*)&beta);	
	eeprom_write_dword(CONFIG_ADDR_BETA,b);
}


/******************************************************************************
*******************************************************************************
STATUS PRINT   STATUS PRINT   STATUS PRINT   STATUS PRINT   STATUS PRINT   STAT
*******************************************************************************
*******************************************************************************/


/******************************************************************************
	function: mpu_printreg
*******************************************************************************
	Prints the 4x128 registers of the ICM20948

	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_printreg(FILE *file)
{
	unsigned char v;

	fprintf(file,"ICM-20948 registers:\n");

	for(int bank=0;bank<4;bank++)
	{
		mpu_selectbank(bank);

		fprintf(file,"\tBank %d:\n",bank);
		fprintf(file,"\t\t");

		for(unsigned char i=0;i<=0x7F;i++)
		{
			fprintf(file,"%02X: ",i);
			v = mpu_readreg_wait_poll_block(i);
			fprintf(file,"%02X",v);
			if((i+1)%8==0 || i==0x7F)
			{
				if(i==0x7f)
					fprintf(file,"\n");
				else
					fprintf(file,"\n\t\t");
			}
			else
				fprintf(file,"   ");
		}
	}
	// Return to bank 0
	mpu_selectbank(0);

}


/******************************************************************************
	function: mpu_printreg2
*******************************************************************************
	Prints only the documented register range of the ICM20948

	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_printreg2(FILE *file)
{
	unsigned char v;
	unsigned char r[4] = {0x76, 0x28, 0x54, 0x17};	// Last register to print

	fprintf(file,"ICM-20948 registers:\n");

	for(int bank=0;bank<4;bank++)
	{
		mpu_selectbank(bank);

		fprintf(file,"\tBank %d:\n",bank);
		fprintf(file,"\t\t");

		for(unsigned char i=0;i<=r[bank];i++)
		{
			fprintf(file,"%02X: ",i);
			v = mpu_readreg_wait_poll_block(i);
			fprintf(file,"%02X",v);
			if((i+1)%8==0 || i==r[bank])
			{
				if(i==r[bank])
					fprintf(file,"\n");
				else
					fprintf(file,"\n\t\t");
			}
			else
				fprintf(file,"   ");
		}
	}
	// Return to bank 0
	mpu_selectbank(0);

}


/******************************************************************************
	function: mpu_printextreg
*******************************************************************************	
	Prints the external slave registers from the ICM 20948

	Updated: ICM 20948
	
	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_printextreg(FILE *file)
{
	unsigned char v;
	unsigned char j=0;
	
	fprintf(file,"External sensor:\n");
	for(unsigned char i=59;i<=82;i++)		// ICM 20948
	{
		fprintf(file,"%02X: ",i);
		v = mpu_readreg_wait_poll_block(i);
		fprintf(file,"%02X",v);
		if((j&0x7)==7 || i==96)
			fprintf(file,"\n");
		else
			fprintf(file,"   ");
		j++;
	}		
	
}


void mpu_printregdesc(FILE *file)
{
#if 0
	// MPU9250
	//	unsigned char address=105;
	unsigned char v[128];
	unsigned short s1,s2,s3;

	fprintf(file,"MPU-9250 registers:\n");
	mpu_readallregs(v);
	/*r = i2c_readregs(address,0,128,v);
	if(r!=0)
	{
		printf("Error\n");
		return;
	}*/
	fprintf(file,"Self test gyro x,y,z:  %02X %02X %02X\n",v[0],v[1],v[2]);
	fprintf(file,"Self test accel x,y,z: %02X %02X %02X\n",v[0x0d],v[0x0e],v[0x0f]);
	s1=v[0x13]; s1<<=8; s1|=v[0x14];
	s2=v[0x15]; s2<<=8; s2|=v[0x16];
	s3=v[0x17]; s3<<=8; s3|=v[0x18];
	fprintf(file,"Gyro offset x,y,z: %d %d %d\n",s1,s2,s3);
	fprintf(file,"SMPLRT_DIV:   %02X    CONFIG:  %02X    GYRO_CONFIG: %02X    ACCEL_CONFIG: %02X %02X\n",v[25],v[26],v[27],v[28],v[29]);
	fprintf(file,"LP_ACCEL_ODR: %02X    WOM_THR: %02X    FIFO_EN:     %02X\n",v[30],v[31],v[35]);
	fprintf(file,"I2C_MST_CTRL: %02X\n",v[36]);
	fprintf(file,"I2C_SLV0 ADDR:%02x    REG:     %02X    CTRL:        %02X  DO: %02X\n",v[37],v[38],v[39],v[99]);
	fprintf(file,"I2C_SLV1 ADDR:%02x    REG:     %02X    CTRL:        %02X  DO: %02X\n",v[40],v[41],v[42],v[100]);
	fprintf(file,"I2C_SLV2 ADDR:%02x    REG:     %02X    CTRL:        %02X  DO: %02X\n",v[43],v[44],v[45],v[101]);
	fprintf(file,"I2C_SLV3 ADDR:%02x    REG:     %02X    CTRL:        %02X  DO: %02X\n",v[46],v[47],v[48],v[102]);
	fprintf(file,"I2C_SLV4 ADDR:%02x    REG:     %02X    CTRL:        %02X  DO: %02X DI: %02X\n",v[49],v[50],v[52],v[51],v[53]);
	fprintf(file,"I2C_MST_STAT: %02X\n",v[54]);
	fprintf(file,"INT_PIN(55):  %02X    INT_EN:  %02X    INT_STATUS:  %02X\n",v[55],v[56],v[58]);
	s1=v[59]; s1<<=8; s1|=v[60];
	s2=v[61]; s2<<=8; s2|=v[62];
	s3=v[63]; s3<<=8; s3|=v[64];
	fprintf(file,"ACCEL: %05d %05d %05d\n",s1,s2,s3);
	s1=v[65]; s1<<=8; s1|=v[66];
	fprintf(file,"TEMP:  %05d\n",s1);
	s1=v[67]; s1<<=8; s1|=v[68];
	s2=v[69]; s2<<=8; s2|=v[70];
	s3=v[71]; s3<<=8; s3|=v[72];
	fprintf(file,"GYRO:  %05d %05d %05d\n",s1,s2,s3);
	for(int i=0;i<24;i++)
	{
		fprintf(file,"EXT_SENS_DATA %02X: %02X",i,v[73+i]);
		if((i+1)%4==0) fprintf(file,"\n"); else fprintf(file,"   ");
	}
	fprintf(file,"I2C_MST_DELAY_CTRL: %02X\n",v[103]);
	fprintf(file,"SIGNAL_PATH_RESET:  %02X\n",v[104]);
	fprintf(file,"MOT_DETECT_CTRL:    %02X  USER_CTRL:  %02X\n",v[105],v[106]);
	fprintf(file,"PWR_MGMT_1:         %02X  PWR_MGMT_2: %02X\n",v[107],v[108]);
	s1=v[114]; s1<<=8; s1|=v[115];
	fprintf(file,"FIFO_COUNT:         %d    FIFO_R_W:   %02X\n",s1,v[116]);
	fprintf(file,"WHO_AM_I:           %02X\n",v[117]);
	s1=v[119]; s1<<=7; s1|=(v[120])&0x7F;
	s2=v[122]; s2<<=7; s2|=(v[123])&0x7F;
	s3=v[125]; s3<<=7; s3|=(v[126])&0x7F;
	fprintf(file,"ACC_OFFSET: %05d %05d %05d\n",s1,s2,s3);
#else
	unsigned char pb = mpu_selectbank(2);
	unsigned char ac = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG);
	unsigned char ac2 = mpu_readreg_wait_poll_block(ICM_R_ACCEL_CONFIG_2);
	unsigned short adiv = mpu_readreg_wait_poll_block(ICM_R_ACCEL_SMPLRT_DIV_1); adiv<<=8; adiv|=mpu_readreg_wait_poll_block(ICM_R_ACCEL_SMPLRT_DIV_2);
	fprintf(file,"ACCEL_CONFIG: %02Xh. dlpcfg: %d scale: %d dlpfen: %d\n",ac,(ac>>3)&0b111,(ac>>1)&0b11,ac&1);
	fprintf(file,"ACCEL_CONFIG_2: %02Xh. ST X|Y|Z: %d|%d|%d dec3: %d\n",ac2,(ac2&0x10)?1:0,(ac2&0x08)?1:0,(ac2&0x04)?1:0,ac2&0b11);
	fprintf(file,"ACCEL_SMPLRT_DIV: %02Xh\n",adiv);

	unsigned char gc1 = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_1);
	unsigned char gc2 = mpu_readreg_wait_poll_block(ICM_R_GYRO_CONFIG_2);
	unsigned char gdiv = mpu_readreg_wait_poll_block(ICM_R_GYRO_SMPLRT_DIV);
	fprintf(file,"GYRO_CONFIG_1: %02Xh. dlpcfg: %d scale: %d dlpfen: %d\n",gc1,(gc1>>3)&0b111,(gc1>>1)&0b11,gc1&1);
	fprintf(file,"GYRO_CONFIG_2: %02Xh. ST X|Y|Z: %d|%d|%d dec3: %d\n",gc2,(gc2&0x10)?1:0,(gc2&0x08)?1:0,(gc2&0x04)?1:0,gc2&0b11);
	fprintf(file,"GYRO_SMPLRT_DIV: %02Xh\n",gdiv);

	// Timebase correction
	mpu_selectbank(1);
	unsigned char tbcorr = mpu_readreg_wait_poll_block(ICM_R_TIMEBASE_CORRECTION_PLL);
	int tbcorrppm=(int)((signed char)tbcorr);						// tbcorr is signed.
	tbcorrppm = tbcorrppm*790;										// LSB = 0.079% = 790ppm
	fprintf(file,"TIMEBASE_CORRECTION_PLL: %02Xh -> %dppm (%d %%)\n",tbcorr,tbcorrppm,tbcorrppm/10000);

	// FIFO
	mpu_selectbank(0);
	unsigned char fe1 = mpu_readreg_wait_poll_block(ICM_R_FIFO_EN_1);
	unsigned char fe2 = mpu_readreg_wait_poll_block(ICM_R_FIFO_EN_2);
	unsigned char frst = mpu_readreg_wait_poll_block(ICM_R_FIFO_RST);
	unsigned char fm = mpu_readreg_wait_poll_block(ICM_R_FIFO_MODE);
	unsigned char fch = mpu_readreg_wait_poll_block(ICM_R_FIFO_COUNTH);
	unsigned char fcl = mpu_readreg_wait_poll_block(ICM_R_FIFO_COUNTL);
	unsigned short fc = fch; fc<<=8; fc|=fcl;
	unsigned char usrctrl = mpu_readreg_wait_poll_block(ICM_R_USR_CTRL);
	fprintf(file,"FIFO: EN: %d SRAM_RST: %d\n",(usrctrl>>6)&1,(usrctrl>>2)&1);
	fprintf(file,"\tEN1: %02Xh EN2: %02Xh\n",fe1,fe2);
	fprintf(file,"\tRST: %02Xh MOD: %02Xh\n",frst,fm);
	fprintf(file,"\tCount: %d\n",fc);

	mpu_selectbank(pb);

#endif

}

void mpu_printregdesc2(FILE *file)
{
#if 0
	unsigned char v[128];
	
	fprintf(file,"MPU-9250 registers:\n");
	mpu_readallregs(v);
	fprintf(file," SMPLRT_DIV(19h):    %02X\n",v[0x19]);
	
	unsigned char dlp_cfg = v[0x1A]&0b111;
	unsigned char gyro_fchoice = v[0x1B]&0b11;
	fprintf(file," Configuration(1Ah): %02X. FIFO mode: %s. Ext sync: %d. DLP_CFG: %dd\n",v[0x1A],(v[0x1A]&0x40)?"No write on full":"Overwrite",(v[0x1A]>>3)&0b111,dlp_cfg);
	fprintf(file," GyroConfig(1Bh):    %02X. X ST: %d. Y ST: %d. Z ST: %d. FS: %d%db. fchoice_b: %d%db\n",v[0x1B],v[0x1B]&0x80?1:0,v[0x1B]&0x40?1:0,v[0x1B]&0x20?1:0,(v[0x1B]>>4)&0b1,(v[0x1B]>>3)&0b1,gyro_fchoice>>1,gyro_fchoice&0b1);
	fprintf(file," AccelConfig(1Ch):   %02X. X ST: %d. Y ST: %d. Z ST: %d. FS: %d%db.\n",v[0x1C],v[0x1C]&0x80?1:0,v[0x1C]&0x40?1:0,v[0x1C]&0x20?1:0,(v[0x1C]>>4)&0b1,(v[0x1C]>>3)&0b1);
	fprintf(file," AccelConfig2(1Dh):  %02X. fchoice_b: %db. A_DLPCFG: %dd\n",v[0x1D],v[0x1D]&0x8?1:0,v[0x1D]&0b111);
	fprintf(file," LPODR(1Eh):         %02X. clksel: %dd\n",v[0x1E],v[0x1E]&0b1111);
	fprintf(file," PWR_MGMT_1(6Bh):    %02X. rst: %d sleep: %d cycle: %d gyro_stby: %d PD-PTAT: %d sel: %dd\n",v[0x6B],v[0x6B]&0x80?1:0,v[0x6B]&0x40?1:0,v[0x6B]&0x20?1:0,v[0x6B]&0x10?1:0,v[0x6B]&0x08?1:0,v[0x6B]&0b111);
	fprintf(file," PWR_MGMT_2(6Ch):    %02X. dis_xa: %d dis_ya: %d dis_za: %d dis_xg: %d dis_yg: %d dis_zg: %d\n",v[0x6C],v[0x6C]&0x20?1:0,v[0x6C]&0x10?1:0,v[0x6C]&0x08?1:0,v[0x6C]&0x04?1:0,v[0x6C]&0x02?1:0,v[0x6C]&0x01);
#else
	fprintf(file,"Not implemented\n");
#endif

}

/******************************************************************************
	function: mpu_printfifo
*******************************************************************************	
	Prints the content of the FIFO 
	
	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_printfifo(FILE *file)
{
	unsigned char d;
	unsigned short n;
	n = mpu_getfifocnt();
	fprintf(file,"FIFO level: %d\n",n);
	if(n)
	{
		for(unsigned short i=0;i<n;i++)
		{
			mpu_fiforead(&d,1);
			fprintf(file,"%02X ",(int)d);
			if((i&7)==7 || i==n-1)
				fprintf(file_pri,"\n");
		}
		n = mpu_getfifocnt();
		fprintf(file,"FIFO level: %d\n",n);
	}
}

/******************************************************************************
	function: mpu_mag_printreg
*******************************************************************************	
	Prints the registers from the magnetometer.
	
	The MPU I2C interface must be active (mpu_mag_interfaceenable) 
	and the MPU must be configured to sample acceleration or gyroscope data 
	(not in low power mode).
	
	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_mag_printreg(FILE *file)
{
	unsigned char v;
	
	// List all registers which do not exist
	const unsigned char regs[16] = {0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x30, 0x31, 0x32};

	fprintf(file,"Mag registers:\n");
	/*for(unsigned char i=0;i<=0x32;i++)
	{
		fprintf(file,"%02X: ",i);
		v = mpu_mag_readreg(i);
		fprintf(file,"%02X",v);
		if( (i&7)==7 || i==0x32)
			fprintf(file,"\n");
		else
			fprintf(file,"   ");
	}*/
	for(unsigned char i=0;i<16;i++)
	{
		fprintf(file,"%02X: ",regs[i]);
		v = mpu_mag_readreg(regs[i]);
		fprintf(file,"%02X",v);
		if( (i&7)==7 || i==15)
			fprintf(file,"\n");
		else
			fprintf(file,"   ");
	}
	
}

void mpu_mag_printcalib(FILE *f)
{
	fprintf(f,"Mag bias: %d %d %d\n",_mpu_mag_bias[0],_mpu_mag_bias[1],_mpu_mag_bias[2]);
	fprintf(f,"Mag sens: %d %d %d\n",_mpu_mag_sens[0],_mpu_mag_sens[1],_mpu_mag_sens[2]);
	fprintf(f,"Mag corrmode: %d\n",_mpu_mag_correctionmode);
}

/******************************************************************************
	function: mpu_printstat
*******************************************************************************	
	Prints interrupt-driven (automatic mode) data acquisition statistics.
	
	Parameters:
		file	-	Output stream
*******************************************************************************/
void mpu_printstat(FILE *file)
{
	fprintf(file,"MPU acquisition statistics:\n");
	fprintf(file," MPU interrupts: %lu\n",mpu_cnt_int);
	fprintf(file," Samples: %lu\n",mpu_cnt_sample_tot);
	fprintf(file," Samples success: %lu\n",mpu_cnt_sample_succcess);
	fprintf(file," Errors: MPU I/O busy=%lu buffer=%lu\n",mpu_cnt_sample_errbusy,mpu_cnt_sample_errfull);
	fprintf(file," Buffer level: %u/%u\n",mpu_data_level(),MPU_MOTIONBUFFERSIZE);
	fprintf(file," Spurious ISR: %lu\n",mpu_cnt_spurious);
}



/*unsigned short mpu_estimateodr(void)
{
	// Estimate ODR of accelerometer
	unsigned long fr;
	unsigned short odr;
	fr = mpu_estimatefifofillrate(0b00001000);
	printf_P(PSTR("FIFO accel fill rate: %ld. ODR: %ld\n"),fr,fr/6);
	odr=fr/6;
	//fr = mpu_estimatefifofillrate(0b01000000);
	//printf_P(PSTR("FIFO gyro x fill rate: %ld. ODR: %ld\n"),fr,fr/2);
	//fr = mpu_estimatefifofillrate(0b01110000);
	//printf_P(PSTR("FIFO gyro x,y,z fill rate: %ld. ODR: %ld\n"),fr,fr/6);
	//fr = mpu_estimatefifofillrate(0b10000000);
	//printf_P(PSTR("FIFO temp fill rate: %ld. ODR: %ld\n"),fr,fr/2);
	//fr = mpu_estimatefifofillrate(0b11111000);
	//printf_P(PSTR("FIFO temp+acc+gyro fill rate: %ld. ODR: %ld\n"),fr,fr/14);
	
	return odr;
}*/

/*unsigned long mpu_estimatefifofillrate(unsigned char fenflag)
{
	unsigned char d;
	unsigned short n;
	unsigned long t1,t2;
	unsigned char debug=0;
	
	if(debug) printf_P(PSTR("Disable FIFO\n"));
	mpu_fifoenable(0x00);
	n = mpu_getfifocnt();
	if(debug) printf_P(PSTR("Empty FIFO: %d bytes\n"),n);
	for(unsigned short i=0;i<n;i++)
		mpu_fiforead(&d,1);
	n = mpu_getfifocnt();
	if(debug) printf_P(PSTR("FIFO level: %d\n"),n);
	_delay_ms(10);
	n = mpu_getfifocnt();
	if(debug) printf_P(PSTR("FIFO level: %d\n"),n);
	if(n!=0)
	{
		printf_P(PSTR("Error emptying FIFO: level %d\n"),n);
		return 0;
	}
	if(debug) printf_P(PSTR("Start ODR estimation - enable FIFO\n"));
	mpu_fifoenable(fenflag);		
	t1=timer_ms_get();
	while(1)
	{
		n = mpu_getfifocnt();
		t2 = timer_ms_get();
		
		if(n>450)
			break;
	}
	if(debug) printf_P(PSTR("End ODR estimation - disable FIFO\n"));
	mpu_fifoenable(0x00);
	if(debug) printf_P(PSTR("Got %d bytes in %ld ms\n"),n,t2-t1);
	unsigned long odr;
	odr = n*1000l/(t2-t1);
	if(debug) printf_P(PSTR("FIFO fills at %ld bytes/s\n"),odr);
	return odr;
}*/



// Try to speedup the conversion from the spi buffer to motiondata
// In benchmarks __mpu_copy_spibuf_to_mpumotiondata_1 is faster than __mpu_copy_spibuf_to_mpumotiondata_2 which is faster than __mpu_copy_spibuf_to_mpumotiondata_3. The asm version is 50% faster.
void __mpu_copy_spibuf_to_mpumotiondata_1(unsigned char *spibuf,unsigned char *mpumotiondata)
{
	spibuf++;				
	// Acc (big endian)
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf+=3;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf+=3;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	spibuf+=3;
	
	// Temp (big endian)
	mpumotiondata+=14;	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	spibuf+=3;
	
	// Gyr (big endian)
	mpumotiondata-=14;	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf+=3;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf+=3;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf--;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	
	// Mag
	spibuf+=2;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
	
	*mpumotiondata=*spibuf;
	mpumotiondata++;
	spibuf++;
}
void __mpu_copy_spibuf_to_mpumotiondata_2(unsigned char *spibuf,unsigned char *mpumotiondata)
{

	spibuf++;				
	// Acc (big endian)
	*mpumotiondata++=*spibuf--;
	*mpumotiondata++=*spibuf;
	spibuf+=3;
	
	*mpumotiondata++=*spibuf--;
	*mpumotiondata++=*spibuf;
	spibuf+=3;
	
	*mpumotiondata++=*spibuf--;
	*mpumotiondata=*spibuf;
	spibuf+=3;
	
	// Temp (big endian)
	mpumotiondata+=14;
	*mpumotiondata++=*spibuf--;
	*mpumotiondata=*spibuf;
	spibuf+=3;
	
	// Gyr (big endian)
	mpumotiondata-=14;	
	*mpumotiondata++=*spibuf--;
	*mpumotiondata++=*spibuf;
	spibuf+=3;
	
	*mpumotiondata++=*spibuf--;
	*mpumotiondata++=*spibuf;
	spibuf+=3;
	
	*mpumotiondata++=*spibuf--;
	*mpumotiondata++=*spibuf;

	// Mag
	spibuf+=2;
	*mpumotiondata++=*spibuf++;
	*mpumotiondata++=*spibuf++;
	
	*mpumotiondata++=*spibuf++;
	*mpumotiondata++=*spibuf++;
	
	*mpumotiondata++=*spibuf++;
	*mpumotiondata++=*spibuf++;

	*mpumotiondata++=*spibuf++;
}
void __mpu_copy_spibuf_to_mpumotiondata_3(unsigned char *spibuf,MPUMOTIONDATA *mpumotiondata)
{
	signed short ax,ay,az,gx,gy,gz,mx,my,mz,temp;
	unsigned char ms;	
	
	ax=spibuf[0]; ax<<=8; ax|=spibuf[1];
	ay=spibuf[2]; ay<<=8; ay|=spibuf[3];
	az=spibuf[4]; az<<=8; az|=spibuf[5];
	temp=spibuf[6]; temp<<=8; temp|=spibuf[7];
	gx=spibuf[8]; gx<<=8; gx|=spibuf[9];
	gy=spibuf[10]; gy<<=8; gy|=spibuf[11];
	gz=spibuf[12]; gz<<=8; gz|=spibuf[13];

	mx=spibuf[15]; mx<<=8; mx|=spibuf[14];
	my=spibuf[17]; my<<=8; my|=spibuf[16];
	mz=spibuf[19]; mz<<=8; mz|=spibuf[18];
	ms=spibuf[20];
	
	mpumotiondata->ax=ax;
	mpumotiondata->ay=ay;
	mpumotiondata->az=az;
	mpumotiondata->gx=gx;
	mpumotiondata->gy=gy;
	mpumotiondata->gz=gz;
	mpumotiondata->mx=mx;
	mpumotiondata->my=my;
	mpumotiondata->mz=mz;
	mpumotiondata->ms=ms;
	mpumotiondata->temp=temp;
}

void mpu_benchmark_isr(void)
{
	// Benchmark ISR
	unsigned long t1,t2;
	__mpu_sample_softdivider_divider=0;
	__mpu_autoread=1;
	//unsigned char spibuf[32];
	t1=timer_ms_get();
	for(unsigned i=0;i<50000;i++)
		mpu_isr();
		//unsigned char r = mpu_readregs_int_try_raw(spibuf,59,21);
	t2=timer_ms_get();
	printf("%ld\n",t2-t1);
	
	
}

/******************************************************************************
	function: mpu_kill
*******************************************************************************	
	Kills (i.e. sets to null) the sensors specified by the provided bitmap.
	
	Parameters:
		bitmap		-	3-bit bitmap indicating whether to null acc|gyr|mag (mag is LSB).
	
	Returns:
		-
	
*******************************************************************************/
void mpu_kill(unsigned char bitmap)
{
	_mpu_kill = bitmap;
}


/******************************************************************************
	mpu_readallregs
*******************************************************************************
	Read all MPU registers
******************************************************************************/
void mpu_readallregs(unsigned char *v)
{
	mpu_readregs_wait_poll_block(v,0,0x80);
}





