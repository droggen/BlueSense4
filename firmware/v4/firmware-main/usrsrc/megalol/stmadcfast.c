#if 1
/*
 * stmadc.c
 *
 *  Created on: 12 janv. 2020
 *      Author: droggen
 */
#include <string.h>
#include <stdio.h>
#include "adc.h"
#include "atomicop.h"
#include "usrmain.h"
#include "global.h"
#include "stmadcfast.h"
#include "stmadc.h"
#include "stmadcfast.h"
#include "stmrcc.h"
#include "wait.h"
#include "stm32l4xx_ll_tim.h"
#include "serial_itm.h"

unsigned short _stm_adcfast_dmabuf[ADCFAST_BUFFERSIZE*2];										// DMA tempory buffer - DMA uses double buffering so buffer twice the size of the payload (maxchannel)
unsigned short *_stm_adcfast_dmabuf_halfptr;														// Pointer to the second half of the DMA buffer

unsigned short _stm_adcfast_buffers[STM_ADCFAST_BUFFER_NUM][ADCFAST_BUFFERSIZE];		// ADC buffer: in DMA/interrupts; emptied by user application. Buffer size: ADCFAST_BUFFERSIZE entries
unsigned long _stm_adcfast_buffers_time[STM_ADCFAST_BUFFER_NUM];							// ADC buffer acquisition time
unsigned long _stm_adcfast_buffers_pktnum[STM_ADCFAST_BUFFER_NUM];							// ADC buffer packet counter

unsigned _stm_adcfast_effframesize;														// ADC acquires data in interleaved fashion and the effective frame size is an integer multiple of the channels acquired
unsigned long _stm_adcfast_ctr_acq,_stm_adcfast_ctr_loss;


unsigned short _stm_adcfast_buffer_rdptr,_stm_adcfast_buffer_wrptr;					// Read and write pointers

/******************************************************************************
	file: stmadc.c
*******************************************************************************


	*Hardware characteristics*

	* ADC: ADC1 on APB2
	* Timer to trigger ADC: TIM15 on APB2


	*Development information*
	Data is buffered in frames. Data within frames is interleaved. This ordering respects the natural way in which the DMA transfers data into memory.
	When acquiring only a single channel then the frame contains data of that channel only.
	Otherwise the frame contains c0|c1|c2...|c0|c1|c2...|c0...

	Frames can be purposefully made smaller to increase real-time responsiveness. In this implementation the remaining frame data is unused.
	This could be optimised to increase the number of frame count (i.e. have flexible frame number and frame size with framenumber*framesize=framemem.

	Data is stored as follows:
		data[frame][interleavedsamples]


	Two buffering behaviors are offered depending on STMADCFAST_KEEPOLDEST.
	With STMADCFAST_KEEPOLDEST=1, the buffering behavior is to keep old frames if the buffer is full. Buffered frames can become old, depending on readout speed.
	With STMADCFAST_KEEPOLDEST=0, the oldest frame is evicted when a new frame is received. This guarantees that buffered frames are the most recent.


	* Main functions*

	Data acquisition
		* stm_adcfast_acquire_start					Start acquisition of specified channels
		* stm_adcfast_acquire_stop					Stop data acquisition
		* stm_adcfast_data_level					Returns how many frames are in the buffers
		* stm_adcfast_isdata						Returns nonzero if data is available to read
		* stm_adcfast_isfull						Returns nonzero if buffers are full
		* stm_adcfast_data_clear					Clear acquisition buffers, and also clears acquisition stats
		* stm_adcfast_data_getnext					Get the next frame, removing it from the buffer
		* stm_adcfast_data_peeknext					Peek into the the next frame, leaving it in the buffer. Useful for in-place processing and minimising memory transfers.

	Statistics
		* stm_adcfast_clearstat
		* stm_adcfast_stat_totframes
		* stm_adcfast_stat_lostframes

	Benchmark
		* stm_adcfast_perfbench_withreadout
*/
/*
	Create assembly output:
	In Properties / C/C++ Build / Settings / Build Steps add post-build step with objdump: arm-none-eabi-objdump -D "${BuildArtifactFileBaseName}.elf" > "${BuildArtifactFileBaseName}.lst"
	I.e.:
	arm-none-eabi-objdump -D "${BuildArtifactFileBaseName}.elf" > "${BuildArtifactFileBaseName}.lst" && arm-none-eabi-objcopy -O ihex "${BuildArtifactFileBaseName}.elf" "${BuildArtifactFileBaseName}.hex" && arm-none-eabi-size "${BuildArtifactFileName}"
*/
/******************************************************************************
function: _stm_adcfast_acquire_start
*******************************************************************************
Start data acquisition of specified channels.
Specify the sample rate through dividers.

Setups GPIO as analog ADC.

Parameters:
	channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

Returns:
	0		-		Ok
	1		-		Error
******************************************************************************/
void stm_adcfast_acquire_start(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,unsigned prescaler,unsigned reload)
{
	fprintf(file_pri,"Channels: %u vbat: %u vref: %u temp: %u Prescaler: %u reload: %u\n",channels,vbat,vref,temp,prescaler,reload);


	unsigned timerclock=stm_rcc_get_apb2_timfrq();
	unsigned timerfrequency = timerclock/(prescaler+1)/(reload+1);	// Number of interrupt per seconds assuming one interrupt per sample

	fprintf(file_pri,"APB2 clock: %d\n",timerclock);
	fprintf(file_pri,"TIM15 frequency: %d\n",timerfrequency);










	// 1. Find number of selected channels
	// Initialise the ADC with all the channels for timing reasons
	int numchannels;
	stm_adcfast_init(channels,vbat,vref,temp,&numchannels);

	// 2. Calculate effectively used portion of the frame as floor(framesize/channels). Used to specify the size of the DMA transfer.
	// Calculate effectively used portion of frame in words. Example: buffer of 512 and 5 channels means 510 bytes are used.
	_stm_adcfast_effframesize = ((unsigned)ADCFAST_BUFFERSIZE/numchannels)*numchannels;

	// 3. Init the timer
	// Initialise the timer at desired repeat rate. Re-use stmadc function as behaviour identical.
	_stm_adc_init_timer(prescaler,reload);

	// 4. Initialise the channels as the normal functions
	// Start data acquisition
	stm_adcfast_acquirecontinuously(_stm_adcfast_effframesize);

	// Clear buffer and statistics
	stm_adcfast_data_clear();

	// Debug info
	fprintf(file_pri,"Eff frame size: %u. numchannels: %u. ADCFAST_BUFFERSIZE: %u\n",_stm_adcfast_effframesize,numchannels,ADCFAST_BUFFERSIZE);


}

void stm_adcfast_acquire_stop()
{
	HAL_ADC_Stop_DMA(&hadc1);
	// Stop timer
	_stm_adc_deinit_timer();
	// Deinit adc
	stm_adc_deinit();
}
/******************************************************************************
	function: stm_adc_init
*******************************************************************************
	Initialise ADC for scan conversion of several channels

	Setups GPIO as analog ADC.

	Parameters:
		channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

	Returns:
		0		-		Ok
		1		-		Error
		numchannel: 	number of channels acquired overall
******************************************************************************/
unsigned char stm_adcfast_init(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,int *numchannel)
{
	ADC_MultiModeTypeDef multimode = {0};
	ADC_ChannelConfTypeDef sConfig = {0};
	HAL_StatusTypeDef s;




	for(int i=0;i<_stm_adc_bs2maxchannel;i++)
	{
		fprintf(file_pri,"Channel %d: %08X\n",i,(unsigned)bs2stmmap[i]);
	}


	// Append the additional channels: vbat, vref, temp in the order <temp><vfref><vbat><int4><int3><int2><int1><int0>
	channels = _stm_adc_channels_to_bitmap(channels,vbat,vref,temp);
	_stm_adc_channels = channels;						// Save this for future de-initialisation
	int nconv = _stm_adc_channels_count(channels);		// Number of active channels for DMA grouping
	if(numchannel)
		*numchannel=nconv;								// Return how many channels are acquired overall

	fprintf(file_pri,"Channels: %X. Number of conversions in a scan: %u\n",_stm_adc_channels,nconv);



	// Initialise the GPIO in analog mode
	stm_adc_init_gpiotoadc(_stm_adc_channels);


	// Deinit


	//ADC_ConversionStop(&hadc1,ADC_REGULAR_INJECTED_GROUP);
	//ADC_Disable(&hadc1);
	//HAL_ADC_DeInit(&hadc1);

	// Common configuration
	hadc1.Instance = ADC1;
	//hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	hadc1.Init.LowPowerAutoWait = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.NbrOfConversion = nconv;
	hadc1.Init.DiscontinuousConvMode = DISABLE;

	hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T15_TRGO;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;

	hadc1.Init.DMAContinuousRequests = ENABLE;
	hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
	hadc1.Init.OversamplingMode = DISABLE;
	s = HAL_ADC_Init(&hadc1);
	if(s)
	{
		fprintf(file_pri,"Error initialising ADC\n");
		return 1;
	}
	/*
		Configure the ADC multi-mode. No multimode (independent ADC)
	*/
	multimode.Mode = ADC_MODE_INDEPENDENT;
	if(HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
	{
		fprintf(file_pri,"Error initialising ADC multimode\n");
		return 1;
	}


	// Config all the chnanels: hack to reset the sequence
	/*hadc1.Instance->SQR1=0x00;
	hadc1.Instance->SQR2=0x00;
	hadc1.Instance->SQR3=0x00;
	hadc1.Instance->SQR4=0x00;*/

	// Config all the chnanels
	int rankidx = 0;		// Index in the initialisation structure
	for(int i=0;i<_stm_adc_bs2maxchannel;i++)
	{
		if( !(channels&(1<<i)) )
			continue;
		sConfig.Channel = bs2stmmap[i];
		sConfig.Rank = bs2stmrank[rankidx];

		// Assuming realistic (desired) maximum sample rate of 20KHz with 8 channels: conversion time must be < 1/20K/8=6.25uS. ADC clock = 8MHZ -> conversion time < 50 ADC clock

		sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;			// Highest
		//sConfig.SamplingTime = ADC_SAMPLETIME_6CYCLES_5;
		//sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;			// Assuming 8 channels, ADC clock=8MHz -> max sample rate > 21KHz
		//sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
		//sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;			// AHB=32MHz; ADC clock = 8MHz; sample time: 8MHz/247 = 32KHz
		//sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
		sConfig.SingleDiff = ADC_SINGLE_ENDED;
		sConfig.OffsetNumber = ADC_OFFSET_NONE;
		sConfig.Offset = 0;
		if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
		{
			fprintf(file_pri,"Error initialising ADC channel %d\n",i);
			return 1;
		}

		rankidx++;
	}




	return 0;
}
void stm_adcfast_deinit()
{
	// Denitialise the GPIO to analog mode
	stm_adc_deinit_gpiotoanalog(_stm_adc_channels);
#warning check if must deinitialise internal channels
}

/******************************************************************************
	function: stm_adcfast_acquirecontinuously
*******************************************************************************
	Start acquisition (DMA, scan conversion)

	Parameters:
		effframesize	-		Effective number of interleaved channel samples in a frame

	Returns:
		0			-		Ok
		Nonzero		-		Error

******************************************************************************/
unsigned char stm_adcfast_acquirecontinuously(unsigned effframesize)
{
	HAL_StatusTypeDef s;

	fprintf(file_pri,"Effective frame size: %u\n",effframesize);

	// Calculate the half buffer position for faster readout
	_stm_adcfast_dmabuf_halfptr = _stm_adcfast_dmabuf+_stm_adcfast_effframesize;

	// Clear DMA buffer for debugging purposes
	memset(_stm_adcfast_dmabuf,0xff,ADCFAST_BUFFERSIZE*2*sizeof(unsigned short));

	// Install the callbacks
	_stm_adc_effective_callback_conv_cplt=_stm_adcfast_callback_conv_cplt;
	_stm_adc_effective_callback_conv_half_cplt=_stm_adcfast_callback_conv_half_cplt;

	s = HAL_ADC_Start_DMA(&hadc1,(uint32_t*)_stm_adcfast_dmabuf,_stm_adcfast_effframesize*2);		// Twice number of conversion to use double buffering

	fprintf(file_pri,"HAL_ADC_Start_DMA: %d\n",s);

	return s?1:0;
}
/******************************************************************************
	function: stm_adcfast_data_clear
*******************************************************************************
	Resets the ADC buffers
*******************************************************************************/
void stm_adcfast_data_clear(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_adcfast_buffer_wrptr=0;
		_stm_adcfast_buffer_rdptr=0;
		stm_adcfast_clearstat();
	}
}
/******************************************************************************
	function: stm_adc_clearstat
*******************************************************************************
	Clear audio frame acquisition statistics.

	Parameters:

	Returns:

******************************************************************************/
void stm_adcfast_clearstat()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_adcfast_ctr_acq=_stm_adcfast_ctr_loss=0;
	}
}
unsigned long stm_adcfast_stat_totframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_adcfast_ctr_acq;
	}
	return 0;
}
unsigned long stm_adcfast_stat_lostframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_adcfast_ctr_loss;
	}
	return 0;
}
/******************************************************************************
	function: stm_adcfast_data_level
*******************************************************************************
	Returns how many ADC frames are in the buffer.
*******************************************************************************/
unsigned short stm_adcfast_data_level(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return (_stm_adcfast_buffer_wrptr-_stm_adcfast_buffer_rdptr)&STM_ADCFAST_BUFFER_MASK;
	}
	return 0;	// To avoid compiler warning
}

/******************************************************************************
	function: stm_adcfast_isdata
*******************************************************************************
	Checks if a read buffer is available to be read.

	Parameters:

	Returns:
		0		-		No read buffer filled yet
		1		-		A read buffer is available to be read
******************************************************************************/
unsigned char stm_adcfast_isdata()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( _stm_adcfast_buffer_wrptr != _stm_adcfast_buffer_rdptr )
			return 1;
	}
	return 0;	// To prevent compiler from complaining
}
/******************************************************************************
	function: stm_adcfast_isfull
*******************************************************************************
	Returns 1 if the buffer is full, 0 otherwise.
*******************************************************************************/
unsigned char stm_adcfast_isfull(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( ((_stm_adcfast_buffer_wrptr+1)&STM_ADCFAST_BUFFER_MASK) == _stm_adcfast_buffer_rdptr )
			return 1;
		return 0;
	}
	return 0;	// To prevent compiler from complaining
}


/******************************************************************************
	function: _stm_adcfast_callback_conv_cplt
*******************************************************************************
	Called by HAL_ADC_ConvCpltCallback through the dispatcher _stm_adc_effective_callback_conv_cplt.

	Stores the acquired data in the ADC buffer
*******************************************************************************/
void _stm_adcfast_callback_conv_cplt(ADC_HandleTypeDef *hadc)
{
	unsigned long t = timer_ms_get();
	//unsigned long t=0;

#if 0
	static int hctr=0;
	hctr++;

	unsigned eoc = __HAL_ADC_GET_FLAG(hadc, ADC_FLAG_EOC);
	unsigned eos = __HAL_ADC_GET_FLAG(hadc, ADC_FLAG_EOS);

	unsigned dr = hadc->Instance->DR;
	unsigned v = HAL_ADC_GetValue(&hadc1);

	//itmprintf("HAL_ADC_ConvCpltCallback %u %lu\n",hctr,t);
	fprintf(file_pri,"HAL_ADC_ConvCpltCallback %u %lu\n",hctr,t);
	for(int i=0;i<ADCFAST_BUFFERSIZE*2;i++)
	{
		if((i&7)==0 && i)
			fprintf(file_pri,"\t");	// Spacing every 8
		if((i&31)==0 && i)
				fprintf(file_pri,"\n");	// Newline every 31
		//itmprintf("%04X ",_stm_adc_dmabuf[i]);
		fprintf(file_pri,"%04X ",_stm_adcfast_dmabuf[i]);
	}
	//itmprintf("\n");
	fprintf(file_pri,"\n");
	//fprintf(file_pri,"HAL_ADC_ConvCpltCallback %u %lu\n",hctr,t);
#endif

#if 1
	// Point to second half of DMA buffer
	_stm_adcfast_data_storenext(_stm_adcfast_dmabuf_halfptr,t);		// Copy data from 1st half of buffer.



#endif
	// Clear DMA buffer for debugging purposes
	//memset(_stm_adcfast_dmabuf,0xaa,ADCFAST_BUFFERSIZE*2*sizeof(unsigned short));
}

/******************************************************************************
	function: _stm_adcfast_callback_conv_half_cplt
*******************************************************************************
	Called by HAL_ADC_ConvHalfCpltCallback through the dispatcher _stm_adc_effective_callback_conv_half_cplt.

	Stores the acquired data in the ADC buffer
*******************************************************************************/

void _stm_adcfast_callback_conv_half_cplt(ADC_HandleTypeDef *hadc)
{
	unsigned long t = timer_ms_get();
	//unsigned long t=0;
#if 0
	static int hctr=0;
	hctr++;
	//itmprintf("HAL_ADC_ConvCpltCallback %u %lu\n",hctr,t);
	fprintf(file_pri,"HAL_ADC_ConvHalfCpltCallback %u %lu\n",hctr,t);
	for(int i=0;i<ADCFAST_BUFFERSIZE;i++)
	{
		if((i&7)==0 && i)
			fprintf(file_pri,"\t");	// Spacing every 8
		if((i&31)==0 && i)
				fprintf(file_pri,"\n");	// Newline every 31
		//itmprintf("%02X ",_stm_adc_dmabuf[i]);
		fprintf(file_pri,"%04X ",_stm_adcfast_dmabuf[i]);
	}
	//itmprintf("\n");
	fprintf(file_pri,"\n");
	//fprintf(file_pri,"HAL_ADC_ConvHalfCpltCallback %u %lu\n",hctr,t);
#endif
#if 1


	//itmprintf("HAL_ADC_ConvHalfCpltCallback %lu\n",t);
	_stm_adcfast_data_storenext(_stm_adcfast_dmabuf+0,t);		// Copy data from 1st half of buffer.



#endif
	// Clear DMA buffer for debugging purposes
	//memset(_stm_adcfast_dmabuf,0x55,ADCFAST_BUFFERSIZE*2*sizeof(unsigned short));
}




/******************************************************************************
	function: _stm_adc_data_storenext
*******************************************************************************
	Internally called from audio data interrupts to fill the ADC frame buffer.

	If the the ADC frame buffer is full, this function removes the oldest frame
	to make space for the new frame provided to this function.
	The rationale for this is to guarantee that the frames are "recent", i.e.
	the frame buffers stores only the most recent audio frames.
	This allows for better alignment with other data sources if buffers were to overrun.

	Do not call from user code.

	The restrict keyword on buffer helps the compiler optimise (x1.2 speedup).
	Restrict indicates that buffer does not point to e.g. _stm_adcfast_buffers, which speeds-up _stm_adcfast_buffers reads.

	Parameters:
		buffer		-		Pointer containing the new ADC frame to store in the frame buffer
		timems		-		Time when the DMA interrupt was triggered, i.e. time of acquisition of the last sample in a frame.

	Returns:
*******************************************************************************/
/*
	Various implementations including:
	- Using restrict: speedup
	- Using automatic loop unrolling: speeds up but only if 32-bit *dst++=*src++;
	- Manual loop unrolling: slower than automatic
	With 1KB buffer:
	- Not-unrolled w/ restrict: 12052 call/sec: 24MB/s
	- Auto unrolling w/ restrict: 21301: 43MB/s
*/
__attribute__((optimize("unroll-loops")))
void _stm_adcfast_data_storenext(unsigned short * restrict buffer,unsigned long timems)
{
#if STMADCFAST_KEEPOLDEST==0
	//ATOMIC_BLOCK(ATOMIC_RESTORESTATE)				// Storenext is always called from an interrupt - no point disabling them
	{
		// Check if buffer is full
		if( ((_stm_adcfast_buffer_wrptr+1)&STM_ADCFAST_BUFFER_MASK) == _stm_adcfast_buffer_rdptr )
		{
			// Drop the oldest frame by incrementing the read pointer
			_stm_adcfast_buffer_rdptr = (_stm_adcfast_buffer_rdptr+1)&STM_ADCFAST_BUFFER_MASK;
			// Increment the lost counter
			_stm_adcfast_ctr_loss++;
		}

		// Here the buffer is not full: copy the data at the write pointer
		// This could be optimised by a 32-bit copy. However, with restrict, this is done automatically.
		unsigned n = ADCFAST_BUFFERSIZE/2;
		//n=n/8;
		unsigned * restrict src=(unsigned* restrict)buffer;
		unsigned * restrict dst=(unsigned* restrict)_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr];
		for(;n!=0;n--)
		{
			//dst[i]=src[i];
			*dst++=*src++;
			// *dst++=*src++;
			//*dst++=*src++;
			//*dst++=*src++;
			//*dst++=*src++;
			//*dst++=*src++;
			//*dst++=*src++;
			//*dst++=*src++;
		}

		/*for(unsigned i=0;i<ADCFAST_BUFFERSIZE;i++)
			_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr][i] = buffer[i];*/
			//_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr][i] = *buffer++;
		//memcpy(buffer,_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr],ADCFAST_BUFFERSIZE*2);
			// Store time
		_stm_adcfast_buffers_time[_stm_adcfast_buffer_wrptr] = timems;
		// Store packet counter
		_stm_adcfast_buffers_pktnum[_stm_adcfast_buffer_wrptr] = _stm_adcfast_ctr_acq;

		// Increment the write pointer
		_stm_adcfast_buffer_wrptr = (_stm_adcfast_buffer_wrptr+1)&STM_ADCFAST_BUFFER_MASK;
		// Increment the frame counter
		_stm_adcfast_ctr_acq++;
		return;
	}
#else
	//ATOMIC_BLOCK(ATOMIC_RESTORESTATE)				// Storenext is always called from an interrupt - no point disabling them
		{
			// Check if buffer is full
			if( ((_stm_adcfast_buffer_wrptr+1)&STM_ADCFAST_BUFFER_MASK) == _stm_adcfast_buffer_rdptr )
			{
				// Drop the new frame.

				// Increment the lost counter
				_stm_adcfast_ctr_loss++;
				// Increment the frame counter
				_stm_adcfast_ctr_acq++;

				return;

			}

			// Here the buffer is not full: copy the data at the write pointer
			// This could be optimised by a 32-bit copy. However, with restrict, this is done automatically.
			unsigned n = ADCFAST_BUFFERSIZE/2;
			//n=n/8;
			unsigned * restrict src=(unsigned* restrict)buffer;
			unsigned * restrict dst=(unsigned* restrict)_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr];
			for(;n!=0;n--)
			{
				//dst[i]=src[i];
				*dst++=*src++;
				// *dst++=*src++;
				//*dst++=*src++;
				//*dst++=*src++;
				//*dst++=*src++;
				//*dst++=*src++;
				//*dst++=*src++;
				//*dst++=*src++;
			}

			/*for(unsigned i=0;i<ADCFAST_BUFFERSIZE;i++)
				_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr][i] = buffer[i];*/
				//_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr][i] = *buffer++;
			//memcpy(buffer,_stm_adcfast_buffers[_stm_adcfast_buffer_wrptr],ADCFAST_BUFFERSIZE*2);
				// Store time
			_stm_adcfast_buffers_time[_stm_adcfast_buffer_wrptr] = timems;
			// Store packet counter
			_stm_adcfast_buffers_pktnum[_stm_adcfast_buffer_wrptr] = _stm_adcfast_ctr_acq;

			// Increment the write pointer
			_stm_adcfast_buffer_wrptr = (_stm_adcfast_buffer_wrptr+1)&STM_ADCFAST_BUFFER_MASK;
			// Increment the frame counter
			_stm_adcfast_ctr_acq++;
			return;
		}
#endif
}



#endif
/******************************************************************************
	function: stm_adcfast_data_getnext
*******************************************************************************
	Returns the next data in the buffer, if data is available.

	This function removes the data from the read buffer and the next call
	to this function will return the next available data.

	If no data is available, the function returns an error.

	The data is copied from the internal audio frame buffers into the user provided
	buffer.

	The restrict keyword on buffer helps the compiler optimise (x3 speedup).
	Restrict indicates that buffer does not point to e.g. _stm_adcfast_buffers, which speeds-up _stm_adcfast_buffers reads.


	Parameters:
		buffer		-		Pointer to a buffer large enough for an ADC frame (ADCFAST_BUFFERSIZE samples)
		effsize		-		Effective number of interleaved samples in buffer. E.g. with a buffer size of 32 and 3 channels to read the effective size is 30
		timems
		pktctr

	Returns:
		0	-	Success
		1	-	Error (no data available in the buffer)
*******************************************************************************/
/*
	Fastest implementation: autoamtic loop unrolling with unsigned pointer access.
*/
__attribute__((optimize("unroll-loops")))
unsigned char stm_adcfast_data_getnext(unsigned short * restrict buffer,unsigned *effsize,unsigned long *timems,unsigned long *pktctr)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is empty
		if(_stm_adcfast_buffer_wrptr==_stm_adcfast_buffer_rdptr)
			return 1;
		// Copy the data
		if(buffer)
		{
			// Faster to copy with loop than calling memcpy
			//unsigned short *src = _stm_adcfast_buffers[_stm_adcfast_buffer_rdptr];
			/*
			for(unsigned i=0;i<ADCFAST_BUFFERSIZE;i++)
			{
				buffer[i] = _stm_adcfast_buffers[_stm_adcfast_buffer_rdptr][i];
				//buffer[i] = *src++;
				//buffer[i] = src[i];
			}
			*/
			unsigned n = ADCFAST_BUFFERSIZE/2;
			//n=n/8;
			unsigned * restrict dst=(unsigned* restrict)buffer;
			unsigned * restrict src=(unsigned* restrict)_stm_adcfast_buffers[_stm_adcfast_buffer_rdptr];
			for(;n!=0;n--)
			{
				*dst++=*src++;
			}

		}
		if(effsize)
			*effsize = _stm_adcfast_effframesize;
		if(timems)
			*timems = _stm_adcfast_buffers_time[_stm_adcfast_buffer_rdptr];
		if(pktctr)
			*pktctr = _stm_adcfast_buffers_pktnum[_stm_adcfast_buffer_rdptr];
		// Increment the read pointer
		_stm_adcfast_buffer_rdptr = (_stm_adcfast_buffer_rdptr+1)&STM_ADCFAST_BUFFER_MASK;
		return 0;
	}
	return 1;	// To avoid compiler warning
}
// This version does not check for a buffer to be available - used for benchmarking
__attribute__((optimize("unroll-loops")))
unsigned char stm_adcfast_data_getnext_anyway(unsigned short * restrict buffer,unsigned *effsize,unsigned long *timems,unsigned long *pktctr)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is empty
		//if(_stm_adcfast_buffer_wrptr==_stm_adcfast_buffer_rdptr)
			//return 1;
		// Copy the data
		if(buffer)
		{
			// Faster to copy with loop than calling memcpy
			//unsigned short *src = _stm_adcfast_buffers[_stm_adcfast_buffer_rdptr];
			/*
			for(unsigned i=0;i<ADCFAST_BUFFERSIZE;i++)
			{
				buffer[i] = _stm_adcfast_buffers[_stm_adcfast_buffer_rdptr][i];
				//buffer[i] = *src++;
				//buffer[i] = src[i];
			}
			*/
			unsigned n = ADCFAST_BUFFERSIZE/2;
			//n=n/8;
			unsigned * restrict dst=(unsigned* restrict)buffer;
			unsigned * restrict src=(unsigned* restrict)_stm_adcfast_buffers[_stm_adcfast_buffer_rdptr];
			for(;n!=0;n--)
			{
				*dst++=*src++;
			}

		}
		if(effsize)
			*effsize = _stm_adcfast_effframesize;
		if(timems)
			*timems = _stm_adcfast_buffers_time[_stm_adcfast_buffer_rdptr];
		if(pktctr)
			*pktctr = _stm_adcfast_buffers_pktnum[_stm_adcfast_buffer_rdptr];
		// Increment the read pointer
		_stm_adcfast_buffer_rdptr = (_stm_adcfast_buffer_rdptr+1)&STM_ADCFAST_BUFFER_MASK;
		return 0;
	}
	return 1;	// To avoid compiler warning
}
/******************************************************************************
	function: stm_adcfast_perfbench_withreadout
*******************************************************************************
	Benchmark all the ADC acquisition overhead and indicates CPU overhead and
	sample loss.

******************************************************************************/
unsigned long stm_adcfast_perfbench_withreadout(unsigned long mintime)
{
	unsigned long int t_last,t_cur;
	//unsigned long int tms_last,tms_cur;
	unsigned long int ctr,cps,nsample;
	//const unsigned long int mintime=1000;
	unsigned short adc[ADCFAST_BUFFERSIZE];		// ADC buffer
	unsigned long adcms,adcpkt;
	unsigned adcnc;

	ctr=0;
	nsample=0;

	//mintime=mintime*1000;
	t_last=timer_s_wait();
	stm_adcfast_data_clear();			// Clear data buffer and statistics

	unsigned long tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		ctr++;

		// Simulate reading out the data from the buffers
		// Read until no data available

		while(!stm_adcfast_data_getnext(adc,&adcnc,&adcms,&adcpkt))
			nsample++;

	}
	unsigned long tint2=timer_ms_get_intclk();

	unsigned long totf = stm_adc_stat_totframes();
	unsigned long lostf = stm_adc_stat_lostframes();

	cps = ctr/(t_cur-t_last);

	fprintf(file_pri,"perfbench_withreadout: %lu perf (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",cps,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"\tTotal frames: %lu. Lost frames: %lu\n",totf,lostf);
	return cps;
}

/******************************************************************************
	function: stm_adcfast_samplerate_to_divider
*******************************************************************************
	Calculates the required divider to achieve a specified sample rate,
	given a specified prescaler.

	Parameters:
		samplerate		-	Desired frequency in Hz
		prescaler		-	Pre-defined prescaler. The prescaler is the exact
							value in the prescaler register. I.e. prescaler=0 corresponds
							to a division by clock frequency division by (prescaler+1)
	Returns:
		Divider			-	Divider to achieve the desired frequency

******************************************************************************/
unsigned stm_adcfast_samplerate_to_divider(unsigned samplerate,unsigned prescaler)
{
	// Get the APB2 clock on which the timer triggering the interrupt is attached
	unsigned timclk = stm_rcc_get_apb2_timfrq();

	// sr = timclk/(prescaler+1)/(divider+1).
	// divider+1 = timclk/(prescaler+1)/sr
	// divider = timclk/(prescaler+1)/sr - 1
	// In order to round to nearest:
	// divider = (timclk + (prescaler+1)/2 + sr/2) / (prescaler+1)/sr -1;

	// unsigned divider = timclk/(prescaler+1)/sr - 1;					// Round down
	unsigned divider = divider = (timclk + (prescaler+1)/2 + samplerate/2) / (prescaler+1)/samplerate -1;		// Round nearest

	unsigned effectivefrq = timclk/(prescaler+1)/(divider+1);	// Verify effective number of samples per second

	fprintf(file_pri,"Desired sample rate: %u. Effective sample rate: %u with prescaler=%u, divider=%u. APB2 timer frequency: %u\n",samplerate,effectivefrq,prescaler,divider,timclk);
	return divider;
}
