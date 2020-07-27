#include <stdio.h>
#include <string.h>
#include "dfsdm.h"
#include "wait.h"
#include "atomicop.h"
#include "stmdfsdm.h"
#include "global.h"

unsigned char _stm_dfsmd_buffer_rdptr,_stm_dfsmd_buffer_wrptr;

STM_DFSMD_TYPE _stm_dfsmd_buffers[STM_DFSMD_BUFFER_NUM][STM_DFSMD_BUFFER_SIZE];	// Audio buffer
unsigned long _stm_dfsmd_buffers_time[STM_DFSMD_BUFFER_NUM];					// Audio buffer acquisition time
unsigned long _stm_dfsmd_buffers_pktnum[STM_DFSMD_BUFFER_NUM];					// Audio buffer packet counter
unsigned long _stm_dfsmd_ctr_acq,_stm_dfsmd_ctr_loss;							// DMA acquisition statistics
int _stm_dfsdm_dmabuf[STM_DFSMD_BUFFER_SIZE*2];									// Internal DMA audio buffer, tTwice size for copy on half full and full
unsigned int _stm_dfsdm_rightshift;												// Number of bits to right shift to obtain a 16-bit signal


const char *_stm_dfsdm_modes[STM_DFSMD_INIT_MAXMODES+1]   =
{
	// Off
    "Sound off",
	"8 KHz",
	"16 KHz",
	"20 KHz (poor quality)",
	"24 KHz (poor quality)",
	/*"32 KHz",
	"48 KHz",*/
};

/******************************************************************************
	function: stm_dfsdm_init
*******************************************************************************
	Initialises the system.

	Assume 20MHz system clock

	sps = clk / divider
	divider = fosr*(iosr-1+ford)+(ford+1)

	Parameters:
		mode	-	STM_DFSMD_INIT_16K or STM_DFSMD_INIT_8K

	Returns:

******************************************************************************/
void stm_dfsdm_init(unsigned mode)
{
	HAL_StatusTypeDef s;

	// Stop all what could be running
	s = HAL_DFSDM_FilterRegularStop_DMA(&hdfsdm1_filter0);
	s = HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter0);

	// Set acquisition mode
	fprintf(file_pri,"Internal microphone: ");
	switch(mode)
	{
	case STM_DFSMD_INIT_OFF:
		// Nothing to be done to turn off
#warning Power optimisation: turn off clocks
		fprintf(file_pri,"Off\n");
		return;
		break;

	case STM_DFSMD_INIT_16K:	// OK quality
		stm_dfsdm_initsampling(5,30,1,8);				// 16026 Hz

		// Range of signal: 30^5 = 24300000
		_stm_dfsdm_rightshift = 10;						// 24300000/2^10 = 23730

		// Experimentally with righshift 10 max range is +/- 3400 -> multiply by 4
		_stm_dfsdm_rightshift-=2;

		fprintf(file_pri,"16KHz\n");
		break;



	case STM_DFSMD_INIT_20K:
		stm_dfsdm_initsampling(5,21,1,9);				// ??

		// Range of signal: 21^5=4084101
		_stm_dfsdm_rightshift = 7;						// 4084101/2^7 = 31907

		// Experimentally with righshift ? max range is +/- ? -> multiply by 4
		_stm_dfsdm_rightshift-=2;

		fprintf(file_pri,"20KHz\n");
		break;

	case STM_DFSMD_INIT_24K:
		stm_dfsdm_initsampling(4,22,1,9);				// 23894

		// Range of signal: 22^4=234256
		_stm_dfsdm_rightshift = 3;						// 234256/2^3 = 29282

		// Experimentally with righshift 6 max range is +/- 4000 -> multiply by 4
		_stm_dfsdm_rightshift-=2;

		fprintf(file_pri,"24KHz\n");
		break;



	case STM_DFSMD_INIT_32K:
		//stm_dfsdm_initsampling(5,17,1,7);				// 31397
		stm_dfsdm_initsampling(3,22,1,9);				// 31397
		//stm_dfsdm_initsampling(5,13,1,9);				// 31298			Noisy
		//stm_dfsdm_initsampling(5,20,1,6);				// ?				Noisy

		// Range of signal: 17^5 = 1419857
		//_stm_dfsdm_rightshift = 6;						// 1419857/2^6 = 22185
		// 22^3
		_stm_dfsdm_rightshift = 0;

		// Experimentally with righshift 6 max range is +/- 4000 -> multiply by 4
		//_stm_dfsdm_rightshift-=2;

		fprintf(file_pri,"32KHz\n");
		break;

	case STM_DFSMD_INIT_48K:
		stm_dfsdm_initsampling(5,8,1,9);				// 48309

		// Range of signal: 8^5=32768
		_stm_dfsdm_rightshift = 0;						//
		//_stm_dfsdm_rightshift = 6;						// 1419857/2^6 = 22185

		// Experimentally with righshift 6 max range is +/- 4000 -> multiply by 4
		//_stm_dfsdm_rightshift-=2;

		fprintf(file_pri,"48KHz\n");
		break;



	case STM_DFSMD_INIT_8K:		// OK
	default:
		stm_dfsdm_initsampling(3,82,1,10);				// 8000 Hz

		// Range of signal: 82^3 = 551368
		_stm_dfsdm_rightshift = 5;						// 551368/2^5 = 17230

		// Experimentally with righshift 5 max range is +/- 2600 -> multiply by 4
		_stm_dfsdm_rightshift-=2;

		fprintf(file_pri,"8KHz\n");
	}
	// Start acquisition for polling
	HAL_DFSDM_FilterRegularStart(&hdfsdm1_filter0);
	// Reset offset
	s = HAL_DFSDM_ChannelModifyOffset(&hdfsdm1_channel5,0);
	// Calibrate
	int offset = stm_dfsdm_calib_zero_internal();
	s = HAL_DFSDM_ChannelModifyOffset(&hdfsdm1_channel5,offset);
	fprintf(file_pri,"\tUncalibrated offset %d... ",offset);
	offset = stm_dfsdm_calib_zero_internal();
	fprintf(file_pri,"After calibration: %d\n",offset);
	// Stop acquisition for polling
	HAL_DFSDM_FilterRegularStop(&hdfsdm1_filter0);

	// Start sampling
	stm_dfsdm_acquirdmaeon();

	// Reset sampling buffers and statistics
	stm_dfsdm_data_clear();

	(void) s;
}
/******************************************************************************
	function: stm_dfsdm_initsampling
*******************************************************************************
	Initialises the sampling parameters.

	Note that this requires the standard cubemx initialisation, and this
	re-initialises the parameters.

	This at places "hacks" the ST HAL library which does not offer functions to
	change sampling parameters.

	sps = clk / divider
	divider = fosr*(iosr-1+ford)+(ford+1)


	Parameters:
		order	-	Value from 1 to 5 inclusive specifying the sinc filter order
		fosr	-	filter oversample ration (downsampling)
		divider	-	sysclk divider which generates the microphone clock

	Returns:

******************************************************************************/
void stm_dfsdm_initsampling(unsigned order,unsigned fosr,unsigned iosr,unsigned div)
{
	HAL_StatusTypeDef s;
	//fprintf(file_pri,"Audio sampling with filter order %d, oversampling %d and clock divider %d\n",order,fosr,div);
	// Stop continuous conversion
	s = HAL_DFSDM_FilterConfigRegChannel(&hdfsdm1_filter0, DFSDM_CHANNEL_5,DFSDM_CONTINUOUS_CONV_OFF);
	/*if(s != HAL_OK)
	{
		fprintf(file_pri,"Error stopping regular conversion %d\n",s);
	}*/


	// Hack to reset filter - using HAL_DFSDM_FilterDeInit leads to issues when re-initting
	hdfsdm1_filter0.Instance->FLTCR1 &= ~(DFSDM_FLTCR1_DFEN);
	hdfsdm1_filter0.State = HAL_DFSDM_FILTER_STATE_RESET;

	//s = HAL_DFSDM_ChannelDeInit(&hdfsdm1_channel5);
	//fprintf(file_pri,"Channel deinit: %d\n",s);


	//HAL_Delay(100);


	//MX_DFSDM1_Init();


	/*fs = HAL_DFSDM_FilterGetState(&hdfsdm1_filter0);
	fprintf(file_pri,"Filter state: %d\n",fs);

	cs = HAL_DFSDM_ChannelGetState(&hdfsdm1_channel5);
	fprintf(file_pri,"Channel state: %d\n",cs);*/


	hdfsdm1_filter0.Instance = DFSDM1_Filter0;
	hdfsdm1_filter0.Init.RegularParam.Trigger = DFSDM_FILTER_SW_TRIGGER;
	hdfsdm1_filter0.Init.RegularParam.FastMode = DISABLE;
	hdfsdm1_filter0.Init.RegularParam.DmaMode = ENABLE;
	switch(order)
	{
		case 1:
			hdfsdm1_filter0.Init.FilterParam.SincOrder = DFSDM_FILTER_SINC1_ORDER;
			break;
		case 2:
			hdfsdm1_filter0.Init.FilterParam.SincOrder = DFSDM_FILTER_SINC2_ORDER;
			break;
		case 3:
			hdfsdm1_filter0.Init.FilterParam.SincOrder = DFSDM_FILTER_SINC3_ORDER;
			break;
		case 4:
			hdfsdm1_filter0.Init.FilterParam.SincOrder = DFSDM_FILTER_SINC4_ORDER;
			break;
		case 5:
		default:
			hdfsdm1_filter0.Init.FilterParam.SincOrder = DFSDM_FILTER_SINC5_ORDER;
			break;
	}
	hdfsdm1_filter0.Init.FilterParam.Oversampling = fosr;
	hdfsdm1_filter0.Init.FilterParam.IntOversampling = iosr;
	s = HAL_DFSDM_FilterInit(&hdfsdm1_filter0);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"\tError initialising filter %d\n",s);
	}
	//else
		//fprintf(file_pri,"Filter init ok\n");

	// Manually change divider
	uint32_t t;
	t = DFSDM1_Channel0->CHCFGR1;
	//fprintf(file_pri,"CHCFGR1: %8X\n",DFSDM1_Channel0->CHCFGR1);
	t &= 0x7fffffff;					// Clear DFSDMEN
	DFSDM1_Channel0->CHCFGR1 = t;

	//fprintf(file_pri,"CHCFGR1: %8X\n",DFSDM1_Channel0->CHCFGR1);

	t &= 0xff00ffff;					// Clear divider
	t |= (div-1)<<16;					// Set new divider
	DFSDM1_Channel0->CHCFGR1 = t;

	//fprintf(file_pri,"CHCFGR1: %8X\n",DFSDM1_Channel0->CHCFGR1);

	t |= 0x80000000;					// Set DFSDMEN
	DFSDM1_Channel0->CHCFGR1 = t;

	//fprintf(file_pri,"CHCFGR1: %8X\n",DFSDM1_Channel0->CHCFGR1);

	/*hdfsdm1_channel5.Instance = DFSDM1_Channel5;
	hdfsdm1_channel5.Init.OutputClock.Activation = ENABLE;
	hdfsdm1_channel5.Init.OutputClock.Selection = DFSDM_CHANNEL_OUTPUT_CLOCK_SYSTEM;
	hdfsdm1_channel5.Init.OutputClock.Divider = div;
	hdfsdm1_channel5.Init.Input.Multiplexer = DFSDM_CHANNEL_EXTERNAL_INPUTS;
	hdfsdm1_channel5.Init.Input.DataPacking = DFSDM_CHANNEL_STANDARD_MODE;
	hdfsdm1_channel5.Init.Input.Pins = DFSDM_CHANNEL_FOLLOWING_CHANNEL_PINS;
	hdfsdm1_channel5.Init.SerialInterface.Type = DFSDM_CHANNEL_SPI_RISING;
	hdfsdm1_channel5.Init.SerialInterface.SpiClock = DFSDM_CHANNEL_SPI_CLOCK_INTERNAL;
	hdfsdm1_channel5.Init.Awd.FilterOrder = DFSDM_CHANNEL_FASTSINC_ORDER;
	hdfsdm1_channel5.Init.Awd.Oversampling = 1;
	hdfsdm1_channel5.Init.Offset = 0;
	hdfsdm1_channel5.Init.RightBitShift = 0x00;
	s = HAL_DFSDM_ChannelInit(&hdfsdm1_channel5);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"Error initialising channel %d\n",s);
	}
	else
		fprintf(file_pri,"Channel init ok\n");*/

	s = HAL_DFSDM_FilterConfigRegChannel(&hdfsdm1_filter0, DFSDM_CHANNEL_5, DFSDM_CONTINUOUS_CONV_ON);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"\tError configuring channel regular conversion: %d\n",s);
	}

}
/******************************************************************************
	function: stm_dfsdm_clearstat
*******************************************************************************
	Clear audio frame acquisition statistics.

	Parameters:

	Returns:

******************************************************************************/
void stm_dfsdm_clearstat()
{
	_stm_dfsmd_ctr_acq=_stm_dfsmd_ctr_loss=0;
}
unsigned long stm_dfsdm_stat_totframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_dfsmd_ctr_acq;
	}
	return 0;
}
unsigned long stm_dfsdm_stat_lostframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_dfsmd_ctr_loss;
	}
	return 0;
}

/******************************************************************************
	function: stm_dfsdm_acq_poll_int
*******************************************************************************
	Polling data acquisition from internal microphone.

	Assumes filter 0 is configured with data coming from DATIN6 on the rising edge.

	Parameters:
		buffer		-	Buffer which receives the data
		n			-	Number of samples to acquire

	Returns:
		Data acquisition time in us
******************************************************************************/
unsigned long stm_dfsdm_acq_poll_internal(int *buffer,unsigned n)
{
	unsigned long t1 = timer_us_get();
	for(unsigned i=0;i<n;i++)
	{
		HAL_DFSDM_FilterPollForRegConversion(&hdfsdm1_filter0,1000);
		unsigned int c;
		int v = HAL_DFSDM_FilterGetRegularValue(&hdfsdm1_filter0,(uint32_t*)&c);
		buffer[i] = v;

	}
	unsigned long t2 = timer_us_get();
	return t2-t1;
}


/******************************************************************************
	function: stm_dfsdm_isdata
*******************************************************************************
	Checks if a read buffer is available to be read.

	Parameters:

	Returns:
		0		-		No read buffer filled yet
		1		-		A read buffer is available to be read
******************************************************************************/
unsigned char stm_dfsdm_isdata()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( _stm_dfsmd_buffer_wrptr != _stm_dfsmd_buffer_rdptr )
			return 1;
	}
	return 0;	// To prevent compiler from complaining
}
/******************************************************************************
	function: stm_dfsdm_isfull
*******************************************************************************
	Returns 1 if the buffer is full, 0 otherwise.
*******************************************************************************/
unsigned char stm_dfsdm_isfull(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( ((_stm_dfsmd_buffer_wrptr+1)&STM_DFSMD_BUFFER_MASK) == _stm_dfsmd_buffer_rdptr )
			return 1;
		return 0;
	}
	return 0;	// To prevent compiler from complaining
}
/******************************************************************************
	function: stm_dfsdm_data_getnext
*******************************************************************************
	Returns the next data in the buffer, if data is available.

	This function removes the data from the read buffer and the next call
	to this function will return the next available data.

	If no data is available, the function returns an error.

	The data is copied from the internal audio frame buffers into the user provided
	buffer.

	Parameters:
		buffer		-		Pointer to a buffer large enough for an audio frame (STM_DFSMD_BUFFER_SIZE samples)

	Returns:
		0	-	Success
		1	-	Error (no data available in the buffer)
*******************************************************************************/
unsigned char stm_dfsdm_data_getnext(STM_DFSMD_TYPE *buffer,unsigned long *timems,unsigned long *pktctr)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is empty
		if(_stm_dfsmd_buffer_wrptr==_stm_dfsmd_buffer_rdptr)
			return 1;
		// Copy the data
		memcpy((void*)buffer,(void*)_stm_dfsmd_buffers[_stm_dfsmd_buffer_rdptr],STM_DFSMD_BUFFER_SIZE*sizeof(STM_DFSMD_TYPE));

		if(timems)
			*timems = _stm_dfsmd_buffers_time[_stm_dfsmd_buffer_rdptr];
		if(pktctr)
			*pktctr = _stm_dfsmd_buffers_pktnum[_stm_dfsmd_buffer_rdptr];
		// Increment the read pointer
		_stm_dfsmd_buffer_rdptr = (_stm_dfsmd_buffer_rdptr+1)&STM_DFSMD_BUFFER_MASK;
		return 0;
	}
	return 1;	// To avoid compiler warning
}
/******************************************************************************
	function: _stm_dfsdm_data_storenext
*******************************************************************************
	Internally called from audio data interrupts to fill the audio frame buffer.

	If the the audio frame buffer is full, this function removes the oldest frame
	to make space for the new frame provided to this function.
	The rationale for this is to guarantee that the audio frames are "recent", i.e.
	the frame buffers stores only the STM_DFSMD_BUFFER_NUM most recent audio frames.
	This allows for better alignment with other data sources if buffers were to overrun.

	Do not call from user code.

	Parameters:
		buffer		-		Pointer containing the new audio frame to store in the frame buffer

	Returns:
*******************************************************************************/
void _stm_dfsdm_data_storenext(int *buffer,unsigned long timems)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is full
		if( ((_stm_dfsmd_buffer_wrptr+1)&STM_DFSMD_BUFFER_MASK) == _stm_dfsmd_buffer_rdptr )
		{
			// Drop the oldest frame by incrementing the read pointer
			_stm_dfsmd_buffer_rdptr = (_stm_dfsmd_buffer_rdptr+1)&STM_DFSMD_BUFFER_MASK;
			// Increment the lost counter
			_stm_dfsmd_ctr_loss++;
		}
		// Here the buffer is not full: copy the data at the write pointer
		// memcpy((void*)_stm_dfsmd_buffers[_stm_dfsmd_buffer_wrptr],(void*)buffer,STM_DFSMD_BUFFER_SIZE*sizeof(int));	// Copy if buffers are int; no bitshift to shrink to 16-bit
		for(unsigned i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
		{
			// Shift right to fit in signed short
			_stm_dfsmd_buffers[_stm_dfsmd_buffer_wrptr][i] = buffer[i]>>(_stm_dfsdm_rightshift+8);				// 8 LSB are channel number
		}
		// Store time
		_stm_dfsmd_buffers_time[_stm_dfsmd_buffer_wrptr] = timems;
		// Store packet counter
		_stm_dfsmd_buffers_pktnum[_stm_dfsmd_buffer_wrptr] = _stm_dfsmd_ctr_acq;

		// Increment the write pointer
		_stm_dfsmd_buffer_wrptr = (_stm_dfsmd_buffer_wrptr+1)&STM_DFSMD_BUFFER_MASK;
		// Increment the frame counter
		_stm_dfsmd_ctr_acq++;
		return;
	}
}
/******************************************************************************
	function: stm_dfsdm_data_level
*******************************************************************************
	Returns how many audio frames are in the buffer.
*******************************************************************************/
unsigned char stm_dfsdm_data_level(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return (_stm_dfsmd_buffer_wrptr-_stm_dfsmd_buffer_rdptr)&STM_DFSMD_BUFFER_MASK;
	}
	return 0;	// To avoid compiler warning
}
/******************************************************************************
	function: stm_dfsdm_data_clear
*******************************************************************************
	Resets the audio frame buffer
*******************************************************************************/
void stm_dfsdm_data_clear(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_dfsmd_buffer_wrptr=0;
		_stm_dfsmd_buffer_rdptr=0;
		stm_dfsdm_clearstat();
	}
}
void stm_dfsdm_data_printstat(void)
{
	fprintf(file_pri,"Total frames: %lu of which lost: %lu\n",stm_dfsdm_stat_totframes(),stm_dfsdm_stat_lostframes());
}
void stm_dfsdm_data_test_status(void)
{
	// Print info about the audio buffers
	fprintf(file_pri,"\trdptr: %d wrptr: %d. isfull: %d isdata: %d level: %d\n",_stm_dfsmd_buffer_rdptr,_stm_dfsmd_buffer_wrptr,stm_dfsdm_isfull(),stm_dfsdm_isdata(),stm_dfsdm_data_level());
	fprintf(file_pri,"\t\tdata at rdptr: %d\n",_stm_dfsmd_buffers[_stm_dfsmd_buffer_rdptr][0]);
	fprintf(file_pri,"\t\tdata at rdptr: %d\n",_stm_dfsmd_buffers[_stm_dfsmd_buffer_rdptr][0]);
}
void stm_dfsdm_data_test()
{
	// Test the audio frame buffers
	// cmd: 2 words with first indicating command and second data. First word=0: read, 1=write. Second word: data to write
	short cmd[]={			0,0,
							0,0,
							1,1,
							0,0,
							0,0,
							1,2,
							0,0,
							1,3,
							1,4,
							1,5,
							1,6,
							0,0,
							1,7,
							1,8,
							1,9,
							1,10,
							1,11,
							1,12,
							1,13,
							0,0,
							0,0,
							0,0,
							1,14,
							1,15,
							1,16,
							1,17,
							0,0,
							0,0,
							0,0,
							1,18,
							0,0,
							0,0,
							1,19,
							0,0,
							0,0,
							1,20,
							0,0,
							0,0,
							1,21,
							0,0,
							0,0,
							1,22,
							0,0,
							0,0
	};
	int ncmd = sizeof(cmd)/sizeof(short)/2;

	fprintf(file_pri,"ncmd: %d\n",ncmd);

	STM_DFSMD_TYPE tmp[STM_DFSMD_BUFFER_SIZE];
	memset(tmp,0,STM_DFSMD_BUFFER_SIZE*sizeof(signed short));
	stm_dfsdm_data_clear();
	stm_dfsdm_data_test_status();
	for(int i=0;i<ncmd;i++)
	{
		fprintf(file_pri,"Command %d: %d  %s ",i,cmd[i*2],cmd[i*2]?"Write":"Read");
		if(cmd[i*2])
			fprintf(file_pri,"%d",cmd[i*2+1]);
		fprintf(file_pri,"\n");

		if(cmd[i*2]==0)
		{
			unsigned char rv = stm_dfsdm_data_getnext(tmp,0,0);
			fprintf(file_pri,"\tgetnext: %d. Data[0]: %d\n",rv,tmp[0]);
		}
		else
		{
			tmp[0] = cmd[i*2+1];
			_stm_dfsdm_data_storenext(tmp,0);
		}

		stm_dfsdm_data_test_status();

		HAL_Delay(50);
	}
	stm_dfsdm_data_test_status();
}
/******************************************************************************
	function: _stm_dfsdm_data_rdnext
*******************************************************************************
	Advances the read pointer to access the next frame buffer.
	Do not call if the buffer is empty.
*******************************************************************************/
/*
void _stm_dfsdm_data_rdnext(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_dfsmd_buffer_rdptr = (_stm_dfsmd_buffer_rdptr+1)&STM_DFSMD_BUFFER_MASK;
	}
}
*/


/******************************************************************************
	function: stm_dfsdm_calib_zero_int
*******************************************************************************
	Calibrate the zero offset

	Assumes filter 0 is configured with data coming from DATIN6 on the rising edge.

	Parameters:
		-
	Returns:
		Offset
******************************************************************************/
int stm_dfsdm_calib_zero_internal()
{
	unsigned maxsample=8000;
	int data[maxsample];

	// Acquire bit of data to clear the DFSDM buffer
	stm_dfsdm_acq_poll_internal(data,1000);

	unsigned long dt = stm_dfsdm_acq_poll_internal(data,maxsample);
	(void) dt;

	// Calculate the mean
	long long mean = 0;
	for(unsigned i=0;i<maxsample;i++)
		mean+=data[i];
	mean/=(long long)maxsample;

	return (int)mean;
}

/******************************************************************************
	function: stm_dfsdm_acquireon
*******************************************************************************
	Turns on audio acquisition via framebuffers.

	Parameters:
		-
	Returns:
		Offset
******************************************************************************/
int stm_dfsdm_acquirdmaeon()
{
	HAL_StatusTypeDef s1 = HAL_DFSDM_FilterRegularStart_DMA(&hdfsdm1_filter0,(int32_t*)_stm_dfsdm_dmabuf,STM_DFSMD_BUFFER_SIZE*2);
	if(s1==0)
		fprintf(file_pri,"Audio frame acquisition by DMA started\n");
	else
		fprintf(file_pri,"Audio frame acquisition by DMA error: %d\n",s1);

	// Clear buffers & statistics
	stm_dfsdm_data_clear();
	return s1;
}
int stm_dfsdm_acquirdmaeoff()
{
	HAL_StatusTypeDef s3 = HAL_DFSDM_FilterRegularStop_DMA(&hdfsdm1_filter0);
	//fprintf(file_pri,"Audio frame acquisition by DMA stopped: %d\n",s3);
	return s3;
}
char *dfsdm_chcfgr1(unsigned reg)
{
	static char str[1024];
	*str=0;
	sprintf(&str[strlen(str)]," DFSDMEN: %d",(reg>>31)&0b1);
	sprintf(&str[strlen(str)]," CKOUTSRC: %s",((reg>>30)&0b1)?"Aud":"Sys");
	sprintf(&str[strlen(str)]," CKOUTDIV: %d",(reg>>16)&0xff);
	sprintf(&str[strlen(str)]," DATAPACK: ");
	switch((reg>>14)&0b11)
	{
	case 0:
		strcat(str,"Std  ");
		break;
	case 1:
		strcat(str,"Inter");
		break;
	case 2:
		strcat(str,"Dual ");
		break;
	default:
		strcat(str,"---");
	}
	sprintf(&str[strlen(str)]," DATAMPX: ");
	switch((reg>>12)&0b11)
	{
	case 0:
		strcat(str,"ext");
		break;
	case 1:
		strcat(str,"ADC");
		break;
	case 2:
		strcat(str,"CPU");
		break;
	default:
		strcat(str,"---");
	}

	sprintf(&str[strlen(str)]," CHINSEL: %s",((reg>>7)&1)?"next":"same");
	sprintf(&str[strlen(str)]," CHEN: %d",(reg>>7)&1);
	sprintf(&str[strlen(str)]," CKABEN: %d",(reg>>6)&1);
	sprintf(&str[strlen(str)]," SCDEN: %d",(reg>>5)&1);

	sprintf(&str[strlen(str)]," SPICKSEL: ");
	switch((reg>>2)&0b11)
	{
	case 0:
		strcat(str,"CKIN       ");
		break;
	case 1:
		strcat(str,"CKOUT      ");
		break;
	case 2:
		strcat(str,"CKOUT/2 fall");
		break;
	default:
		strcat(str,"CKOUT/2 rise");
	}
	sprintf(&str[strlen(str)]," SITP: ");
	switch(reg&0b11)
	{
	case 0:
		strcat(str,"SPI rise  ");
		break;
	case 1:
		strcat(str,"SPI fall  ");
		break;
	case 2:
		strcat(str,"Man rise=0");
		break;
	default:
		strcat(str,"Man rise=1");
	}
	return str;
}
char *dfsdm_chcfgr2(unsigned reg)
{
	static char str[1024];
	*str=0;
	sprintf(&str[strlen(str)]," OFFSET: %d",(reg>>8)&0xffffff);
	sprintf(&str[strlen(str)]," DTRBS: %d",(reg>>3)&0b11111);
	return str;
}

//unsigned long tdmacb;
//unsigned int ctrdmacb;
void HAL_DFSDM_FilterRegConvCpltCallback(DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
{
	//unsigned long t = timer_us_get();
	//fprintf(file_pri,"cb %lu %lu\n",t,t-tdmacb);
	//fprintf(file_pri,"cb %d: %lu %lu\n",ctrdmacb,t,t-tdmacb);
	//fprintf(file_itm,"cb %d: %lu %lu\n",ctrdmacb,t,t-tdmacb);
	//ctrdmacb++;
	//tdmacb = t;
	/*fprintf(file_pri,"\t%08X %08X %08X %08X %08X %08X %08X %08X\n",audbuf[0],audbuf[1],
				audbuf[AUDBUFSIZ/2-2],audbuf[AUDBUFSIZ/2-1],
				audbuf[AUDBUFSIZ/2],audbuf[AUDBUFSIZ/2+1],
				audbuf[AUDBUFSIZ-2],audbuf[AUDBUFSIZ-1]);*/
	/*for(int i=0;i<AUDBUFSIZ;i++)
	{
		fprintf(file_pri,"%08X ",audbuf[i]);
	}
	fprintf(file_pri,"\n\n");*/
	//for(int i=0;i<AUDBUFSIZ;i++)
	//	audbuf[i]=0x12345678;

	_stm_dfsdm_data_storenext(_stm_dfsdm_dmabuf+STM_DFSMD_BUFFER_SIZE,timer_ms_get());
}
void HAL_DFSDM_FilterRegConvHalfCpltCallback (DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
{
	//unsigned long t = timer_us_get();
	//fprintf(file_pri,"halfcb %u %lu %lu\n",ctrdmacb,t,t-tdmacb);
	//fprintf(file_itm,"halfcb %u %lu %lu\n",ctrdmacb,t,t-tdmacb);
	//ctrdmacb++;
	//tdmacb = t;
	/*fprintf(file_pri,"\t%08X %08X %08X %08X %08X %08X %08X %08X\n",audbuf[0],audbuf[1],
			audbuf[AUDBUFSIZ/2-2],audbuf[AUDBUFSIZ/2-1],
			audbuf[AUDBUFSIZ/2],audbuf[AUDBUFSIZ/2+1],
			audbuf[AUDBUFSIZ-2],audbuf[AUDBUFSIZ-1]);*/
/*	for(int i=0;i<AUDBUFSIZ;i++)
	{
		fprintf(file_pri,"%08X ",audbuf[i]);
	}
	fprintf(file_pri,"\n");*/
	//for(int i=0;i<AUDBUFSIZ;i++)
	//	audbuf[i]=0x87654321;

	// Copy data into next buffer

	_stm_dfsdm_data_storenext(_stm_dfsdm_dmabuf,timer_ms_get());


}



/******************************************************************************
	function: stm_dfsmd_perfbench_withreadout
*******************************************************************************
	Benchmark all the audio acquisition overhead and indicates CPU overhead and
	sample loss.

******************************************************************************/
unsigned long stm_dfsmd_perfbench_withreadout(unsigned long mintime)
{
	unsigned long int t_last,t_cur;
	//unsigned long int tms_last,tms_cur;
	unsigned long int ctr,cps,nsample;
	//const unsigned long int mintime=1000;
	STM_DFSMD_TYPE audio[STM_DFSMD_BUFFER_SIZE];
	unsigned long audioms,audiopkt;

	ctr=0;
	nsample=0;

	//mintime=mintime*1000;
	t_last=timer_s_wait();
	stm_dfsdm_data_clear();			// Clear data buffer and statistics

	unsigned long tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		ctr++;

		// Simulate reading out the data from the buffers
		// Read until no data available

		while(!stm_dfsdm_data_getnext(audio,&audioms,&audiopkt))
			nsample++;

	}
	unsigned long tint2=timer_ms_get_intclk();

	unsigned long totf = stm_dfsdm_stat_totframes();
	unsigned long lostf = stm_dfsdm_stat_lostframes();

	cps = ctr/(t_cur-t_last);

	fprintf(file_pri,"perfbench_withreadout: %lu perf (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",cps,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"\tTotal frames: %lu. Lost frames: %lu\n",totf,lostf);
	return cps;
}

void stm_dfsdm_printmodes(FILE *file)
{
	fprintf(file,"Sound modes:\n");
	for(unsigned char i=0;i<=STM_DFSMD_INIT_MAXMODES;i++)
	{
		char buf[128];
		stm_dfsdm_getmodename(i,buf);
		fprintf(file,"[%d]\t",i);
		fputs(buf,file);
		fputc('\n',file);
	}
}
/******************************************************************************
	function: stm_dfsdm_getmodename
*******************************************************************************
	Returns the name of a sound mode from its ID.

	Parameters:
		mode		-	Sound mode
		buffer		-	Long enough buffer to hold the name of the motion mode.
						The recommended length is 96 bytes (longest of mc_xx strings).
						The buffer is returned null-terminated.
		Returns:
			Name of the motion mode in buffer, or an empty string if the motionmode
			is invalid.
*******************************************************************************/
void stm_dfsdm_getmodename(unsigned char mode,char *buffer)
{
	buffer[0]=0;
	if(mode>STM_DFSMD_INIT_MAXMODES)
		return;
	strcpy(buffer,_stm_dfsdm_modes[mode]);
}
