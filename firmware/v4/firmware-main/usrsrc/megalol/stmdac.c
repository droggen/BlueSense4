#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "dac.h"
#include "atomicop.h"
#include "usrmain.h"
#include "global.h"
#include "stmdac.h"
#include "wait.h"
#include "stm32l4xx_ll_tim.h"
#include "serial_itm.h"
#include "system-extra.h"
#include "stmrcc.h"

const unsigned short _stm_dac_cos_lut[STMDAC_LUTSIZ] = {4095, 4095, 4095, 4095, 4094, 4094, 4094, 4093, 4093, 4092, 4091, 4090, 4089, 4088, 4087, 4086, 4085, 4084, 4083, 4081, 4080, 4078, 4076, 4075, 4073, 4071, 4069, 4067, 4065, 4063, 4060, 4058, 4056, 4053, 4051, 4048, 4045, 4042, 4040, 4037, 4034, 4031, 4027, 4024, 4021, 4017, 4014, 4010, 4007, 4003, 3999, 3996, 3992, 3988, 3984, 3980, 3975, 3971, 3967, 3962, 3958, 3953, 3949, 3944, 3939, 3934, 3929, 3924, 3919, 3914, 3909, 3904, 3898, 3893, 3888, 3882, 3876, 3871, 3865, 3859, 3853, 3847, 3841, 3835, 3829, 3823, 3816, 3810, 3804, 3797, 3791, 3784, 3777, 3771, 3764, 3757, 3750, 3743, 3736, 3729, 3722, 3714, 3707, 3700, 3692, 3685, 3677, 3669, 3662, 3654, 3646, 3638, 3630, 3622, 3614, 3606, 3598, 3590, 3581, 3573, 3565, 3556, 3548, 3539, 3530, 3522, 3513, 3504, 3495, 3486, 3477, 3468, 3459, 3450, 3441, 3432, 3423, 3413, 3404, 3394, 3385, 3375, 3366, 3356, 3346, 3337, 3327, 3317, 3307, 3297, 3287, 3277, 3267, 3257, 3247, 3237, 3226, 3216, 3206, 3195, 3185, 3175, 3164, 3154, 3143, 3132, 3122, 3111, 3100, 3089, 3078, 3068, 3057, 3046, 3035, 3024, 3013, 3002, 2990, 2979, 2968, 2957, 2946, 2934, 2923, 2912, 2900, 2889, 2877, 2866, 2854, 2843, 2831, 2819, 2808, 2796, 2784, 2773, 2761, 2749, 2737, 2725, 2714, 2702, 2690, 2678, 2666, 2654, 2642, 2630, 2618, 2606, 2594, 2581, 2569, 2557, 2545, 2533, 2521, 2508, 2496, 2484, 2472, 2459, 2447, 2435, 2422, 2410, 2398, 2385, 2373, 2360, 2348, 2335, 2323, 2311, 2298, 2286, 2273, 2261, 2248, 2236, 2223, 2211, 2198, 2186, 2173, 2161, 2148, 2135, 2123, 2110, 2098, 2085, 2073, 2060, 2048, 2035, 2022, 2010, 1997, 1985, 1972, 1960, 1947, 1934, 1922, 1909, 1897, 1884, 1872, 1859, 1847, 1834, 1822, 1809, 1797, 1784, 1772, 1760, 1747, 1735, 1722, 1710, 1697, 1685, 1673, 1660, 1648, 1636, 1623, 1611, 1599, 1587, 1574, 1562, 1550, 1538, 1526, 1514, 1501, 1489, 1477, 1465, 1453, 1441, 1429, 1417, 1405, 1393, 1381, 1370, 1358, 1346, 1334, 1322, 1311, 1299, 1287, 1276, 1264, 1252, 1241, 1229, 1218, 1206, 1195, 1183, 1172, 1161, 1149, 1138, 1127, 1116, 1105, 1093, 1082, 1071, 1060, 1049, 1038, 1027, 1017, 1006, 995, 984, 973, 963, 952, 941, 931, 920, 910, 900, 889, 879, 869, 858, 848, 838, 828, 818, 808, 798, 788, 778, 768, 758, 749, 739, 729, 720, 710, 701, 691, 682, 672, 663, 654, 645, 636, 627, 618, 609, 600, 591, 582, 573, 565, 556, 547, 539, 530, 522, 514, 505, 497, 489, 481, 473, 465, 457, 449, 441, 433, 426, 418, 410, 403, 395, 388, 381, 373, 366, 359, 352, 345, 338, 331, 324, 318, 311, 304, 298, 291, 285, 279, 272, 266, 260, 254, 248, 242, 236, 230, 224, 219, 213, 207, 202, 197, 191, 186, 181, 176, 171, 166, 161, 156, 151, 146, 142, 137, 133, 128, 124, 120, 115, 111, 107, 103, 99, 96, 92, 88, 85, 81, 78, 74, 71, 68, 64, 61, 58, 55, 53, 50, 47, 44, 42, 39, 37, 35, 32, 30, 28, 26, 24, 22, 20, 19, 17, 15, 14, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 17, 19, 20, 22, 24, 26, 28, 30, 32, 35, 37, 39, 42, 44, 47, 50, 53, 55, 58, 61, 64, 68, 71, 74, 78, 81, 85, 88, 92, 96, 99, 103, 107, 111, 115, 120, 124, 128, 133, 137, 142, 146, 151, 156, 161, 166, 171, 176, 181, 186, 191, 197, 202, 207, 213, 219, 224, 230, 236, 242, 248, 254, 260, 266, 272, 279, 285, 291, 298, 304, 311, 318, 324, 331, 338, 345, 352, 359, 366, 373, 381, 388, 395, 403, 410, 418, 426, 433, 441, 449, 457, 465, 473, 481, 489, 497, 505, 514, 522, 530, 539, 547, 556, 565, 573, 582, 591, 600, 609, 618, 627, 636, 645, 654, 663, 672, 682, 691, 701, 710, 720, 729, 739, 749, 758, 768, 778, 788, 798, 808, 818, 828, 838, 848, 858, 869, 879, 889, 900, 910, 920, 931, 941, 952, 963, 973, 984, 995, 1006, 1017, 1027, 1038, 1049, 1060, 1071, 1082, 1093, 1105, 1116, 1127, 1138, 1149, 1161, 1172, 1183, 1195, 1206, 1218, 1229, 1241, 1252, 1264, 1276, 1287, 1299, 1311, 1322, 1334, 1346, 1358, 1370, 1381, 1393, 1405, 1417, 1429, 1441, 1453, 1465, 1477, 1489, 1501, 1514, 1526, 1538, 1550, 1562, 1574, 1587, 1599, 1611, 1623, 1636, 1648, 1660, 1673, 1685, 1697, 1710, 1722, 1735, 1747, 1760, 1772, 1784, 1797, 1809, 1822, 1834, 1847, 1859, 1872, 1884, 1897, 1909, 1922, 1934, 1947, 1960, 1972, 1985, 1997, 2010, 2022, 2035, 2047, 2060, 2073, 2085, 2098, 2110, 2123, 2135, 2148, 2161, 2173, 2186, 2198, 2211, 2223, 2236, 2248, 2261, 2273, 2286, 2298, 2311, 2323, 2335, 2348, 2360, 2373, 2385, 2398, 2410, 2422, 2435, 2447, 2459, 2472, 2484, 2496, 2508, 2521, 2533, 2545, 2557, 2569, 2581, 2594, 2606, 2618, 2630, 2642, 2654, 2666, 2678, 2690, 2702, 2714, 2725, 2737, 2749, 2761, 2773, 2784, 2796, 2808, 2819, 2831, 2843, 2854, 2866, 2877, 2889, 2900, 2912, 2923, 2934, 2946, 2957, 2968, 2979, 2990, 3002, 3013, 3024, 3035, 3046, 3057, 3068, 3078, 3089, 3100, 3111, 3122, 3132, 3143, 3154, 3164, 3175, 3185, 3195, 3206, 3216, 3226, 3237, 3247, 3257, 3267, 3277, 3287, 3297, 3307, 3317, 3327, 3337, 3346, 3356, 3366, 3375, 3385, 3394, 3404, 3413, 3423, 3432, 3441, 3450, 3459, 3468, 3477, 3486, 3495, 3504, 3513, 3522, 3530, 3539, 3548, 3556, 3565, 3573, 3581, 3590, 3598, 3606, 3614, 3622, 3630, 3638, 3646, 3654, 3662, 3669, 3677, 3685, 3692, 3700, 3707, 3714, 3722, 3729, 3736, 3743, 3750, 3757, 3764, 3771, 3777, 3784, 3791, 3797, 3804, 3810, 3816, 3823, 3829, 3835, 3841, 3847, 3853, 3859, 3865, 3871, 3876, 3882, 3888, 3893, 3898, 3904, 3909, 3914, 3919, 3924, 3929, 3934, 3939, 3944, 3949, 3953, 3958, 3962, 3967, 3971, 3975, 3980, 3984, 3988, 3992, 3996, 3999, 4003, 4007, 4010, 4014, 4017, 4021, 4024, 4027, 4031, 4034, 4037, 4040, 4042, 4045, 4048, 4051, 4053, 4056, 4058, 4060, 4063, 4065, 4067, 4069, 4071, 4073, 4075, 4076, 4078, 4080, 4081, 4083, 4084, 4085, 4086, 4087, 4088, 4089, 4090, 4091, 4092, 4093, 4093, 4094, 4094, 4094, 4095, 4095, 4095, };

unsigned int _stm_dac_lut_index[DACMAXWAVEFORMS];		// LUT index in 16.16 format
unsigned int _stm_dac_lut_incr[DACMAXWAVEFORMS];		// LUT increment in 16.16 format
unsigned int _stm_dac_waveform_vol[DACMAXWAVEFORMS];	// Volume from 0 to 4095
unsigned int _stm_dac_dacclock;						// DAC clock in Hz
unsigned int _stm_dac_numwaveforms;					// Number of simultaneously generated waveforms
//unsigned int _stm_dac_ampldiv=20;					// Common amplitude divider
//unsigned int _stm_dac_ampldiv=1;					// Common amplitude divider

unsigned short _stm_dac_dmabuf[DACDMAHALFBUFSIZ*2];			// DMA tempory buffer - DMA uses double buffering so buffer twice the size of DACDMABUFSIZ

unsigned long _stm_dac_ctr_acq,_stm_dac_ctr_loss;

/*
D,200000,697,128,1209,128;w,1000;D,200000,697,128,1336,128;w,1000;D,200000,697,128,1477,128;w,1000;D


D,200000,440,128;w,1000;D,200000,494,128;w,1000;D,200000,554,128;w,1000;D,200000,587,128;w,1000;D,200000,659,128;w,1000;D,200000,740,128;w,1000;D,200000,831,128;w,1000;D,200000,880,128;w,1000;D


*/


// Callback to allow user-specified signal generation
void (*_stm_dac_user_siggen)(unsigned short *buffer,unsigned int n)=0;			// Signal generation: called from DAC DMA callback; user-provided function must generate n samples in buffer
//void (*_stm_dac_user_siginit)(unsigned short *buffer,unsigned int n)=0;			// Signal generation initialisation: called from DAC DMA callback; user-provided function must initialise n samples in buffer (n is double the size here than _stm_dac_user_siggen as the double-buffer should be initialised)


void _stm_dac_clearwaveforms();

/******************************************************************************
	function: stmdac_init()
*******************************************************************************
	Starts the DAC either for constant output or for waveform output.

	Parameters:
		samplerate		-	0 for constant output; or nonzero to specify the samplerate for waveform output
		cb				-	Callback function called to generate samples. A null pointer can be passed when samplerate=0 as callbacks are not used when the output is constant.

	Returns:
		-

*******************************************************************************/
void stmdac_init(unsigned int samplerate,void (*cb)(unsigned short *buffer,unsigned int n))
{
	HAL_StatusTypeDef s;

	// First, deinit.
	stmdac_deinit();

	stm_dac_clearstat();

	if(samplerate!=0)
	{
		// Waveform generation mode
		memset(_stm_dac_dmabuf,0,DACDMAHALFBUFSIZ*2*sizeof(short));			// Clear sample buffer
		_stm_dac_user_siggen = cb;											// Set the callback

		// HAL_DAC_Start_DMA must be called before initialising the timer. Otherwise, experiments showed the DAC sometimes does not start.

		// Start DMA
		s = HAL_DAC_Start_DMA(&hdac1,DAC_CHANNEL_1,(uint32_t*)_stm_dac_dmabuf,DACDMAHALFBUFSIZ*2,DAC_ALIGN_12B_R);

		// Compute the dividers
		unsigned pclk = stm_rcc_get_apb1_timfrq();
		fprintf(file_pri,"DAC: pclk=%u\n",pclk);
		unsigned div = (pclk+(samplerate/2))/samplerate;		// Round to nearest
		fprintf(file_pri,"DAC: desired divider=%u\n",div);
		div=div/2-1;											// Timer must have divider set to 1 at least so divide prescaler by 2; -1 as divides by (1+...)
		_stm_dac_init_timer(div,1);
		//fprintf(file_pri,"div=%u\n",div);
		fprintf(file_pri,"DAC: effective sample rate=%u; interrupt per second=%u\n",_stm_dac_dacclock,(_stm_dac_dacclock+DACDMAHALFBUFSIZ/2)/DACDMAHALFBUFSIZ);		// Round to nearest
	}
	else
	{
		// Samplerate is zero: DAC for constant output
		s = HAL_DAC_Start(&hdac1,DAC_CHANNEL_1);
	}
	if(s)
		fprintf(file_pri,"DAC: error starting DAC (%d)\n",s);
}
/******************************************************************************
	function: stmdac_deinit()
*******************************************************************************
	Stops the timer and DMA and software conversion.

	Returns:
		-

*******************************************************************************/
void stmdac_deinit()
{
	_stm_dac_deinit_timer();

	// Stop both DMA and regular
	HAL_DAC_Stop_DMA(&hdac1,DAC_CHANNEL_1);
	HAL_DAC_Stop(&hdac1,DAC_CHANNEL_1);

}
/******************************************************************************
	function: stmdac_set_siggencallback()
*******************************************************************************
	Sets the callback function which will be called to generate samples to send
	to the DAC.
	This callback will be called from an interrupt routine so must complete
	promptly.

	Parameters:
		samplerate		-	0 for constant output; or nonzero to specify the samplerate for waveform output

	Returns:
		-

*******************************************************************************/
void stmdac_set_siggencallback(void (*cb)(unsigned short *buffer,unsigned int n))
{
	_stm_dac_user_siggen = cb;
}

/******************************************************************************
	function: stmdac_setval()
*******************************************************************************
	Sets a static (DC) DAC output value.
	The DAC must be initialised with stm_init and a samplerate of 0 to indicate
	DC generation.

	Parameters:
		val		-	Value 0 to 4096

	Returns:
		-

*******************************************************************************/
void stmdac_setval(unsigned val)
{
	HAL_StatusTypeDef s = HAL_DAC_SetValue(&hdac1,DAC_CHANNEL_1,DAC_ALIGN_12B_R,val);
	//fprintf(file_pri,"Set val: %d\n",s);
	LL_TIM_GenerateEvent_UPDATE(TIM5);				// DAC configured with timer as trigger. (Alternatively configure to no trigger: sConfig.DAC_Trigger = DAC_TRIGGER_NONE;)

	(void) s;
}
/******************************************************************************
	function: stmdac_getval()
*******************************************************************************
	Gets a static (DC) DAC output value
	The DAC must be initialised with stm_init and a samplerate of 0 to indicate
	DC generation.

	Parameters:
		-

	Returns:
		Current DAC value

*******************************************************************************/
unsigned stmdac_getval()
{
	return (unsigned) HAL_DAC_GetValue(&hdac1,DAC_CHANNEL_1);
}


/******************************************************************************
	function: _stm_dac_addwaveform
*******************************************************************************
	Add a waveforms if space available

	Returns:
		0		-		Ok
		1		-		Error

*******************************************************************************/
int stm_dac_cosinegenerator_addwaveform(unsigned freq, unsigned vol)
{
	if(_stm_dac_numwaveforms>=DACMAXWAVEFORMS)
		return 1;

	if(freq==0)
		return 1;
	if(vol>4096)
		return 1;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_dac_lut_incr[_stm_dac_numwaveforms] = _stm_dac_computeincr(freq,_stm_dac_dacclock,STMDAC_LUTSIZ);
		if(vol>4095)
			vol = 4095;
		_stm_dac_waveform_vol[_stm_dac_numwaveforms] = vol;
		_stm_dac_lut_index[_stm_dac_numwaveforms] = 0;
		_stm_dac_numwaveforms++;
	}
	//fprintf(file_pri,"Waveform %d: freq: %u; vol: %u; increment: %u\n",_stm_dac_numwaveforms-1,freq,vol,_stm_dac_lut_incr[_stm_dac_numwaveforms-1]);

	return 0;
}
/******************************************************************************
	function: stm_dac_cosinegenerator_init
*******************************************************************************
	Initialiser for cosine waveform generation.

	Clears all waveforms.

*******************************************************************************/
void stm_dac_cosinegenerator_init()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		//for(unsigned i=0;i<n;i++)
//			buffer[i]=0;
		_stm_dac_numwaveforms=0;
	}
}


/******************************************************************************
	function: _stm_dac_initwaveforms
*******************************************************************************
	Initialise n waveforms and compute the increment for n specified waveforms.

*******************************************************************************/
#if 0
void _stm_dac_cosinegenerator_initwaveforms(unsigned n,...)
{
	_stm_dac_clearwaveforms();

	va_list args;

	// Initialise the return values to null
	va_start(args,n);
	for(unsigned i=0;i<n;i++)
	{
		unsigned f = va_arg(args, unsigned);
		int rv = stm_dac_cosinegenerator_addwaveform(f,4095);
		fprintf(file_pri,"%d: %u (ok: %d)\n",i,f,rv);
	}
	va_end(args);

}
#endif
/******************************************************************************
	function: _stm_dac_initwaveforms
*******************************************************************************
	Initialise n waveforms and compute the increment for n specified waveforms.

*******************************************************************************/
/*void _stm_dac_cosinegenerator_(unsigned n,...)
{

}*/
/******************************************************************************
	function: stm_dac_cosinegenerator_siggen
*******************************************************************************
	Signal generator callback function for superposition of cosines.

*******************************************************************************/
void stm_dac_cosinegenerator_siggen(unsigned short *buffer,unsigned n)
{
	for(unsigned i=0;i<n;i++)
	{
		buffer[i] = 0;
		for(int w=0;w<_stm_dac_numwaveforms;w++)
		{
			unsigned int s = _stm_dac_cos_lut[_stm_dac_lut_index[w]>>16];		// Index is in 16.16 format
			s=(s*_stm_dac_waveform_vol[w])>>12;									// Volume is 0..4096 inclusive
			buffer[i] += s;
			_stm_dac_lut_index[w] += _stm_dac_lut_incr[w];
			_stm_dac_lut_index[w] &= STMDAC_LUTINDEXMASK;						// Wraparound
		}
		//_stm_dac_dmabuf[DACDMAHALFBUFSIZ+i] /= _stm_dac_numwaveforms;			// Rescale to ensure same amplitude regardless of number of waveforms
		//_stm_dac_dmabuf[DACDMAHALFBUFSIZ+i] /= _stm_dac_ampldiv;
	}
}

/******************************************************************************
	function: _stm_dac_computeincr
*******************************************************************************
	Computes a 16.16 increment to achieve a specified target frequency, assuming
	a specific DAC clock, and a specified lookup table size (for 1 period of the
	waveform).

	1 Hz: increment goes through lutsize in 1 second, i.e. increment = lutsize/dacclock
	f Hz: increment goes through lutsize in 1/f seconds: i.e. increment = f*lutsize/dacclock

	Returns increment in 16.16 fixed-point format


*******************************************************************************/
unsigned _stm_dac_computeincr(unsigned targetfrq,unsigned dacclock,unsigned lutsize)
{
	// Rounding sophistications - round to nearest
	unsigned increment = ((unsigned long long)targetfrq*lutsize*65536+dacclock/2)/dacclock;
	return increment;
}
/******************************************************************************
	function: _stm_dac_computefrqfrominc
*******************************************************************************
	Complement of _stm_dac_computeincr
*******************************************************************************/
unsigned _stm_dac_computefrqfrominc(unsigned increment,unsigned dacclock,unsigned lutsize)
{
	// Rounding sophistications - round to nearest
	unsigned frq = (((unsigned long long)dacclock*increment+32768)/65536+lutsize/2)/lutsize;
	return frq;
}

/******************************************************************************
	function: _stm_adc_init_timer
*******************************************************************************
	Init timer for timer-triggered conversion

	Clock is at 20MHz

	Use timer 5
	Parameters:


*******************************************************************************/
void _stm_dac_init_timer(unsigned prescaler,unsigned reload)
{
	unsigned pclk = stm_rcc_get_apb1_timfrq();
	_stm_dac_dacclock = pclk/(prescaler+1)/(reload+1);		// Compute the DAC clock frequency

	LL_TIM_InitTypeDef TIM_InitStruct = {0};

	// Peripheral clock enable
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM5);

	// TIM5 interrupt Init
	NVIC_SetPriority(TIM5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(TIM5_IRQn);

	TIM_InitStruct.Prescaler = prescaler;
	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
	TIM_InitStruct.Autoreload = reload;
	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
	TIM_InitStruct.RepetitionCounter = 0;
	ErrorStatus s = LL_TIM_Init(TIM5, &TIM_InitStruct);
	(void) s;
	//fprintf(file_pri,"timer init: %u\n",s);

	LL_TIM_SetClockSource(TIM5, LL_TIM_CLOCKSOURCE_INTERNAL);
	LL_TIM_SetTriggerOutput(TIM5, LL_TIM_TRGO_UPDATE);
	LL_TIM_DisableMasterSlaveMode(TIM5);

	//LL_TIM_EnableIT_UPDATE(TIM5);						// Optionally, enable interrupts
	LL_TIM_EnableCounter(TIM5);
	//LL_TIM_GenerateEvent_UPDATE(TIM5);				// Doesn't seem necessary




}
/******************************************************************************
	function: _stm_adc_deinit_timer
*******************************************************************************
	Stop timer for conversions

	Use timer 5

*******************************************************************************/
void _stm_dac_deinit_timer()
{

	LL_TIM_DisableCounter(TIM5);			// Stop counting
	LL_TIM_ClearFlag_UPDATE(TIM5);			// Disable flag
	LL_TIM_ClearFlag_TRIG(TIM5);			// Disable flag
}


#if 0
// Timer 5 interrrupt handler
void TIM5_IRQHandler(void)
{
	if(LL_TIM_IsActiveFlag_UPDATE(TIM5) == 1)
	{
		// Clear the update interrupt flag
		LL_TIM_ClearFlag_UPDATE(TIM5);
	}

	static unsigned long ctr=0;
	static unsigned long tlast=0;

/*	if(file_pri)
	{
		ctr++;
		unsigned long t = timer_ms_get();
		//fprintf(file_pri,"t %lu: %lu\n",ctr,t-tlast);
		itmprintf("t %lu: %lu\n",ctr,t-tlast);
		tlast=t;
	}*/
	fprintf(file_pri,".\n");

}
#endif




//void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	//fprintf(file_pri,"C\n");
	// Second half fed to DAC; DMA now reads from 1st half, so fill second half of DMA buf with new data

	if(_stm_dac_user_siggen)
	{
		// Call the callback passing the second half of the DMA buffer
		_stm_dac_user_siggen(_stm_dac_dmabuf+DACDMAHALFBUFSIZ,DACDMAHALFBUFSIZ);
		_stm_dac_ctr_acq += DACDMAHALFBUFSIZ;										// Update statistics
	}
}
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac)
{
	//fprintf(file_pri,"H\n");
	// First half fed to DAC; DMA now reads from 2nd half, so fill first half of DMA buf with new data

	if(_stm_dac_user_siggen)
	{
		// Call the callback passing the first half of the DMA buffer
		_stm_dac_user_siggen(_stm_dac_dmabuf,DACDMAHALFBUFSIZ);
		_stm_dac_ctr_acq += DACDMAHALFBUFSIZ;										// Update statistics
	}
}

unsigned long stmdac_stat_totframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_dac_ctr_acq;
	}
	return 0;
}
unsigned long stmdac_stat_lostframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_dac_ctr_loss;
	}
	return 0;
}


/******************************************************************************
	function: stm_dac_clearstat
*******************************************************************************
	Clear DAC signal generation statistics.


*******************************************************************************/
void stm_dac_clearstat()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_dac_ctr_acq=_stm_dac_ctr_loss=0;
	}
}

void stmdac_benchmaxspeed()
{
	// Benchmark the time to fill a half DMA buffer.
	// This function should be called when DAC generation is off
	stmdac_deinit();
	_stm_dac_dacclock = 200000;					//	Arbitrary sample rate: does not influence benchmark



	//
	unsigned long t1,t2;
	unsigned sps;
	unsigned nit = 1000;


	for(int nw=0;nw<5;nw++)
	{
		stm_dac_cosinegenerator_addwaveform(20000-nw*1000,4096);			// Add waveform

		t1 = timer_ms_get();
		for(unsigned i=0;i<nit;i++)
		{
			HAL_DAC_ConvHalfCpltCallbackCh1(0);
		}
		t2 = timer_ms_get();

		fprintf(file_pri,"DAC benchmark: number of waveforms: %u; time for %u iterations: %lu ms\n",nw+1,nit,t2-t1);
		sps = nit*DACDMAHALFBUFSIZ*1000/(t2-t1);
		fprintf(file_pri,"\tMax DAC sample rate for %u waveforms: %u sps\n",nw+1,sps);
	}

}
void stmdac_benchcpuoverhead()
{
	unsigned long refperf,perf;
	unsigned sr,nw;

	// Format: samplerate, numwaves, frq0, frq1,...; samplerate, ...
	unsigned sr_frq[]={	250000,1,1000,
						200000,1,1000,
						200000,2,1000,2000,
						100000,1,1000,
						100000,2,1000,2000,
						100000,3,1000,2000,3000,
						50000,1,1000,
						50000,2,1000,2000,
						50000,3,1000,2000,3000,
						50000,4,1000,2000,3000,4000,
						0
					};

	fprintf(file_pri,"DAC: benchmarking waveform generation CPU overhead\n");

	stmdac_deinit();				// DAC must be off



	refperf = system_perfbench(2);

	fprintf(file_pri,"DAC: reference performance: %lu\n",refperf);

	for(unsigned c=0;1;)
	{
		sr = sr_frq[c];			// c: index of sample rate
		if(sr==0)				// sr=0 means quit
			break;
		c++;								// c: index of number of waveforms
		nw = sr_frq[c];
		c++;								// c: frequency of wave

		fprintf(file_pri,"\tSample rate: %u. Number of waveforms: %u\n",sr,nw);

		// Start the DAC
		stmdac_init(sr,stm_dac_cosinegenerator_siggen);

		// Init cosine generator
		// Note: this must be done after stmdac_init, as the sample rate set in internal variables is needed by the cosine generator.
		stm_dac_cosinegenerator_init();
		for(int i=0;i<nw;i++)
		{
			stm_dac_cosinegenerator_addwaveform(sr_frq[c+i],4096);
			fprintf(file_pri,"\t\tWaveform %d: frq=%d\n",i,sr_frq[c+i]);
		}

		c+=nw;

		perf = system_perfbench(2);
		unsigned long o = (refperf-perf)*100/refperf;
		fprintf(file_pri,"\tPerformance: %lu. Overhead: %lu%%\n",perf,o);

		stmdac_deinit();

		fprintf(file_pri,"\n");
	}


}
