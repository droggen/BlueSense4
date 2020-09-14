#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "wait.h"
#include "atomicop.h"
#include "stmdfsdm.h"
#include "global.h"
#include "eeprom-m24.h"



/******************************************************************************
	file: stmdfsdm
*******************************************************************************


	*Hardware characteristics: Internal microphone*

	* Internal microphone on DFSDM1-DATIN6
	* DATIN6 is decoded by channel 6 and channel 6-1=5
 	* Channel 5: rising edge			Decoded by: filter 0		Microphone with LR=0			Location: Next to power input		Selected with: STM_DFSMD_LEFT
	* Channel 6: falling edge			Decoded by: filter 1		Microphone with LR=VCC			Location: Next to push button		Selected with: STM_DFSMD_RIGHT


	*Development information*

	In STM32CubeMX the following must be set:
	* Two DMA channels called DFSDM1_FLT0 (DM1 Channel 4) and DFSDM1_FLT1 (DM1 Channel 5) set up as: Peripheral to Memory; Circular; Increment address memory only; Peripheral is word; Memory is word
	* Channel 5: SPI with rising;  Internal SPI; Offset=0; Bit shift=0; Analog watchdog: fastsinc oversampling 1 (LEFT microphone)
	* Channel 6: SPI with falling; Internal SPI; Offset=0; Bit shift=0; Analog watchdog: fastsinc oversampling 1 (RIGHT microphone)
	* Filter 0: Regular channel 5; Continuous mode; Software trigger; Fast disabled; DMA enabled; All injected disabled; Filter sinc3 fosr=70, iosr=1 (LEFT microphone)
	* Filter 1: Regular channel 6; Continuous mode; Software trigger; Fast disabled; DMA enabled; All injected disabled; Filter sinc3 fosr=70, iosr=1 (RIGHT microphone)

	DFSDM internal right shift applied first; then offset is applied.

	Microphone performs best at higher end of frequency range. Currently 20MHz/6=3.33MHz (2.5% above documented spec)

	* Main functions*


	Data acquisition
		* stm_dfsdm_init			-	Primary function to start or stop data acquisition. Data acquisition is done via DMA and accessed with stm_dfsdm_data_getnext

	Statistics
		* stm_dfsdm_clearstat
		* stm_dfsdm_stat_totframes
		* stm_dfsdm_stat_lostframes
		* stm_dfsdm_data_printstat





TODO:
1. Select left or right microphone
1.1 Parameter in init to select microphone
1.2 No change in buffering or streaming
1.3 Off parameter will turn off

2. Select both microphone

3. Instead of software rightshift use hardware right shift

4. getnext frame:
Instead of returning mono frames try to return stereo frame:
- assumes left/right frames alternate in circular buffer
- return immediately if there are not at least 2 frames in circular buffer
- if two frames, check if both are identical packet counter: if yes -> stereo and returned combined. If not


DFSDM_Channel_InitTypeDef.RightBitShift: DFSDM channel right bit shift. This parameter must be a number between Min_Data = 0x00 and Max_Data = 0x1F
Configure the output clock, input, serial interface, analog watchdog, offset and data right bit shift parameters for this channel using the HAL_DFSDM_ChannelInit() function.




******************************************************************************/


unsigned char _stm_dfsmd_buffer_rdptr,_stm_dfsmd_buffer_wrptr;

STM_DFSMD_TYPE _stm_dfsmd_buffers[STM_DFSMD_BUFFER_NUM][STM_DFSMD_BUFFER_SIZE];	// Audio buffer
unsigned long _stm_dfsmd_buffers_time[STM_DFSMD_BUFFER_NUM];					// Audio buffer acquisition time
unsigned long _stm_dfsmd_buffers_pktnum[STM_DFSMD_BUFFER_NUM];					// Audio buffer packet counter
unsigned char _stm_dfsmd_buffers_leftright[STM_DFSMD_BUFFER_NUM];				// Audio buffer left_right indication
unsigned long _stm_dfsmd_ctr_acq[2],_stm_dfsmd_ctr_loss;						// DMA acquisition statistics; the acquisition counter contains independently the left/right channels
int _stm_dfsdm_dmabuf[STM_DFSMD_BUFFER_SIZE*2];									// Internal DMA audio buffer, twice size for copy on half full and full
int _stm_dfsdm_dmabuf2[STM_DFSMD_BUFFER_SIZE*2];								// Internal DMA audio buffer, twice size for copy on half full and full
unsigned int _stm_dfsdm_rightshift;												// Number of bits to right shift to obtain a 16-bit signal


// filters[0] and channels[0] are STM_DFSMD_LEFT
// filters[1] and channels[1] are STM_DFSMD_RIGHT
DFSDM_Filter_HandleTypeDef *_stm_dfsdm_filters[2] = {&hdfsdm1_filter0,&hdfsdm1_filter1};
DFSDM_Channel_HandleTypeDef *_stm_dfsdm_channels[2] = {&hdfsdm1_channel5,&hdfsdm1_channel6};

const char *_stm_dfsdm_modes_20MHz[STM_DFSMD_INIT_MAXMODES+1]   =
{
	// Off
    "Sound off",
	"8 KHz (8003Hz)",
	"16 KHz (16181Hz)",
	"20 KHz (20080Hz)",
};
const char *_stm_dfsdm_modes_16MHz[STM_DFSMD_INIT_MAXMODES+1]   =
{
	// Off
    "Sound off",
	"8 KHz (8008Hz)",
	"16 KHz (15920Hz)",
	"20 KHz (19875Hz)",
};
#define _stm_dfsdm_modes _stm_dfsdm_modes_16MHz

/******************************************************************************
	_stm_dfsdm_modes_param
*******************************************************************************
	Parameters to set-up the audio acquisition at pre-defined frequencies.
	Each entry comprises the following parameters in this order: order fosr iosr div shift
******************************************************************************/
const unsigned char _stm_dfsdm_modes_param_20MHz[STM_DFSMD_INIT_MAXMODES+1][5] = {
		// These parameters assume an audio clock of 20MHz.

		// OFF - the parameters are irrelevant for this mode
		{0,     0,     0,     0,     0},

		// 8KHz
		{4,     88,    1,     7,     11},			// 8003Hz. Clipping rightshift: 10. Ok rightshift 11
		//{3,82,1,10,3},							// Alternative settings with non-overclocked microphone, poorer quality

		// 16KHz
		{5,     40,    1,     6,     10},			// 16181 Hz. Clipping with rightshift: 7, 8, 9. Ok with rightshift: 10

		// 20KHz
		{5,     32,    1,     6,     8},			// 20080 Hz with clock at 3.33MHz. Rightshift=6 leads to clipping; rightshift=7 also clipping; minimum experimental is 8

};
const unsigned char _stm_dfsdm_modes_param_16MHz[STM_DFSMD_INIT_MAXMODES+1][5] = {
		// These parameters assume an audio clock of 16MHz.

		// OFF - the parameters are irrelevant for this mode
		{0,     0,     0,     0,     0},

		// 8KHz
		{4,     82,    1,     6,     8},			// 8008Hz. Clipping rightshift: 8 (with 11: -3500-+3500)

		// 16KHz
		{5,     39,    1,     5,     9},			// 15920 Hz. Clipping rightshift: 9 (with 10: -11000-+11000)

		// 20KHz
		{5,     31,    1,     5,     8},			// 19875 Hz. Clipping rightshift: 8 (with 8: -9000-+13000)

};
// Select the parameters
//#define _stm_dfsdm_modes_param _stm_dfsdm_modes_param_20MHz
#define _stm_dfsdm_modes_param _stm_dfsdm_modes_param_16MHz


/******************************************************************************
	function: stm_dfsdm_init
*******************************************************************************
	Primary function to initialise and start, or stop, audio data acquisition by DMA.

	Starts or stops the audio data acquisition with DMA at the specified sample
	rate and with the specified left/right/stereo channels.

	Acquisition statistics are cleared when starting.

	Assume 20MHz system clock

	sps = clk / divider
	divider = fosr*(iosr-1+ford)+(ford+1)

	Parameters:
		mode		-	STM_DFSMD_INIT_OFF, STM_DFSMD_INIT_8K, STM_DFSMD_INIT_16K, ...
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT, STM_DFSMD_STEREO. This parameter is unused if mode is STM_DFSMD_INIT_OFF.

	Returns:

******************************************************************************/
void stm_dfsdm_init(unsigned mode,unsigned char left_right)
{
	HAL_StatusTypeDef s;

	fprintf(file_pri,"Audio initialisation. Memory used: %u bytes\n",stm_dfsdm_memoryused());




	/*for(int i=0;i<2;i++)
	{
		fprintf(file_pri,"Filter %d: %p\n",i,_stm_dfsdm_filters[i]);
		fprintf(file_pri,"Channel %d: %p\n",i,_stm_dfsdm_channels[i]);
	}
	fprintf(file_pri,"Filter %p %p\n",&hdfsdm1_filter0,&hdfsdm1_filter1);
	fprintf(file_pri,"Channel %p %p\n",&hdfsdm1_channel5,&hdfsdm1_channel6);*/


	//stm_dfsdm_state_print();

	// Stop all what could be running on filters 0 and 1 - polling or DMA
	stm_dfsdm_acquire_stop_all();

	// Initialise the filters for the predefined settings
	_stm_dfsdm_init_predef(mode,left_right);
	// If "off" mode, do not continue.
	if(mode==STM_DFSMD_INIT_OFF)
		return;

	//fprintf(file_pri,"Predefined initialisation done\n");

	//stm_dfsdm_state_print();

	// Calibration procedure to cancel offset
	// Start acquisition for polling
	stm_dfsdm_acquire_start_poll(left_right);		// This starts the left/right/stereo as appropriate

	//HAL_Delay(100); fprintf(file_pri,"After start poll\n");
	//stm_dfsdm_state_print();



	// Measure offset and print
	_stm_dfsdm_offset_calibrate(left_right,1);

	// Stop polling sampling
	stm_dfsdm_acquire_stop_poll();

	// Start sampling (also clears stats)
	stm_dfsdm_acquire_start_dma(left_right);


	// This tentatively aims to synchronise the channels; but it does not seem necessary.
	//_stm_dfsdm_sampling_sync();


	(void) s;
}



void _stm_dfsdm_init_predef(unsigned mode,unsigned char left_right)
{
	// Set acquisition mode
	fprintf(file_pri,"\tInternal microphone: ");
	switch(left_right)
	{
		case STM_DFSDM_LEFT:
			fprintf(file_pri,"left ");
			break;
		case STM_DFSDM_RIGHT:
			fprintf(file_pri,"right ");
			break;
		case STM_DFSDM_STEREO:
		default:
			fprintf(file_pri,"stereo ");
			break;
	}

	// Print the mode (off or frequency)
	fprintf(file_pri,"%s\n",_stm_dfsdm_modes[mode]);
	// If mode is off, nothing more to do.
	if(mode==STM_DFSMD_INIT_OFF)
		return;
	// Init the mode
	stm_dfsdm_initsampling(left_right,_stm_dfsdm_modes_param[mode][0],_stm_dfsdm_modes_param[mode][1],_stm_dfsdm_modes_param[mode][2],_stm_dfsdm_modes_param[mode][3]);
	_stm_dfsdm_rightshift=_stm_dfsdm_modes_param[mode][4];

	// Get the volume gain
	int v = stm_dfsdm_loadvolumegain();
	if(v>0)
		_stm_dfsdm_rightshift-=v;		// Positive: higher gain wanted -> less right shift
	else
		_stm_dfsdm_rightshift+=(-v);	// Negative: higher gain wanted -> less right shift
	fprintf(file_pri,"\tVolume gain: %d (rightshift: %d)\n",v,_stm_dfsdm_rightshift);

	// Set the hardware right shift
	stm_dfsdm_rightshift_set(left_right,_stm_dfsdm_rightshift);

}
/******************************************************************************
	function: stm_dfsdm_acquire_start_poll
*******************************************************************************
	Start the data acquisition by polling.

	The initialisation function must be called beforehand.

	TODO: add which is the initialisation function


	Parameters:
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT, STM_DFSMD_STEREO

	Returns:

******************************************************************************/
void stm_dfsdm_acquire_start_poll(unsigned char left_right)
{
	fprintf(file_pri,"stm_dfsdm_acquire_start_poll %d\n",left_right);

	HAL_StatusTypeDef s;
	assert(left_right<3);
	if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
	{
		fprintf(file_pri,"Starting left ");
		s=HAL_DFSDM_FilterRegularStart(_stm_dfsdm_filters[0]);
		fprintf(file_pri,"%d\n",s);
		if(s!=HAL_OK)
			fprintf(file_pri,"Error starting polling left (%d)\n",s);
	}
	if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
	{
		fprintf(file_pri,"Starting right ");
		s=HAL_DFSDM_FilterRegularStart(_stm_dfsdm_filters[1]);
		fprintf(file_pri,"%d\n",s);
		if(s!=HAL_OK)
			fprintf(file_pri,"Error starting polling right (%d)\n",s);
	}
	(void) s;
}
/******************************************************************************
	function: stm_dfsdm_offset_set
*******************************************************************************
	Set the offset.


	Parameters:
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT or STM_DFSDM_STEREO. In stereo mode, both channels are set the same offset. To set individual channel offsets, call this function twice for left and right.
		offset		-	Offset (the negative of that will be added to the signal).

	Returns:

******************************************************************************/
void stm_dfsdm_offset_set(unsigned char left_right,int offset)
{
	HAL_StatusTypeDef s;
	assert(left_right<3);
	if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
	{
		s = HAL_DFSDM_ChannelModifyOffset(_stm_dfsdm_channels[STM_DFSDM_LEFT],offset);
		if(s!=HAL_OK)
			fprintf(file_pri,"Error setting left offset\n");
	}
	if(left_right==STM_DFSDM_RIGHT|| left_right==STM_DFSDM_STEREO)
	{
		s = HAL_DFSDM_ChannelModifyOffset(_stm_dfsdm_channels[STM_DFSDM_RIGHT],offset);
		if(s!=HAL_OK)
			fprintf(file_pri,"Error setting right offset\n");
	}
}
/******************************************************************************
	function: stm_dfsdm_rightshift_set
*******************************************************************************
	Set the right bit shift.

	HAL does not provide a function for this: it must be done in the call to HAL_DFSDM_ChannelInit.

	HAL documentation states:
	DFSDM_Channel_InitTypeDef.RightBitShift: DFSDM channel right bit shift. This parameter must be a number between Min_Data = 0x00 and Max_Data = 0x1F
	Configure the output clock, input, serial interface, analog watchdog, offset and data right bit shift parameters for this channel using the HAL_DFSDM_ChannelInit() function.

	DFSDM_CHyCFGR2: channel y configuration register
	Bits 7:3 DTRBS[4:0]: Data right bit-shift for channel y
		0-31: Defines the shift of the data result coming from the integrator - how many bit shifts to the right
		will be performed to have final results. Bit-shift is performed before offset correction. The data shift is
		rounding the result to nearest integer value. The sign of shifted result is maintained (to have valid
		24-bit signed format of result data).
		This value can be modified only when CHEN=0 (in DFSDM_CHyCFGR1 register).


	Parameters:
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT or STM_DFSDM_STEREO. In stereo mode, both channels are set the same offset. To set individual channel offsets, call this function twice for left and right.
		shift		-	shift

	Returns:

******************************************************************************/
void stm_dfsdm_rightshift_set(unsigned char left_right,int shift)
{
	assert(left_right<3);

	unsigned r;
	// Sanity on shift: 5 bits only
	shift&=0b11111;

	if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
	{
		// Left is channel 5
		// Must disable channel to change shift
		r = DFSDM1_Channel5->CHCFGR1;
		//fprintf(file_pri,"CHCFGR1 Before: %08X\n",DFSDM1_Channel5->CHCFGR1);
		r&=~(0b10000000);
		DFSDM1_Channel5->CHCFGR1=r;
		//fprintf(file_pri,"CHCFGR After: %08X\n",DFSDM1_Channel5->CHCFGR1);

		//unsigned r = DFSDM_CH5CFGR2;
		r = DFSDM1_Channel5->CHCFGR2;
		// Preserve offset
		r&=0xffffff00;
		// Insert shift
		r|=shift<<3;
		// Update reg
		DFSDM1_Channel5->CHCFGR2 = r;
		//char * s = dfsdm_chcfgr2(r);
		//fprintf(file_pri,"Setting CHCFGR2: %08X %s\n",r,s);
		//s = dfsdm_chcfgr2(DFSDM1_Channel5->CHCFGR2);
		//fprintf(file_pri,"Verifying CHCFGR2: %08X %s\n",DFSDM1_Channel5->CHCFGR2,s);

		// Reactivate channel
		r = DFSDM1_Channel5->CHCFGR1;
		//fprintf(file_pri,"CHCFGR1 Before: %08X\n",DFSDM1_Channel5->CHCFGR1);
		r|=0b10000000;
		DFSDM1_Channel5->CHCFGR1=r;
		//fprintf(file_pri,"CHCFGR After: %08X\n",DFSDM1_Channel5->CHCFGR1);
	}
	if(left_right==STM_DFSDM_RIGHT|| left_right==STM_DFSDM_STEREO)
	{
		// Left is channel 6
		// Must disable channel to change shift
		r = DFSDM1_Channel6->CHCFGR1;
		//fprintf(file_pri,"CHCFGR1 Before: %08X\n",DFSDM1_Channel6->CHCFGR1);
		r&=~(0b10000000);
		DFSDM1_Channel6->CHCFGR1=r;
		//fprintf(file_pri,"CHCFGR After: %08X\n",DFSDM1_Channel6->CHCFGR1);

		r = DFSDM1_Channel6->CHCFGR2;
		// Preserve offset
		r&=0xffffff00;
		// Insert shift
		r|=shift<<3;
		// Update reg
		DFSDM1_Channel6->CHCFGR2 = r;
		//char * s = dfsdm_chcfgr2(r);
		//fprintf(file_pri,"Setting CHCFGR2: %08X %s\n",r,s);
		//s = dfsdm_chcfgr2(DFSDM1_Channel6->CHCFGR2);
		//fprintf(file_pri,"Verifying CHCFGR2: %08X %s\n",DFSDM1_Channel6->CHCFGR2,s);

		// Reactivate channel
		r = DFSDM1_Channel6->CHCFGR1;
		//fprintf(file_pri,"CHCFGR1 Before: %08X\n",DFSDM1_Channel6->CHCFGR1);
		r|=0b10000000;
		DFSDM1_Channel6->CHCFGR1=r;
		//fprintf(file_pri,"CHCFGR After: %08X\n",DFSDM1_Channel6->CHCFGR1);
	}
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
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT or STM_DFSMD_STEREO
		order		-	Value from 1 to 5 inclusive specifying the sinc filter order
		fosr		-	filter oversample ration (downsampling)
		div			-	sysclk divider which generates the microphone clock

	Returns:

******************************************************************************/
void stm_dfsdm_initsampling(unsigned char left_right,unsigned order,unsigned fosr,unsigned iosr,unsigned div)
{
	HAL_StatusTypeDef s;

	fprintf(file_pri,"\tFilter order %d; oversampling ratio: %d; integral oversampling: %d; system clock divider: %d\n",order,fosr,iosr,div);

	/*fprintf(file_pri,"====================BEFORE OFF====================\n");
	stm_dfsdm_printreg();*/


	// Stop continuous conversion
	s = HAL_DFSDM_FilterConfigRegChannel(_stm_dfsdm_filters[0], DFSDM_CHANNEL_5, DFSDM_CONTINUOUS_CONV_OFF);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"Error stopping regular conversion left. %d\n",s);
	}
	s = HAL_DFSDM_FilterConfigRegChannel(_stm_dfsdm_filters[1], DFSDM_CHANNEL_6, DFSDM_CONTINUOUS_CONV_OFF);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"Error stopping regular conversion right. %d\n",s);
	}

	/*fprintf(file_pri,"====================AFTER OFF====================\n");
	stm_dfsdm_printreg();*/

	// Initialise the left or right
	if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
	{
		_stm_dfsdm_initsampling_internal(STM_DFSDM_LEFT,order,fosr,iosr,div);
	}

	if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
	{
		_stm_dfsdm_initsampling_internal(STM_DFSDM_RIGHT,order,fosr,iosr,div);
	}


	/*fprintf(file_pri,"====================AFTER INIT====================\n");
	stm_dfsdm_printreg();*/





}
/******************************************************************************
	function: _stm_dfsdm_initsampling_internal
*******************************************************************************
	Set the parameter for the channel - handles only left or right channel.
	Must be called twice for stereo

	Parameters:

	Returns:

******************************************************************************/
void _stm_dfsdm_initsampling_internal(unsigned char left_right,unsigned order,unsigned fosr,unsigned iosr,unsigned div)
{
	HAL_StatusTypeDef s;

	assert(left_right<2);
	//fprintf(file_pri,"_stm_dfsdm_initsampling_internal: filter order %d, oversampling %d and clock divider %d\n",order,fosr,div);


	// Print registers



	DFSDM_Filter_HandleTypeDef *filter = _stm_dfsdm_filters[left_right];
	uint32_t channel;
	if(left_right==STM_DFSDM_LEFT)
		channel = DFSDM_CHANNEL_5;
	if(left_right==STM_DFSDM_RIGHT)
		channel = DFSDM_CHANNEL_6;


	// Hack to reset filter - using HAL_DFSDM_FilterDeInit leads to issues when re-initting
	filter->Instance->FLTCR1 &= ~(DFSDM_FLTCR1_DFEN);
	filter->State = HAL_DFSDM_FILTER_STATE_RESET;

	//s = HAL_DFSDM_ChannelDeInit(&hdfsdm1_channel5);
	//fprintf(file_pri,"Channel deinit: %d\n",s);


	//HAL_Delay(100);


	//MX_DFSDM1_Init();


	/*fs = HAL_DFSDM_FilterGetState(&hdfsdm1_filter0);
	fprintf(file_pri,"Filter state: %d\n",fs);

	cs = HAL_DFSDM_ChannelGetState(&hdfsdm1_channel5);
	fprintf(file_pri,"Channel state: %d\n",cs);*/


	if(left_right==STM_DFSDM_LEFT)
		filter->Instance = DFSDM1_Filter0;					// DAN 27.07.2020 - is this needed? MX_DFSDM1_Init already does this
	if(left_right==STM_DFSDM_RIGHT)
		filter->Instance = DFSDM1_Filter1;					// DAN 27.07.2020 - is this needed? MX_DFSDM1_Init already does this
	if(left_right==STM_DFSDM_LEFT)
		filter->Init.RegularParam.Trigger = DFSDM_FILTER_SW_TRIGGER;
	else
		//filter->Init.RegularParam.Trigger = DFSDM_FILTER_SYNC_TRIGGER;		// Does not work
		filter->Init.RegularParam.Trigger = DFSDM_FILTER_SW_TRIGGER;
	filter->Init.RegularParam.FastMode = DISABLE;
	filter->Init.RegularParam.DmaMode = ENABLE;
	switch(order)
	{
		case 1:
			filter->Init.FilterParam.SincOrder = DFSDM_FILTER_SINC1_ORDER;
			break;
		case 2:
			filter->Init.FilterParam.SincOrder = DFSDM_FILTER_SINC2_ORDER;
			break;
		case 3:
			filter->Init.FilterParam.SincOrder = DFSDM_FILTER_SINC3_ORDER;
			break;
		case 4:
			filter->Init.FilterParam.SincOrder = DFSDM_FILTER_SINC4_ORDER;
			break;
		case 5:
		default:
			filter->Init.FilterParam.SincOrder = DFSDM_FILTER_SINC5_ORDER;
			break;
	}
	filter->Init.FilterParam.Oversampling = fosr;
	filter->Init.FilterParam.IntOversampling = iosr;
	s = HAL_DFSDM_FilterInit(filter);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"\tError initialising filter %d\n",s);
	}
	//else
		//fprintf(file_pri,"Filter init ok\n");

	// Manually change divider - the divider is stored in a register of DFSDM1_Channel0, regardless of which channel is actually used.
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

	s = HAL_DFSDM_FilterConfigRegChannel(filter, channel, DFSDM_CONTINUOUS_CONV_ON);
	if(s != HAL_OK)
	{
		fprintf(file_pri,"\tError configuring channel regular conversion: %d\n",s);
	}
	//else
		//fprintf(file_pri,"\tHAL_DFSDM_FilterConfigRegChannel ok\n");
}
/******************************************************************************
	function: stm_dfsdm_stat_clear
*******************************************************************************
	Clear audio frame acquisition statistics.

	Parameters:

	Returns:

******************************************************************************/
void stm_dfsdm_stat_clear()
{
	_stm_dfsmd_ctr_acq[0]=_stm_dfsmd_ctr_acq[1]=_stm_dfsmd_ctr_loss=0;
}
unsigned long stm_dfsdm_stat_totframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_dfsmd_ctr_acq[0]+_stm_dfsmd_ctr_acq[1];
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
	function: stm_dfsdm_acq_poll_internal_old
*******************************************************************************
	Polling data acquisition from internal microphone.

	Assumes filter 0 is configured with data coming from DATIN6 on the rising edge.

	Parameters:
		left_right	-		Can be STM_DFSMD_LEFT or STM_DFSMD_RIGHT (STM_DFSMD_STEREO is not valid here)
		buffer		-	Buffer which receives the data
		n			-	Number of samples to acquire

	Returns:
		Data acquisition time in us
******************************************************************************/
#if 0
unsigned long stm_dfsdm_acq_poll_internal_old(unsigned char left_right,int *buffer,unsigned n)
{
	fprintf(file_pri,"stm_dfsdm_acq_poll_internal left_right %d buffer %p n: %d\n",left_right,buffer,n);

	unsigned long t1 = timer_us_get();
	HAL_StatusTypeDef s;
	for(unsigned i=0;i<n;i++)
	{
		s = HAL_DFSDM_FilterPollForRegConversion(_stm_dfsdm_filters[left_right],1000);
		if(s!=HAL_OK)
			fprintf(file_pri,"HAL_DFSDM_FilterPollForRegConversion error %d\n",s);
		unsigned int c;
		int v = HAL_DFSDM_FilterGetRegularValue(_stm_dfsdm_filters[left_right],(uint32_t*)&c);
		buffer[i] = v;

	}
	unsigned long t2 = timer_us_get();
	return t2-t1;
}
#endif
/******************************************************************************
	function: stm_dfsdm_acq_poll_internal
*******************************************************************************
	Polling data acquisition from internal microphone.

	Assumes filter 0 is configured with data coming from DATIN6 on the rising edge.

	Parameters:
		left_right	-	Can be STM_DFSMD_LEFT, STM_DFSMD_RIGHT, or STM_DFSMD_STEREO
		buffer		-	Buffer which receives the data
		n			-	Number of samples to acquire. In Stereo mode, n/2 samples from each left and right microphones are acquired and interleaved in the buffer: l0|r0|l1|r1|...

	Returns:
		Data acquisition time in us
******************************************************************************/
unsigned long stm_dfsdm_acq_poll_internal(unsigned char left_right,int *buffer,unsigned n)
{
	//fprintf(file_pri,"stm_dfsdm_acq_poll_internal left_right %d buffer %p n: %d\n",left_right,buffer,n);

	unsigned long t1 = timer_us_get();
	HAL_StatusTypeDef s;
	//unsigned timeout = 10;
	unsigned timeout = 1000;

	unsigned inc=1;							// Increment for loop
	if(left_right==STM_DFSDM_STEREO)		// Stereo mode - acquire half the samples
		inc=2;

	// Clear buffer
	memset(buffer,0x55,n*sizeof(int));

	for(unsigned i=0;i<n;i+=inc)
	{
		// Poll until conversion is complete on both channels
		if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
		{
			s = HAL_DFSDM_FilterPollForRegConversion(_stm_dfsdm_filters[STM_DFSDM_LEFT],timeout);
			if(s!=HAL_OK)
			{
				fprintf(file_pri,"stm_dfsdm_acq_poll_internal: left: HAL_DFSDM_FilterPollForRegConversion error %d\n",s);
				return 0;	// In case of error return instead of trying to acquire the next samples which will also fail
			}
		}
		if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
		{
			s = HAL_DFSDM_FilterPollForRegConversion(_stm_dfsdm_filters[STM_DFSDM_RIGHT],timeout);
			if(s!=HAL_OK)
			{
				fprintf(file_pri,"stm_dfsdm_acq_poll_internal: right: HAL_DFSDM_FilterPollForRegConversion error %d\n",s);
				return 0;	// In case of error return instead of trying to acquire the next samples which will also fail
			}
		}

		if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
		{
			unsigned int c;
			int v = HAL_DFSDM_FilterGetRegularValue(_stm_dfsdm_filters[STM_DFSDM_LEFT],(uint32_t*)&c);
			buffer[i] = v;
		}
		if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
		{
			unsigned int c;
			int v = HAL_DFSDM_FilterGetRegularValue(_stm_dfsdm_filters[STM_DFSDM_RIGHT],(uint32_t*)&c);
			// Handle the interleaving of data - if stereo buffer[i] has left; buffer[i+1] has right; and i is incremented by 2
			if(left_right==STM_DFSDM_RIGHT)
				buffer[i] = v;
			else
				buffer[i+1] = v;
		}
	}
	unsigned long t2 = timer_us_get();
	return t2-t1;
}
unsigned long stm_dfsdm_acq_poll_internal_t(unsigned char left_right,int *buffer,unsigned *buffert,unsigned n)
{
	//fprintf(file_pri,"stm_dfsdm_acq_poll_internal left_right %d buffer %p n: %d\n",left_right,buffer,n);

	unsigned long t1 = timer_us_get();
	HAL_StatusTypeDef s;
	unsigned timeout = 10;

	unsigned inc=1;							// Increment for loop
	if(left_right==STM_DFSDM_STEREO)		// Stereo mode - acquire half the samples
		inc=2;

	// Clear buffer
	memset(buffer,0x55,n*sizeof(int));
	memset(buffert,0x55,n*sizeof(unsigned));

	for(unsigned i=0;i<n;i+=inc)
	{
		// Poll until conversion is complete on both channels
		if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
		{
			s = HAL_DFSDM_FilterPollForRegConversion(_stm_dfsdm_filters[STM_DFSDM_LEFT],timeout);
			if(s!=HAL_OK)
			{
				fprintf(file_pri,"stm_dfsdm_acq_poll_internal: left: HAL_DFSDM_FilterPollForRegConversion error %d\n",s);
				//return 0;	// In case of error return instead of trying to acquire the next samples which will also fail
			}

		}
		if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
		{
			s = HAL_DFSDM_FilterPollForRegConversion(_stm_dfsdm_filters[STM_DFSDM_RIGHT],timeout);
			if(s!=HAL_OK)
			{
				fprintf(file_pri,"stm_dfsdm_acq_poll_internal: right: HAL_DFSDM_FilterPollForRegConversion error %d\n",s);
				//return 0;	// In case of error return instead of trying to acquire the next samples which will also fail
			}

		}



		if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
		{
			unsigned int c;
			int v = HAL_DFSDM_FilterGetRegularValue(_stm_dfsdm_filters[STM_DFSDM_LEFT],(uint32_t*)&c);
			buffer[i] = v;

			unsigned t = HAL_DFSDM_FilterGetConvTimeValue(_stm_dfsdm_filters[STM_DFSDM_LEFT]);
			//fprintf(file_pri,"%08X\n",t);
			buffert[i] = t;

		}
		if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
		{
			unsigned int c;
			int v = HAL_DFSDM_FilterGetRegularValue(_stm_dfsdm_filters[STM_DFSDM_RIGHT],(uint32_t*)&c);
			// Handle the interleaving of data - if stereo buffer[i] has left; buffer[i+1] has right; and i is incremented by 2
			if(left_right==STM_DFSDM_RIGHT)
			{
				buffer[i] = v;
				buffert[i] = HAL_DFSDM_FilterGetConvTimeValue(_stm_dfsdm_filters[STM_DFSDM_RIGHT]);
			}
			else
			{
				buffer[i+1] = v;
				buffert[i+1] = HAL_DFSDM_FilterGetConvTimeValue(_stm_dfsdm_filters[STM_DFSDM_RIGHT]);
			}
		}
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
		left_right	-		Pointer to a variable that will receive whether the frame is from the left (0) or right (1) microphone

	Returns:
		0	-	Success
		1	-	Error (no data available in the buffer)
*******************************************************************************/
unsigned char stm_dfsdm_data_getnext(STM_DFSMD_TYPE *buffer,unsigned long *timems,unsigned long *pktctr,unsigned char *left_right)
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
		if(left_right)
			*left_right = _stm_dfsmd_buffers_leftright[_stm_dfsmd_buffer_rdptr];
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

	The function pushes stereo data on a unique circular buffer

	Do not call from user code.

	Parameters:
		buffer		-		Pointer containing the new audio frame to store in the frame buffer
		timems		-		Time in millisecond when the callback received the data
		left_right	-		0 (left) or right (1) microphone

	Returns:
*******************************************************************************/
void _stm_dfsdm_data_storenext(int *buffer,unsigned long timems,unsigned char left_right)
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
		// debug
		//fprintf(file_pri,"DMA %08X %08X %08X %08X\n",buffer[0], buffer[1],buffer[2],buffer[3]);
		for(unsigned i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
		{
			// Shift right to fit in signed short
			//_stm_dfsmd_buffers[_stm_dfsmd_buffer_wrptr][i] = buffer[i]>>(_stm_dfsdm_rightshift+8);				// 8 LSB are channel number - software rightshift
			_stm_dfsmd_buffers[_stm_dfsmd_buffer_wrptr][i] = buffer[i]>>(8);				// 8 LSB are channel number - hardware rightshift
		}
		// Store time
		_stm_dfsmd_buffers_time[_stm_dfsmd_buffer_wrptr] = timems;
		// Store packet counter
		_stm_dfsmd_buffers_pktnum[_stm_dfsmd_buffer_wrptr] = _stm_dfsmd_ctr_acq[left_right];
		// Store left/right indication
		_stm_dfsmd_buffers_leftright[_stm_dfsmd_buffer_wrptr] = left_right;

		// Increment the write pointer
		_stm_dfsmd_buffer_wrptr = (_stm_dfsmd_buffer_wrptr+1)&STM_DFSMD_BUFFER_MASK;
		// Increment the frame counter
		_stm_dfsmd_ctr_acq[left_right]++;
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
		stm_dfsdm_stat_clear();
	}
}
void stm_dfsdm_stat_print(void)
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
			unsigned char rv = stm_dfsdm_data_getnext(tmp,0,0,0);
			fprintf(file_pri,"\tgetnext: %d. Data[0]: %d\n",rv,tmp[0]);
		}
		else
		{
			tmp[0] = cmd[i*2+1];
#warning must fix tmp data type
			_stm_dfsdm_data_storenext(tmp,0,0);
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
	function: _stm_dfsdm_offset_calibrate
*******************************************************************************
	Measure the microphone offset and set the offset.

	This function must be called with polling acquisition enabled with
	stm_dfsdm_acquire_start_poll.

	Parameters:
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT, STM_DFSMD_STEREO. This parameter is unused if mode is STM_DFSMD_INIT_OFF.
		print		-	Nonzero to print info

	Returns:

******************************************************************************/
void _stm_dfsdm_offset_calibrate(unsigned char left_right,unsigned char print)
{
	// Measure offset and print
	int offset_l,offset_r;

	// This resets the offset left/right/stereo as appropriate
	stm_dfsdm_offset_set(left_right,0);

	_stm_dfsdm_offset_measure(left_right,&offset_l,&offset_r,print);

	fprintf(file_pri,"\tSetting offset\n");
	// Call set offset twice to set independent offsets for left and right
	if(left_right == STM_DFSDM_LEFT || left_right == STM_DFSDM_STEREO)
		stm_dfsdm_offset_set(STM_DFSDM_LEFT,offset_l);
	if(left_right == STM_DFSDM_RIGHT || left_right == STM_DFSDM_STEREO)
			stm_dfsdm_offset_set(STM_DFSDM_RIGHT,offset_r);

	// Measure offset again and print
	_stm_dfsdm_offset_measure(left_right,&offset_l,&offset_r,print);
}
/******************************************************************************
	function: _stm_dfsdm_offset_measure
*******************************************************************************
	Measure the microphone offset.

	This function must be called with polling acquisition enabled with
	stm_dfsdm_acquire_start_poll.

	Parameters:
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT, STM_DFSMD_STEREO. This parameter is unused if mode is STM_DFSMD_INIT_OFF.
		offset_l	-	Left offset. This won't be modified if the measurement is only of the right microphone.
		offset_r	-	Right offset. This won't be modified if the measurement is only of the left microphone.
		print		-	Nonzero to print info

	Returns:

******************************************************************************/
int _stm_dfsdm_offset_measure(unsigned char left_right,int *offset_l,int *offset_r,unsigned char print)
{
	unsigned maxsample=8000;
	int data[maxsample];

	//fprintf(file_pri,"stm_dfsdm_calib_zero_internal %d\n",left_right);

	if(print)
		fprintf(file_pri,"\tMeasuring microphone offset: ");

	// Acquire bit of data to clear the DFSDM buffer
	stm_dfsdm_acq_poll_internal(left_right,data,2000);

	unsigned long dt = stm_dfsdm_acq_poll_internal(left_right,data,maxsample);
	(void) dt;

	// Calculate the mean alternating left/right - mono is addressed later
	long long mean_l = 0,mean_r=0;
	for(unsigned i=0;i<maxsample;i+=2)
	{
		mean_l+=data[i];
		mean_r+=data[i+1];
	}
	if(left_right==STM_DFSDM_STEREO)
	{
		mean_l/=(long long)(maxsample/2);
		mean_r/=(long long)(maxsample/2);
		if(offset_l)
			*offset_l = mean_l;
		if(offset_r)
			*offset_r = mean_r;
	}
	else
	{
		// Mono: combine the sum mean_l and mean_r;
		mean_l+=mean_r;					// mean_l is the overall mean
		mean_l/=(long long)maxsample;
		mean_r = mean_l;				// To simplify printing below
		if(left_right==STM_DFSDM_LEFT)
		{
			if(offset_l)
				*offset_l = mean_l;
		}
		if(left_right==STM_DFSDM_RIGHT)
		{
			if(offset_r)
				*offset_r = mean_l;
		}
	}
	if(print)
	{
		if(left_right == STM_DFSDM_LEFT || left_right == STM_DFSDM_STEREO)
			fprintf(file_pri,"left: %ld ",(long)mean_l);
		if(left_right == STM_DFSDM_RIGHT || left_right == STM_DFSDM_STEREO)
			fprintf(file_pri,"right: %ld ",(long)mean_r);
		fprintf(file_pri,"\n");
	}

	return 0;
}

/******************************************************************************
	function: stm_dfsdm_acquire_start_dma
*******************************************************************************
	Turns on audio acquisition via DMA and framebuffers.

	Parameters:
		left_right	-	One of STM_DFSMD_LEFT, STM_DFSMD_RIGHT, STM_DFSMD_STEREO
	Returns:
		Offset
******************************************************************************/
int stm_dfsdm_acquire_start_dma(unsigned char left_right)
{
	HAL_StatusTypeDef s1=HAL_OK;

	//fprintf(file_pri,"stm_dfsdm_acquire_dma_start: left_right=%d\n",left_right);

	//stm_dfsdm_state_print();

	if(left_right==STM_DFSDM_LEFT || left_right==STM_DFSDM_STEREO)
	{
		s1 = HAL_DFSDM_FilterRegularStart_DMA(_stm_dfsdm_filters[0],(int32_t*)_stm_dfsdm_dmabuf,STM_DFSMD_BUFFER_SIZE*2);
		if(s1==0)
			;//fprintf(file_pri,"Audio frame acquisition by DMA left started\n");
		else
			fprintf(file_pri,"Audio frame acquisition by DMA left error: %d\n",s1);
	}


	if(left_right==STM_DFSDM_RIGHT || left_right==STM_DFSDM_STEREO)
	{
		s1 = HAL_DFSDM_FilterRegularStart_DMA(_stm_dfsdm_filters[1],(int32_t*)_stm_dfsdm_dmabuf2,STM_DFSMD_BUFFER_SIZE*2);		// Need a different DMA buffer
		if(s1==0)
			;//fprintf(file_pri,"Audio frame acquisition by DMA right started\n");
		else
			fprintf(file_pri,"Audio frame acquisition by DMA right error: %d\n",s1);
	}

	//stm_dfsdm_state_print();

	// Clear buffers & statistics
	stm_dfsdm_data_clear();
	return s1;
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
#if 0
	fprintf(file_pri,"Full CB: ");
	for(int i=0;i<4;i++)
	{
		fprintf(file_pri,"%08X ",_stm_dfsdm_dmabuf[STM_DFSMD_BUFFER_SIZE+i]);
	}
	fprintf(file_pri," -  ");
	for(int i=0;i<4;i++)
	{
		fprintf(file_pri,"%08X ",_stm_dfsdm_dmabuf2[STM_DFSMD_BUFFER_SIZE+i]);
	}
	fprintf(file_pri,"\n");

	// Identify which is the filter and therefore microphone which triggered the callback
	fprintf(file_pri,"%p (%p %p)\n",hdfsdm_filter,_stm_dfsdm_filters[0],_stm_dfsdm_filters[1]);
#endif

	//fprintf(file_pri,"%c\n",hdfsdm_filter==_stm_dfsdm_filters[0]?'L':'R');

	if(hdfsdm_filter == _stm_dfsdm_filters[0])
		_stm_dfsdm_data_storenext(_stm_dfsdm_dmabuf+STM_DFSMD_BUFFER_SIZE,timer_ms_get(),0);		// Left mic
	else
		_stm_dfsdm_data_storenext(_stm_dfsdm_dmabuf2+STM_DFSMD_BUFFER_SIZE,timer_ms_get(),1);		// Right mic

#if 0
	for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
	{
		_stm_dfsdm_dmabuf[STM_DFSMD_BUFFER_SIZE+i]=0x41;
		_stm_dfsdm_dmabuf2[STM_DFSMD_BUFFER_SIZE+i]=0x42;
	}
#endif
}
void HAL_DFSDM_FilterRegConvHalfCpltCallback (DFSDM_Filter_HandleTypeDef *hdfsdm_filter)
{
#if 0
	fprintf(file_pri,"Half CB: ");
	for(int i=0;i<4;i++)
	{
		fprintf(file_pri,"%08X ",_stm_dfsdm_dmabuf[i]);
	}
	fprintf(file_pri," -  ");
	for(int i=0;i<4;i++)
	{
		fprintf(file_pri,"%08X ",_stm_dfsdm_dmabuf2[i]);
	}
	fprintf(file_pri,"\n");

	// Identify which is the filter and therefore microphone which triggered the callback
	fprintf(file_pri,"%p (%p %p)\n",hdfsdm_filter,_stm_dfsdm_filters[0],_stm_dfsdm_filters[1]);
#endif

	//for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i+=4)fprintf(file_pri,"%c\n",hdfsdm_filter==_stm_dfsdm_filters[0]?'L':'R');

	// Copy data into next buffer
	if(hdfsdm_filter == _stm_dfsdm_filters[0])
		_stm_dfsdm_data_storenext(_stm_dfsdm_dmabuf,timer_ms_get(),0);		// Left mic
	else
		_stm_dfsdm_data_storenext(_stm_dfsdm_dmabuf2,timer_ms_get(),1);		// Right mic

#if 0
	// Debug clear
	for(int i=0;i<STM_DFSMD_BUFFER_SIZE;i++)
	{
		_stm_dfsdm_dmabuf[i]=0x31;
		_stm_dfsdm_dmabuf2[i]=0x32;
	}
#endif
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
	unsigned char left_right;

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

		while(!stm_dfsdm_data_getnext(audio,&audioms,&audiopkt,&left_right))
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

/******************************************************************************
	function: stm_dfsdm_acquire_stop_all
*******************************************************************************
	Stop all internal and polling data acquisition.

	Parameters:
		-

	Returns:
		-
*******************************************************************************/
void stm_dfsdm_acquire_stop_all()
{
	//fprintf(file_pri,"stm_dfsdm_acquire_stop_all: ");
	HAL_StatusTypeDef s;
	for(unsigned i=0;i<2;i++)
	{
		s=HAL_DFSDM_FilterRegularStop_DMA(_stm_dfsdm_filters[i]);
		/*if(s==HAL_OK)
			fprintf(file_pri,"DMA %d OK; ",i);
		else
			fprintf(file_pri,"DMA %d KO; ",i);*/
		s=HAL_DFSDM_FilterRegularStop(_stm_dfsdm_filters[i]);
		/*if(s==HAL_OK)
			fprintf(file_pri,"poll %d OK; ",i);
		else
			fprintf(file_pri,"poll %d KO; ",i);*/
	}
	//fprintf(file_pri,"\n");
	(void) s;
}
void stm_dfsdm_acquire_stop_poll()
{
	//fprintf(file_pri,"stm_dfsdm_acquire_stop_poll: ");
	HAL_StatusTypeDef s;
	for(unsigned i=0;i<2;i++)
	{
		/*s=HAL_DFSDM_FilterRegularStop_DMA(_stm_dfsdm_filters[i]);
		if(s==HAL_OK)
			fprintf(file_pri,"DMA %d OK; ",i);
		else
			fprintf(file_pri,"DMA %d KO; ",i);*/
		s=HAL_DFSDM_FilterRegularStop(_stm_dfsdm_filters[i]);
		/*if(s==HAL_OK)
			fprintf(file_pri,"poll %d OK; ",i);
		else
			fprintf(file_pri,"poll %d KO; ",i);*/
	}
	(void) s;
	fprintf(file_pri,"\n");
}

void stm_dfsdm_state_print()
{
	fprintf(file_pri,"Channel states:\n");
	for(int i=0;i<2;i++)
		fprintf(file_pri,"\tChannel %d: %d\n",i,HAL_DFSDM_ChannelGetState(_stm_dfsdm_channels[i]));
	fprintf(file_pri,"Filter states:\n");
	for(int i=0;i<2;i++)
		fprintf(file_pri,"\tFilter %d: %d\n",i,HAL_DFSDM_FilterGetState(_stm_dfsdm_filters[i]));

}
/******************************************************************************
	function: stm_dfsdm_memoryused
*******************************************************************************
	Returns how much memory is used in the buffers of the DFSDM library.

	This only accounts for memory used which can increase or decrease
	when buffering parameters (e.g. number of buffers, buffer size) is modified.

	Parameters:
		-

	Returns:
		Memory used
*******************************************************************************/
unsigned stm_dfsdm_memoryused()
{
	/*fprintf(file_pri,"%d\n",sizeof(_stm_dfsmd_buffers));
	fprintf(file_pri,"%d\n",sizeof(_stm_dfsmd_buffers_time));
	fprintf(file_pri,"%d\n",sizeof(_stm_dfsmd_buffers_pktnum));
	fprintf(file_pri,"%d\n",sizeof(_stm_dfsdm_dmabuf));
	fprintf(file_pri,"%d\n",sizeof(_stm_dfsdm_dmabuf2));*/

	return sizeof(_stm_dfsmd_buffers)+sizeof(_stm_dfsmd_buffers_time)+sizeof(_stm_dfsmd_buffers_pktnum)+sizeof(_stm_dfsdm_dmabuf)+sizeof(_stm_dfsdm_dmabuf2)+sizeof(_stm_dfsmd_buffers_leftright);
}

void stm_dfsdm_printreg()
{
	DFSDM_Channel_TypeDef *channels[8]={	DFSDM1_Channel0,DFSDM1_Channel1,DFSDM1_Channel2,DFSDM1_Channel3,
											DFSDM1_Channel4,DFSDM1_Channel5,DFSDM1_Channel6,DFSDM1_Channel7};
	for(int i=0;i<8;i++)
	{
		fprintf(file_pri,"Channel %d (%p)\n",i,channels[i]);
		fprintf(file_pri,"\tCHCFGR1: %08X %s\n",(unsigned)channels[i]->CHCFGR1,dfsdm_chcfgr1(channels[i]->CHCFGR1));
		fprintf(file_pri,"\tCHCFGR2: %08X %s\n",(unsigned)channels[i]->CHCFGR2,dfsdm_chcfgr2(channels[i]->CHCFGR2));
		fprintf(file_pri,"\tCHAWSCDR: %08X\n",(unsigned)channels[i]->CHAWSCDR);
		fprintf(file_pri,"\tCHWDATAR: %08X\n",(unsigned)channels[i]->CHWDATAR);
		fprintf(file_pri,"\tCHDATINR: %08X\n",(unsigned)channels[i]->CHDATINR);
	}

	DFSDM_Filter_TypeDef *filters[4]={	DFSDM1_Filter0,DFSDM1_Filter1,DFSDM1_Filter2,DFSDM1_Filter3};
	for(int i=0;i<4;i++)
	{
		fprintf(file_pri,"Filter %d (%p)\n",i,filters[i]);
		fprintf(file_pri,"\tFLTCR1: %08X FLTCR2: %08X\n",(unsigned)filters[i]->FLTCR1,(unsigned)filters[i]->FLTCR2);
		fprintf(file_pri,"\tFLTISR: %08X FLTICR: %08X\n",(unsigned)filters[i]->FLTISR,(unsigned)filters[i]->FLTICR);
		fprintf(file_pri,"\tFLTJCHGR: %08X FLTFCR: %08X\n",(unsigned)filters[i]->FLTJCHGR,(unsigned)filters[i]->FLTFCR);
		fprintf(file_pri,"\tFLTJDATAR: %08X FLTRDATAR: %08X\n",(unsigned)filters[i]->FLTJDATAR,(unsigned)filters[i]->FLTRDATAR);
		fprintf(file_pri,"\tFLTAWHTR: %08X FLTAWLTR: %08X\n",(unsigned)filters[i]->FLTAWHTR,(unsigned)filters[i]->FLTAWLTR);
		fprintf(file_pri,"\tFLTAWSR: %08X FLTAWCFR: %08X\n",(unsigned)filters[i]->FLTAWSR,(unsigned)filters[i]->FLTAWCFR);
		fprintf(file_pri,"\tFLTEXMAX: %08X FLTEXMIN: %08X\n",(unsigned)filters[i]->FLTEXMAX,(unsigned)filters[i]->FLTEXMIN);
		fprintf(file_pri,"\tFLTCNVTIMR: %08X\n",(unsigned)filters[i]->FLTCNVTIMR);
	}
}

/******************************************************************************
	function: _stm_dfsdm_sampling_sync
*******************************************************************************
	Tentative hack to synchronise two filters.

	Must be called immediately after DMA acquisition started.

	This does not seem


	Parameters:
		-

	Returns:
		-
*******************************************************************************/
void _stm_dfsdm_sampling_sync()
{
	// Hack to synchronise sampling of left and right channels

	// Global enable: DFSDMEN=1 in the DFSDM_CH0CFGR1
	// Tentative procedure: global channel disable; clear dfen; restore dfen; global enable

	unsigned gr;
	unsigned c0,c1;

	// Understand which filters are running: if running ISR has RCIP (0x4000) set.
	c0 = DFSDM1_Filter0->FLTISR;
	c1 = DFSDM1_Filter1->FLTISR;
	//fprintf(file_pri,"Filter 0 FLTISR: %08X\n",c0);
	//fprintf(file_pri,"Filter 1 FLTISR: %08X\n",c1);

	// Global disable of the DSFSDM peripheral
	gr = DFSDM1_Channel0->CHCFGR1;	// Initial state
	//fprintf(file_pri,"DFSDM_CH0CFGR1 pre clear: %08X\n",gr);
	DFSDM1_Channel0->CHCFGR1 = gr&0x7fffffff;		// Clear global enable
	//fprintf(file_pri,"DFSDM_CH0CFGR1 post clear: %08X\n",DFSDM1_Channel0->CHCFGR1);

	// Save filter DFEN bit and clear DFEN
	unsigned f0,f1;
	f0 = DFSDM1_Filter0->FLTCR1;
	f1 = DFSDM1_Filter1->FLTCR1;
	//fprintf(file_pri,"Filter 0 FLTCR1 pre clear: %08X\n",f0);
	//fprintf(file_pri,"Filter 1 FLTCR1 pre clear: %08X\n",f1);

	DFSDM1_Filter0->FLTCR1 = f0&0xfffffffe;		// Clear DFEN
	DFSDM1_Filter1->FLTCR1 = f1&0xfffffffe;		// Clear DFEN
	//fprintf(file_pri,"Filter 0 FLTCR1 post clear: %08X\n",DFSDM1_Filter0->FLTCR1);
	//fprintf(file_pri,"Filter 1 FLTCR1 post clear: %08X\n",DFSDM1_Filter1->FLTCR1);

	// Restore filter DFEN
	//DFSDM1_Filter1->FLTCR1 |= 0x80000;		// Set sync
	//DFSDM1_Filter1->FLTCR1 = f1|0x80000;

	DFSDM1_Filter0->FLTCR1 = f0;
	DFSDM1_Filter1->FLTCR1 = f1;

	//fprintf(file_pri,"Filter 0 FLTCR1 restored: %08X\n",DFSDM1_Filter0->FLTCR1);
	//fprintf(file_pri,"Filter 1 FLTCR1 restored: %08X\n",DFSDM1_Filter1->FLTCR1);


	// Restore DFSDFM global enable
	DFSDM1_Channel0->CHCFGR1 = gr;
	//fprintf(file_pri,"DFSDM_CH0CFGR1 restored: %08X\n",DFSDM1_Channel0->CHCFGR1);



	//fprintf(file_pri,"Filter 0 FLTISR: %08X\n",DFSDM1_Filter0->FLTISR);
	//fprintf(file_pri,"Filter 1 FLTISR: %08X\n",DFSDM1_Filter1->FLTISR);

	// Clearing DFEN stops acquisition, reactivate if the filter was initially activated
	if(c0&0x4000)
	{
		//fprintf(file_pri,"Activate flt0\n");
		DFSDM1_Filter0->FLTCR1 |= 0x20000;
	}
	if(c1&0x4000)
	{
		//fprintf(file_pri,"Activate flt1\n");


		DFSDM1_Filter1->FLTCR1 |= 0x20000;		// Set DFEN

		//DFSDM1_Filter1->FLTCR1 |= 0x20000 | 0x80000;		// Try sync
	}

	//fprintf(file_pri,"Filter 0 FLTISR: %08X\n",DFSDM1_Filter0->FLTISR);
	//fprintf(file_pri,"Filter 1 FLTISR: %08X\n",DFSDM1_Filter1->FLTISR);



	//fprintf(file_pri,"Filter 0 FLTISR: %08X\n",DFSDM1_Filter0->FLTISR);
	//fprintf(file_pri,"Filter 1 FLTISR: %08X\n",DFSDM1_Filter1->FLTISR);


}

/******************************************************************************
	function: stm_dfsdm_storevolumegain
*******************************************************************************
	Store the volume gain to EEPROM.
	The volume gain adjusts the default bit-right shift and can be used
	to increase senstivity to low volume, or increase tolerance to high volumes.


	Parameters:
		g:				Gain. Value from -2 to +2. 0 means default volume. +2 increases amplitude by a factor 4. -2 decreases amplitude by a factor 4.

	Returns:
		-
*******************************************************************************/
void stm_dfsdm_storevolumegain(signed char g)
{
	if(g>2) g=2;
	if(g<-2) g=-2;
	// Shift to positive range, as 0xFF is a reserved value in the EEPROM indicating an uninitialised cell.
	g+=2;
	eeprom_write_byte(CONFIG_ADDR_AUDIO_SETTINGS,(unsigned char)g);
}

/******************************************************************************
	function: stm_dfsdm_loadvolumegain
*******************************************************************************
	Load the volume gain from EEPROM.

	The volume gain adjusts the default bit-right shift and can be used
	to increase senstivity to low volume, or increase tolerance to high volumes.


	Parameters:
		-

	Returns:
		Gain. Value from -2 to +2. 0 means default volume. +2 increases amplitude by a factor 4. -2 decreases amplitude by a factor 4.
*******************************************************************************/
signed char stm_dfsdm_loadvolumegain()
{
	unsigned char gu = eeprom_read_byte(CONFIG_ADDR_AUDIO_SETTINGS,1,2);		// Default value is "2" which corresponds to vol of 0
	// Shift to positive-negative range
	signed char g = (signed char)gu;
	g-=2;

	if(g>2) g=2;
	if(g<-2) g=-2;
	return g;
}



