#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "stm32l496xx.h"
#include "stmcic.h"
#include "global.h"
#include "dfsdm.h"
#include "main.h"
#include "wait.h"
#include "helper.h"
#include "stmadcfast.h"
#include "stm32l4xx_hal.h"



/******************************************************************************
	file: stmcic.c
*******************************************************************************

	Access to STM CIC hardware filter (SFDFSDM) for general purpose signal processing.
	Unlike stmdfsdm which is geared for sigma-delta microphone processing, these methods are useful for processing in-memory data.

	Usage options:
		- Write to CIC: 1) one sample at a time (normal mode); 2) 2 samples at a time in a single 32-bit write (interleaved)
		- Read from CIC: 1) CPU read; 2) DMA readout.

	The data is pushed to the CIC by the programmer by direct register writes.


	*Hardware characteristics*

	* DFSDM on APB2
	* Use channels 0, 1
	* Use Filters 0, 1
	* Use DMA1c4, DM1c5 for data readout
	* DFSDM1_Channel0<>DFSDM1_Filter0<>DMA1_Channel4
	* DFSDM1_Channel1<>DFSDM1_Filter1<>DMA1_Channel5

	*Implementation information*

	The filters and DMA channels are also used for microphone therefore microphone acquisition and hardcic are mutually exclusive.

	The DMA channels are used without interrupts, so no need to modify interrupt vectors.
	Polling can be used to wait for DMA readout completion. For maximum performance, judicious insertions of NOPs can be used instead of polling for DMA transfer complete flag.

	* Main functions*

		*Core functions*

		* stm_hardcic_init									Initialise the hardware CIC filter.
		* stm_hardcic_offset								Specify an offset to be added to the filter output result
		* stm_hardcic_process_dma_setup						Only used in DMA mode. Call before feeding data to specify where output data will be DMA-stored
		* stm_hardcic_process_dma_waitcomplete				Polling wait for the DMA transfer to complete.
															Ensure the right amount of data is fed to the CIC according to downsampling ratio and DMA output buffer, otherwise this hangs.
		* stm_hardcic_process_dma_remaining					Returns how many output samples the DMA expects. When 0, the DMA transfer has completed.

 	 	 *High-level streaming processing*
 	 	 * stm_hardcic_process_stream_16_32_cpu_init		Initialise the CIC to process a stream of data frame by frame
 	 	 * stm_hardcic_process_stream_16_32_cpu_process		Process a frame of data.

*/



// Abstraction to allow easier selection and configuration of a channel.
DFSDM_Filter_TypeDef *_stm_hardcic_filters[2] = {DFSDM1_Filter0,DFSDM1_Filter1};
DFSDM_Channel_TypeDef *_stm_hardcic_channels[2] = {DFSDM1_Channel0,DFSDM1_Channel1};
DMA_Channel_TypeDef *_stm_hardcic_dma[2] = {DMA1_Channel4,DMA1_Channel5};
unsigned _stm_hardcic_dma_intclear[2] = {DMA_ISR_TCIF4 | DMA_ISR_HTIF4 | DMA_ISR_GIF4 | DMA_ISR_TEIF4, DMA_ISR_TCIF5 | DMA_ISR_HTIF5 | DMA_ISR_GIF5 | DMA_ISR_TEIF5};	// Clear interrupt flags
unsigned _stm_hardcic_dma_cselr_mask[2] = {~DMA_CSELR_C4S,~DMA_CSELR_C5S};			// Mask to select the dfsdm channel for dma
unsigned _stm_hardcic_dma_isr_pos[2] = {12,16};			// Position of first DMA ISR bit

/******************************************************************************
	function: stm_hardcic_init
*******************************************************************************
	Primary function to initialise the CIC.

	Call only once. Subsequencly call stm_hardcic_process().

	Input:
		cicchannel:		CIC channel, 0 or 1.
		winsize:		Actual CIC window size is winsize+1, and this is equal to downsampling ratio. For example: winsize=1 for summing 2 samples, and downsampling by 2.
		order:			filter order. 1=sinc1, 2=sinc2, ... 5=sinc5
		interleaved:	0: write a single sample per write to CHDATINR; 1: write two samples per 32-bit write to CHDATINR. Lower 16-bits is first sample.
		dma:			0: disable dma readout
		msbonly:		Only used when dma=1. Indicates that only the upper 16-bit of the 32-bit result

	Returns:
		0				Success
		other			error

******************************************************************************/
unsigned stm_hardcic_init(unsigned cicchannel,unsigned winsize,unsigned order,unsigned interleaved,unsigned dma,unsigned msbonly)
{
	// Sanity check
	if(cicchannel>1 || winsize<1 || winsize>1023 || order<1 || order>5)
		return 1;


	/* ==== 1. Known initial state ====
		Put the system in a known initial state by disabling the channel, filter, DMA and clearing DMA interrupts.
	*/
	// Deactivate the filter - clear to reset defaults
	_stm_hardcic_filters[cicchannel]->FLTCR1 = 0;
	_stm_hardcic_filters[cicchannel]->FLTCR1 = 0;		// Need to write twice: first time deactivates, which allows other fields to be changed.
	_stm_hardcic_filters[cicchannel]->FLTCR2 = 0;
	_stm_hardcic_filters[cicchannel]->FLTFCR = 0;
	// Deactivate the channel - clear to reset defaults
	_stm_hardcic_channels[cicchannel]->CHCFGR1 = 0;
	_stm_hardcic_channels[cicchannel]->CHCFGR1 = 0;			// Need to write twice: first time deactivates, which allows other fields to be changed.
	// Deactivate the DMA1 channel 4 - clear to reset defaults.
	_stm_hardcic_dma[cicchannel]->CCR = 0;
	_stm_hardcic_dma[cicchannel]->CCR = 0;
	// Clear DMA interrupts if any pending. Interrupts are Transfer complete (TC); Half transfer (HT); Transfer error (TE); Global interrupt (GF)
	DMA1->IFCR = _stm_hardcic_dma_intclear[cicchannel];

	/* ==== 2. DFSDM channel initialisation ====
		Channelx - Filterx  - DMA1c4+x
	*/
	// Enable DFSDM peripheral
	_stm_hardcic_channels[cicchannel]->CHCFGR1 |= DFSDM_CHCFGR1_DFSDMEN;
	// Enable channel 0 with data coming from DFSDM_CHyDATINR register by direct CPU/DMA write
	// DATMPX=2: data comes from CPU/DMA
	_stm_hardcic_channels[cicchannel]->CHCFGR1 |= DFSDM_CHCFGR1_DATMPX_1;			// DATMPX=2: data comes from CPU/DMA
	// DATPACK=0: data comes from INDAT0[15:0]: standard mode
	// DATPACK=1: data comes from INDAT0[15:0] (lower 16-bit) of INDAT and INDAT1[15:0] (upper 16-bit) of INDAT, both going to same channel (same filter)
	// DATPACK=2: data comes from INDAT0[15:0] (lower 16-bit) of INDAT to channel0 and INDAT1[15:0] (upper 16-bit) of INDAT to channel1 (two distinct filters)
	// Interleaved mode is modified at the end of this function, after filling the CIC pipeline.
	// Configure offset and bit shift: zero offset and zero bit shift
	_stm_hardcic_channels[cicchannel]->CHCFGR2 = 0;
	// Enable channel
	_stm_hardcic_channels[cicchannel]->CHCFGR1 |= DFSDM_CHCFGR1_CHEN;

	//fprintf(file_pri,"DFSDM CHCFGR1 after: %08x\n",_stm_hardcic_channels[cicchannel]->CHCFGR1);
	//fprintf(file_pri,"DFSDM CHCFGR2 after: %08x\n",_stm_hardcic_channels[cicchannel]->CHCFGR2);


	/* ==== 3. Configure filters ====
		Channel0 - Filter 0 - DMA1c4
		// Downsampling 2
		// Order 2
		// Window size 4
		// Filter 0 for channel 0 for dma readout
	*/
	//fprintf(file_pri,"FLTCR1 prior: %08x\n",_stm_hardcic_filters[cicchannel]->FLTCR1);
	// Enable fast conversion - required to work with DMA (assumes data comes from a continuous stream; i.e. single filter for single stream, which is the case)
	_stm_hardcic_filters[cicchannel]->FLTCR1 = DFSDM_FLTCR1_FAST;
	// Enable DMA if DMA option is selected
	if(dma)
		_stm_hardcic_filters[cicchannel]->FLTCR1 |= DFSDM_FLTCR1_RDMAEN;
	// Select filter type. 1 for sinc1, 2 for sinc2, etc.
	_stm_hardcic_filters[cicchannel]->FLTFCR |= order<< DFSDM_FLTFCR_FORD_Pos;
	// Select window size. effective window size is winsize+1 (i.e. winsize+1 samples are summed)
	_stm_hardcic_filters[cicchannel]->FLTFCR |= winsize<<DFSDM_FLTFCR_FOSR_Pos;
	// Continuous conversion: every time data is written the filter does its job
	_stm_hardcic_filters[cicchannel]->FLTCR1 |= DFSDM_FLTCR1_RCONT;
	// Select from which channel the data comes from: regular channel 0 or regular channel 1. Channel 0 selected by default.
	if(cicchannel==1)
	{
		// Select channel 1
		_stm_hardcic_filters[cicchannel]->FLTCR1 |= 1<<DFSDM_FLTCR1_RCH_Pos;
	}
	// Enable filter
	_stm_hardcic_filters[cicchannel]->FLTCR1 |= DFSDM_FLTCR1_DFEN;
	// Start filtering operation
	_stm_hardcic_filters[cicchannel]->FLTCR1 |= DFSDM_FLTCR1_RSWSTART;



	/* ==== 4. DMA readout initialisation - common part ====
		Initialise DMA1c4 or DMA1c5 to readout from filter 0 or filter 1
	*/
	// Select channel of DMA1c4 or DMA1c5
	DMA1_CSELR->CSELR =  DMA1_CSELR->CSELR&_stm_hardcic_dma_cselr_mask[cicchannel];
	// Configure DMA1cx common parameters: no interrupt, transfer size, increment.
	// Do not use transfer complete interrupt enable (TCIE), as we poll for completion and simplifies the code (no need to hook up into interrupt vector).
	// MSIZE_1=32 bits; PSIZE_1=32 bits; MINC=memory increment
	if(msbonly)		// Read-out only the 16 upper bits of the output register
		_stm_hardcic_dma[cicchannel]->CCR = DMA_CCR_MSIZE_0 | DMA_CCR_PSIZE_0 | DMA_CCR_MINC;						// 16 bit periph and 16-bit memory
	else			// Read-out the 32 bits of the output register
		_stm_hardcic_dma[cicchannel]->CCR = DMA_CCR_MSIZE_1 | DMA_CCR_PSIZE_1 | DMA_CCR_MINC;						// 32 bit periph and memory
	//_stm_hardcic_dma[cicchannel]->CCR = DMA_CCR_MSIZE_1 | DMA_CCR_PSIZE_0 | DMA_CCR_MINC;						// 16 bit periph and 32-bit memory
	// Set the priority of the DMA to high. No clear effect noticed, but aims to ensure that the DMA reads-out as fast as possible.
	// In practice the CPU can fill-in data faster than DMA readout, unless a polling wait or NOPs are used.
	_stm_hardcic_dma[cicchannel]->CCR |= 0b11<<DMA_CCR_PL_Pos;
	// set source register
	if(msbonly)		// Get the data from the upper 16-bit of FLTRDATAR
		_stm_hardcic_dma[cicchannel]->CPAR = ((uint32_t)&_stm_hardcic_filters[cicchannel]->FLTRDATAR)+2;
	else	// Get the data from FLTRDATAR
		_stm_hardcic_dma[cicchannel]->CPAR = (uint32_t)&_stm_hardcic_filters[cicchannel]->FLTRDATAR;
	// Not using interrupts currently.
	//HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);


	/* ==== 5. Pre-fill the CIC pipeline ====
		Feed-in as many samples as needed to trigger a new output sample.
		Guarantees that feeding exactly downsample input samples gives one output sample.
		This is needed as the datasheet initialisation cycles does not seem to be experimentally verified.
		Depending on use of DMA or not distinct filling strategies are followed.
	 */
	if(dma)
		_stm_hardcic_fill_dma(cicchannel);
	else
		_stm_hardcic_fill_nodma(cicchannel);

	/* ==== 6. Interleaving ====
		Change mode to interleaved if requested.
		This is done after sample-by-sample synchronisation.
	 */
	if(interleaved)
	{

		//fprintf(file_pri,"Activating interleaved mode: %08X\n",(unsigned int)_stm_hardcic_channels[cicchannel]->CHCFGR1);
		_stm_hardcic_channels[cicchannel]->CHCFGR1 &= ~DFSDM_CHCFGR1_CHEN;
		_stm_hardcic_channels[cicchannel]->CHCFGR1 |=	DFSDM_CHCFGR1_DATPACK_0;		// Interleaved (two samples at a time)
		_stm_hardcic_channels[cicchannel]->CHCFGR1 |= DFSDM_CHCFGR1_CHEN;
		//fprintf(file_pri,"After activation: %08X\n",(unsigned int)_stm_hardcic_channels[cicchannel]->CHCFGR1);
	}

	// If DMA deactivated disable DMA
	/*if(dma==0)
	{
		fprintf(file_pri," Filter %d. FLTCR1: %08X\n",cicchannel,_stm_hardcic_filters[cicchannel]->FLTCR1);
		fprintf(file_pri," Filter %d. FLTCR2: %08X\n",cicchannel,_stm_hardcic_filters[cicchannel]->FLTCR2);
		fprintf(file_pri," Filter %d. FLTFCR: %08X\n",cicchannel,_stm_hardcic_filters[cicchannel]->FLTFCR);
		_stm_hardcic_filters[cicchannel]->FLTCR1 &= ~DFSDM_FLTCR1_DFEN;
		_stm_hardcic_filters[cicchannel]->FLTCR1 &= ~DFSDM_FLTCR1_RDMAEN;
		_stm_hardcic_filters[cicchannel]->FLTCR1 |=  DFSDM_FLTCR1_DFEN;
		_stm_hardcic_filters[cicchannel]->FLTCR1 |= DFSDM_FLTCR1_RSWSTART;
		fprintf(file_pri," Filter %d. FLTCR1: %08X\n",cicchannel,_stm_hardcic_filters[cicchannel]->FLTCR1);
		fprintf(file_pri," Filter %d. FLTCR2: %08X\n",cicchannel,_stm_hardcic_filters[cicchannel]->FLTCR2);
		fprintf(file_pri," Filter %d. FLTFCR: %08X\n",cicchannel,_stm_hardcic_filters[cicchannel]->FLTFCR);

	}*/
	return 0;	// Success
}
/******************************************************************************
	function: _stm_hardcic_fill
*******************************************************************************
	Pre-fills the CIC buffer with zero samples as needed until it outputs a first sample.

	This ensures the CIC pipeline is synchronised and from then on will output
	one sample after exactly (fosr+1) input samples.

	In principle this could be calculated from the datasheet however there are odd cases:
	ford=2 fosr=3 SINC2, window 4: requires 7 samples to be written, which does not fit
	the datasheet explanations.
******************************************************************************/
unsigned _stm_hardcic_fill_dma(unsigned cicchannel)
{
	unsigned dmaout[4]={0x5555aaaa,0x5555aaaa,0x5555aaaa,0x5555aaaa};		// Output DMA buffer (only 2 samples needed)
	unsigned isr;
	unsigned nfill=0;

	// Setup the DMA
	_stm_hardcic_dma[cicchannel]->CCR &= ~DMA_CCR_EN;		// Deactivate DMA to allow modifications
	// Clear all interrupt flags
	DMA1->IFCR = _stm_hardcic_dma_intclear[cicchannel];
	// Set data destination pointer
	_stm_hardcic_dma[cicchannel]->CMAR=(uint32_t)dmaout;
	// Set number of samples
	_stm_hardcic_dma[cicchannel]->CNDTR = 2;			// Trigger after two output samples
	// Deactivate the TCIE (should not have been previously active)
	_stm_hardcic_dma[cicchannel]->CCR &= ~(DMA_CCR_TCIE);
	// Enable DMA
	_stm_hardcic_dma[cicchannel]->CCR |= DMA_CCR_EN;

	fprintf(file_pri,"Pre-filling CIC pipeline...\n");
	while(1)
	{
	/*	isr = ((DMA1->ISR)>>_stm_hardcic_dma_isr_pos[cicchannel])&0b1111;

		fprintf(file_pri,"Prior write %d. ISR: %x. ",nfill,isr);*/

		// Feed-in zeros to ensure the pipeline is flushed
		_stm_hardcic_channels[cicchannel]->CHDATINR = 0;
		HAL_Delay(1);		// Wait some time for the DMA to indicate a completion of the transfer. Polling could be used for faster initialisation.

		// Read ISR
		isr = ((DMA1->ISR)>>_stm_hardcic_dma_isr_pos[cicchannel])&0b1111;

		//fprintf(file_pri,"After write %d. ISR: %x.\n",nfill,isr);
		// Accumulate the number of samples fed
		nfill++;

		if(isr&0b10)		// Check TCIE bit. If TCIE set, then the DMA transfer is complete and the pipeline is filled
			break;
	}
	fprintf(file_pri,"Pre-filled CIC pipeline with %u samples\n",nfill);
	//fprintf(file_pri,"dmaout: %08x %08x %08x %08x\n",dmaout[0],dmaout[1],dmaout[2],dmaout[3]);

	// Clear DMA interrupt
	DMA1->IFCR = _stm_hardcic_dma_intclear[cicchannel];

	// Disable DMA
	_stm_hardcic_dma[cicchannel]->CCR &= ~DMA_CCR_EN;

	return nfill;
}
/******************************************************************************
	function: _stm_hardcic_fill_nodma
*******************************************************************************
	Pre-fills the CIC buffer with zero samples as needed until it outputs a first sample.

	This ensures the CIC pipeline is synchronised and from then on will output
	one sample after exactly (fosr+1) input samples.

	In principle this could be calculated from the datasheet however there are odd cases:
	ford=2 fosr=3 SINC2, window 4: requires 7 samples to be written, which does not fit
	the datasheet explanations.

	This version does not use the DMA and instead fills the pipeline with ones,
	then with zeros until the output takes the value 0.

******************************************************************************/
unsigned _stm_hardcic_fill_nodma(unsigned cicchannel)
{
	unsigned nfill=0;

	fprintf(file_pri,"Pre-filling CIC pipeline...\n");

	for(int i=0;i<1024;i++)
	{
		// Fill with ones
		_stm_hardcic_channels[cicchannel]->CHDATINR = 1;
		unsigned r = _stm_hardcic_filters[cicchannel]->FLTRDATAR;
		(void)r;
	}
	while((_stm_hardcic_filters[cicchannel]->FLTRDATAR&0xFFFFFF00)!=0)
	{
		_stm_hardcic_channels[cicchannel]->CHDATINR = 0;
		nfill++;
	}

	fprintf(file_pri,"Pre-filled CIC pipeline with %u samples\n",nfill);

	return nfill;
}
/******************************************************************************
	function: stm_hardcic_process_dma_setup
*******************************************************************************
	Call before feeding the cic with data. Afterwards manually feed the data to
	the CIC and call stm_hardcic_process_waitcomplete

	This function setups the DMA to read-out outlen from the cic.

	The amount of data written to the CIC must be equal to outlen*window, with window
	the decimation rate.
******************************************************************************/
void stm_hardcic_process_dma_setup(unsigned cicchannel,unsigned *dataout,unsigned outlen)
{
	//fprintf(file_pri,"\n======= stm_hardcic_process_setup: setting up channel %d with target pointer %p, len: %d\n",cicchannel,dataout,outlen);
	//printdmastatus();

	// Deactivate the DMA to set-it up
	_stm_hardcic_dma[cicchannel]->CCR &= ~DMA_CCR_EN;

	// Clear DMA interrupt
	DMA1->IFCR = _stm_hardcic_dma_intclear[cicchannel];

	// set data destination pointer
	_stm_hardcic_dma[cicchannel]->CMAR=(uint32_t)dataout;
	// set number of output samples
	_stm_hardcic_dma[cicchannel]->CNDTR = outlen;

	// Enable dma
	_stm_hardcic_dma[cicchannel]->CCR |= DMA_CCR_EN;


	//printdmastatus();
	//fprintf(file_pri,"======= stm_hardcic_process_setup end\n\n");


	// At this stage feed data to the CIC by direct writes to DFSDM1_Channel0->CHDATINR or DFSDM1_Channel1->CHDATINR.
}
/******************************************************************************
	function: stm_hardcic_process_dma_waitcomplete
*******************************************************************************
	Call after having fed data to the CIC. Returns when the DMA has written out the last
	filtered data.

	This function blocks, therefore it must be called after the correct amount of input
	data has been fed to the CIC.

******************************************************************************/
void stm_hardcic_process_dma_waitcomplete(unsigned cicchannel)
{
	while(1)
	{
		unsigned isr = ((DMA1->ISR)>>_stm_hardcic_dma_isr_pos[cicchannel])&0b1111;

		if(isr&0b10)		// Check TCIE bit
			break;
	}

	// Clear DMA interrupt
	DMA1->IFCR = _stm_hardcic_dma_intclear[cicchannel];
}
/******************************************************************************
	function: stm_hardcic_process_remaining
*******************************************************************************
	Returns the number of samples which the DMA still should provide.

	This can be used to debug DMA issues where the CIC is filled too quickly for
	DMA readout.
	If stm_hardcic_process_setup is called with a specified number outlen of output
	samples then feeding outlen*downsampleratio should lead to the DMA readout
	being completed and this function should return 0.
	If it returns non-zero, it means samples were written too quickly and the DMA
	readout could not follow.

******************************************************************************/
unsigned stm_hardcic_process_dma_remaining(unsigned cicchannel)
{
	return _stm_hardcic_dma[cicchannel]->CNDTR;
}


/******************************************************************************
	function: stm_hardcic_offset
*******************************************************************************
	Set the specific CIC channel to subtract offset from the result.

******************************************************************************/
void stm_hardcic_offset(int cicchannel,int offset)
{
	// Deactivate channel
	_stm_hardcic_channels[cicchannel]->CHCFGR1 &= ~DFSDM_CHCFGR1_CHEN;
	// Set offset
	_stm_hardcic_channels[cicchannel]->CHCFGR2 = (_stm_hardcic_channels[cicchannel]->CHCFGR2 & 0xFF) | (offset<<8);
	// Activate channel
	_stm_hardcic_channels[cicchannel]->CHCFGR1 |= DFSDM_CHCFGR1_CHEN;
}



void printdmastatus()
{
	// Try to get DMA to work
	fprintf(file_pri,"==== DMA1_Channel4 ====\n");
	fprintf(file_pri,"DMA1_Channel4 CMAR: %08x\n",(unsigned)DMA1_Channel4->CMAR);
	fprintf(file_pri,"DMA1_Channel4 CPAR: %08x\n",(unsigned)DMA1_Channel4->CPAR);
	fprintf(file_pri,"DMA1_Channel4 CNDTR: %08x\n",(unsigned)DMA1_Channel4->CNDTR);
	fprintf(file_pri,"DMA1_Channel4 CCR: %08x\n",(unsigned)DMA1_Channel4->CCR);
	fprintf(file_pri,"==== DMA1_Channel5 ====\n");
	fprintf(file_pri,"DMA1_Channel5 CMAR: %08x\n",(unsigned)DMA1_Channel5->CMAR);
	fprintf(file_pri,"DMA1_Channel5 CPAR: %08x\n",(unsigned)DMA1_Channel5->CPAR);
	fprintf(file_pri,"DMA1_Channel5 CNDTR: %08x\n",(unsigned)DMA1_Channel5->CNDTR);
	fprintf(file_pri,"DMA1_Channel5 CCR: %08x\n",(unsigned)DMA1_Channel5->CCR);
	fprintf(file_pri,"==== DMA1 general====\n");
	fprintf(file_pri,"DMA1->ISR: %08x\n",(unsigned)DMA1->ISR);
	fprintf(file_pri,"DMA1->IFCR: %08x\n",(unsigned)DMA1->IFCR);
	fprintf(file_pri,"DMA1_CSELR: %08x\n",(unsigned)DMA1_CSELR->CSELR);

}

/******************************************************************************
	HARDCIC - HIGH-LEVEL - HARDCIC - HIGH-LEVEL - HARDCIC - HIGH-LEVEL - HARDCIC - HIGH-LEVEL -
********************************************************************************/

/******************************************************************************
	function: stm_hardcic_process_stream_16_32_cpu_init
*******************************************************************************
	Initialise a CIC channel for streaming processing with CPU readout.

	Input:
		cicchannel:		CIC channel, 0 or 1.
		downsampling:	CIC window size and downsampling ratio. For example: downsampling=2 for summing 2 samples, and downsampling by 2.
		order:			filter order. 1=sinc1, 2=sinc2, ... 5=sinc5
******************************************************************************/
unsigned stm_hardcic_process_stream_16_32_cpu_init(unsigned cicchannel,unsigned downsampling,unsigned order)
{
	return stm_hardcic_init(cicchannel,downsampling-1,order,0,0,0);
}
/******************************************************************************
	function: stm_hardcic_process_stream_16_32_cpu_process
*******************************************************************************
	Feeds in data to filter and get output.

	Checks conversion complete flag to know when a conversion is ready for readout.
	This ensures that the number of output samples is correct regardless of the downsampling ratio.


	Input:
		cicchannel:		CIC channel, 0 or 1.
		input:			Array of signed 16-bit input data
		output:			Array of signed 32-bit output data
		inlen:			Number of input data

	Returns:
		Number of output samples
******************************************************************************/
unsigned stm_hardcic_process_stream_16_32_cpu_process(unsigned cicchannel,short * restrict input,int *restrict output,unsigned inlen)
{
	unsigned nr=0;
	DFSDM_Channel_TypeDef *channel = _stm_hardcic_channels[cicchannel];
	DFSDM_Filter_TypeDef *filter = _stm_hardcic_filters[cicchannel];

	// Feed data to filter
	for(unsigned i=inlen;i!=0;i--)
	{
		// Push data
		channel->CHDATINR=*input++;
		//channel->CHDATINR=*input++;
		// Wait for filter conversion to complete
		//while(!(filter->FLTISR&2));
		// Read-out data
		//*output++=filter->FLTRDATAR;
		if(filter->FLTISR&2)
		{
			*output++=filter->FLTRDATAR;
			nr++;
		}
	}
	return nr;
}

#if 0
/******************************************************************************
	SOFTWARE CIC - INCOMPLETE - SOFTWARE CIC - INCOMPLETE - SOFTWARE CIC - INCOMPLETE
*******************************************************************************
*/
/*
    Streaming implementation
    Prints the working buffer.

    n: number of blocks
    d: window size
    size: size of working buffer (excluding the extra d samples)
*/
void cic_4_2_stream_print(double *w,int n,int d,int size)
{
    // Implemtation of the CIC

    for(int ni=0;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
    {
        fprintf(file_pri,"%d: ",ni);
        for(int i=0;i<size+d;i++)       // Iterate all samples
        {
            if(i==d)
                fprintf(file_pri," | ");
            fprintf(file_pri,"%+07.2f ",w[ni*(size+4)+i]);

        }
        fprintf(file_pri,"\n");
    }
}
void cic_4_2_short_stream_print(short *w,int n,int d,int size)
{
    // Implemtation of the CIC

    for(int ni=0;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
    {
        fprintf(file_pri,"%d: ",ni);
        for(int i=0;i<size+d;i++)       // Iterate all samples
        {
            if(i==d)
                fprintf(file_pri," | ");
            fprintf(file_pri,"%+05d ",w[ni*(size+4)+i]);

        }
        fprintf(file_pri,"\n");
    }
}
void cic_4_2_stream(double *x,int size,double *xf)
{
#if MEGALOLDSPDEBUG==1
    fprintf(file_pri,"CIC\n");
#endif

    // Allocate memory for the processing
    int d=4;                    // d: window size
    int n=2;                    // n: number of stages
    int stride = CICBUFSIZ+d;      // spacing between lines in w

    // Allocate an array with n+1 lines of size+d elements.
    double w[3*(CICBUFSIZ+4)];

    // Initialisation
    // Set all to zero
    for(int ni=0;ni<n+1;ni++)
    {
        for(int i=0;i<stride;i++)
        {
            w[ni*stride+i] = 0;
        }
    }
#if MEGALOLDSPDEBUG==1
    fprintf(file_pri,"Initialisation\n");
    cic_4_2_stream_print(w,n,d,CICBUFSIZ);
#endif

    // Read in the data in chunks of CICBUFSIZ
    double *xin=x;
    while(size)
    {

        // Copy a buffer of data or less, depending on size
        int cs = size;
        if(cs>CICBUFSIZ)
            cs=CICBUFSIZ;

        fprintf(file_pri,"Processing block: cs=%d\n",cs);

        // Copy the input data at the first line in w, offseted by d
        for(int i=0;i<cs;i++)
            w[d+i]=*xin++;

        fprintf(file_pri,"Data in\n");
        cic_4_2_stream_print(w,n,d,CICBUFSIZ);

        // Implemtation of the CIC
        for(int i=d;i<d+cs;i++)       // Iterate samples starting at sample d (first sample)
        {
            for(int ni=1;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
            {
                //fprintf(1,'Prev out: %d. Adding %d, subbing %d\n',out(1,i-1),x(i),x(i-d+1));
                double t=0;
                t = w[ni*stride+i-1];
                t += w[(ni-1)*stride+i];
                t -= w[(ni-1)*stride+i-d];
                w[ni*stride+i] = t;
            }
        }

        // Extract the result
        for(int i=0;i<cs;i++)
        {
            *xf++ = w[n*stride+d+i];
        }

        // Finalise: to ensure streamability, move the last d samples of each line back at position 0
        for(int ni=0;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
        {
            for(int i=0;i<d;i++)       // Last 4 samples
            {
                w[ni*stride+i] = w[ni*stride+CICBUFSIZ+i];
            }
        }

        fprintf(file_pri,"Data updated\n");
        cic_4_2_stream_print(w,n,d,CICBUFSIZ);

        // Decrease amount of data left to process
        size-=cs;
    }

}
/*
	Clear the working buffer

	The working buffer must be of size 3*(CICBUFSIZ+4)
*/
void cic_4_2_short_stream_init(short *w)
{
	// Set all to zero
	int stride = CICBUFSIZ+4;      // spacing between lines in w
	for(int ni=0;ni<3;ni++)
	{
		for(int i=0;i<stride;i++)
		{
			w[ni*stride+i] = 0;
		}
	}
}
/*
	Process chunk of data x, store the result in xf, and use the provided working buffer w.
	w must be of size short w[3*(CICBUFSIZ+4)];
	1344 cps

*/
 __attribute__((optimize("unroll-loops")))
void cic_4_2_short_stream_process(short * restrict x,short * restrict w,int size,short * restrict xf)
{
#if MEGALOLDSPDEBUG==1
    fprintf(file_pri,"CIC\n");
#endif

    // Allocate memory for the processing
    int d=4;                    // d: window size
    int n=2;                    // n: number of stages
    int stride = CICBUFSIZ+d;      // spacing between lines in w

    // Allocate an array with n+1 lines of size+d elements.
    //short w[3*(CICBUFSIZ+4)];

    short *restrict tmp;
    short *restrict w0 = w;			// Input data
    short *restrict w1 = w0+stride;	// Stage 1
    short *restrict w2 = w1+stride;	// Stage 2, output


#if MEGALOLDSPDEBUG==1
    fprintf(file_pri,"Initialisation\n");
    cic_4_2_short_stream_print(w,n,d,CICBUFSIZ);
#endif

    // Read in the data in chunks of CICBUFSIZ
    short * restrict xin=x;
    while(size)
    {

        // Copy a buffer of data or less, depending on size
        int cs = size;
        if(cs>CICBUFSIZ)
            cs=CICBUFSIZ;

#if MEGALOLDSPDEBUG==1
        fprintf(file_pri,"Processing block: cs=%d\n",cs);
#endif

        // Copy the input data at the first line in w, offseted by d
        tmp = w+d;		// Point to location of input data
        for(unsigned i=cs;i!=0;i--)
        	*tmp++=*xin++;

#if MEGALOLDSPDEBUG==1
        fprintf(file_pri,"Data in\n");
        cic_4_2_short_stream_print(w,n,d,CICBUFSIZ);
#endif

        // Implemtation of the CIC
		/*for(int i=d;i<d+cs;i++)       // Iterate samples starting at sample d (first sample)
		{
			for(int ni=1;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
			{
                //fprintf(1,'Prev out: %d. Adding %d, subbing %d\n',out(1,i-1),x(i),x(i-d+1));
                short t=0;
                t = w[ni*stride+i-1];
                t += w[(ni-1)*stride+i];
                t -= w[(ni-1)*stride+i-d];
                w[ni*stride+i] = t;
            }
        }*/
        // Initialise the working pointers

#if 0
        for(unsigned ni=0;ni<n;ni++)	// n iterations (n=2)
		{
	        short * restrict wcur = w1+ni*stride+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
			//short * restrict wprev = w0+ni*stride+d;		// Previous line. Points to line 0 (input) then 1.
			//short * restrict wprevd = w0+ni*stride;		// Previous line but d samples earlier. Points to line 0 (input) then 1.

			/*for(int ni=1;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
			{
				//fprintf(1,'Prev out: %d. Adding %d, subbing %d\n',out(1,i-1),x(i),x(i-d+1));
				short t=0;
				t = w[ni*stride+i-1];
				t += w[(ni-1)*stride+i];
				t -= w[(ni-1)*stride+i-d];
				w[ni*stride+i] = t;
			}*/
	        short t = *(wcur-1);
			for(unsigned i=cs;i!=0;i--)     // Iterate samples
			{
        		t += *(wcur-stride);
        		t -= *(wcur-stride-d);
        		*wcur=t;
        		wcur++;
        		//wprev++;
        		//wprevd++;
        	}
		}
#endif
        short * restrict wcur0 = w0+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
        short * restrict wcur1 = w1+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
        short * restrict wcur2 = w2+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
        short t1 = *(wcur1-1);
        short t2 = *(wcur2-1);
        for(unsigned i=cs;i!=0;i--)     // Iterate samples
        {
        	t1 += *(wcur1-stride);
        	t1 -= *(wcur1-stride-d);
        	*wcur1=t1;
        	t2 += t1;
        	t2 -= *(wcur2-stride-d);
        	*wcur2=t2;
        	wcur1++;
        	wcur2++;
        }

        // Extract the result
		tmp = w2+d;	// Point to result on last line
		for(unsigned i=cs;i!=0;i--)
			*xf++ = *tmp++;


#if 1
        // Finalise: to ensure streamability, move the last d samples of each line back at position 0
        for(int ni=0;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
        {
            for(int i=0;i<d;i++)       // Last 4 samples
            {
                w[ni*stride+i] = w[ni*stride+CICBUFSIZ+i];
            }
        }
#endif

#if MEGALOLDSPDEBUG==1
        fprintf(file_pri,"Data updated\n");
        cic_4_2_short_stream_print(w,n,d,CICBUFSIZ);
#endif

        // Decrease amount of data left to process
        size-=cs;
    }
}
 //__attribute__((optimize("unroll-loops")))
 void cic_4_2_short_stream_process2(short * restrict w,short * restrict xf)
 {
 #if MEGALOLDSPDEBUG==1
     fprintf(file_pri,"CIC\n");
 #endif

     // Allocate memory for the processing
     const int d=4;                    // d: window size
     const int n=2;                    // n: number of stages
     const int stride = CICBUFSIZ+d;      // spacing between lines in w

     // Allocate an array with n+1 lines of size+d elements.
     //short w[3*(CICBUFSIZ+4)];

     short *restrict tmp;
     short *restrict w0 = w;			// Input data
     short *restrict w1 = w0+stride;	// Stage 1
     short *restrict w2 = w1+stride;	// Stage 2, output


 #if MEGALOLDSPDEBUG==1
     fprintf(file_pri,"Initialisation\n");
     cic_4_2_short_stream_print(w,n,d,CICBUFSIZ);
 #endif


#if MEGALOLDSPDEBUG==1
	 fprintf(file_pri,"Processing block: cs=%d\n",CICBUFSIZ);
#endif



	 // Implemtation of the CIC
	/*for(int i=d;i<d+cs;i++)       // Iterate samples starting at sample d (first sample)
	{
		for(int ni=1;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
		{
			 //fprintf(1,'Prev out: %d. Adding %d, subbing %d\n',out(1,i-1),x(i),x(i-d+1));
			 short t=0;
			 t = w[ni*stride+i-1];
			 t += w[(ni-1)*stride+i];
			 t -= w[(ni-1)*stride+i-d];
			 w[ni*stride+i] = t;
		 }
	 }*/
	 // Initialise the working pointers

#if 0
	 for(unsigned ni=0;ni<n;ni++)	// n iterations (n=2)
	{
		short * restrict wcur = w1+ni*stride+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
		//short * restrict wprev = w0+ni*stride+d;		// Previous line. Points to line 0 (input) then 1.
		//short * restrict wprevd = w0+ni*stride;		// Previous line but d samples earlier. Points to line 0 (input) then 1.

		/*for(int ni=1;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
		{
			//fprintf(1,'Prev out: %d. Adding %d, subbing %d\n',out(1,i-1),x(i),x(i-d+1));
			short t=0;
			t = w[ni*stride+i-1];
			t += w[(ni-1)*stride+i];
			t -= w[(ni-1)*stride+i-d];
			w[ni*stride+i] = t;
		}*/
		short t = *(wcur-1);
		for(unsigned i=cs;i!=0;i--)     // Iterate samples
		{
			t += *(wcur-stride);
			t -= *(wcur-stride-d);
			*wcur=t;
			wcur++;
			//wprev++;
			//wprevd++;
		}
	}
#endif
	 /*short * restrict wcur0 = w0;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
	 short * restrict wcur1 = w1;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
	 short * restrict wcur2 = w2+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
	 short t1 = *(wcur1-1);
	 short t2 = *(wcur2-1);*/
	 /*for(unsigned i=CICBUFSIZ;i!=0;i--)     // Iterate samples
	 {
		//t1 += *(wcur1-stride) - *(wcur1-stride-d);
		t1 += *(wcur0) - *(wcur0-d);

		t2 += t1 - *(wcur1-d);
		//t2 -= *(wcur2-stride-d);
		*wcur1++=t1;
		*wcur2++=t2;
		wcur0++;
	 }*/
	 /*for(unsigned i=0;i<CICBUFSIZ;i++)     // Iterate samples
	 {
		//t1 += *(wcur1-stride) - *(wcur1-stride-d);
		t1 += wcur0[i+4] - wcur0[i];

		t2 += t1 - wcur1[i];

		//t2 -= *(wcur2-stride-d);
		wcur1[i+4]=t1;
		wcur2[i]=t2;
	 }*/
	 short * restrict wcur0 = w0;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
	 short * restrict wcur1 = w1+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).

	 short t1 = *(wcur1-1);

	 //for(unsigned i=0;i<CICBUFSIZ;i++)     // Iterate samples
	 for(unsigned i=CICBUFSIZ;i!=0;i--)     // Iterate samples
	 {
		 t1+=*(wcur0+4) - *(wcur0);
		 *wcur1++=t1;
		 wcur0++;
	 }
	 wcur1 = w1;
	 short * restrict wcur2 = w2+d;		// "Current" line of the stage where data is computed. Points to line 1 then 2 (output).
	 short t2 = *(wcur2-1);
	 //for(unsigned i=0;i<CICBUFSIZ;i++)     // Iterate samples
	 for(unsigned i=CICBUFSIZ;i!=0;i--)     // Iterate samples
	 {
		 t2+=*(wcur1+4) - *(wcur1);
		 *wcur2++=t2;
		 wcur1++;
	 }



	 // Extract the result
	tmp = w2+d;	// Point to result on last line
	/*for(unsigned i=CICBUFSIZ;i!=0;i--)
		*xf++ = *tmp++;*/
	// Decimate
	for(unsigned i=CICBUFSIZ;i!=0;i-=2)
	{
			*xf++ = *tmp++;
			tmp++;
	}


#if 0
	 // Finalise: to ensure streamability, move the last d samples of each line back at position 0
	 for(int ni=0;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
	 {
		 for(int i=0;i<d;i++)       // Last 4 samples
		 {
			 w[ni*stride+i] = w[ni*stride+CICBUFSIZ+i];
		 }
	 }
#endif

#if MEGALOLDSPDEBUG==1
	 fprintf(file_pri,"Data updated\n");
	 cic_4_2_short_stream_print(w,n,d,CICBUFSIZ);
#endif

}

/*
    CIC filter
    Parameters:
 *      x: input
 *      xf output (pre-allocated by caller
*/
double *cic_4_2(double *x,int size,double *xf)
{
    fprintf(file_pri,"CIC\n");

    // Allocate memory for the processing
    int d=4;                // d: window size
    int n=2;                // n: number of stages
    int stride = size+d;    // spacing between lines in w

    // Allocate an array with n+1 lines of size+d elements.
    double *w = (double*)malloc( (size+d)*(n+1) * sizeof(double) );

    // Set all to zero
    for(int ni=0;ni<n+1;ni++)
    {
        for(int i=0;i<stride;i++)
        {
            w[ni*stride+i] = 0;
        }
    }

    // Copy the input data at the first line in w, offseted by d
    for(int i=0;i<size;i++)
        w[d+i]=x[i];

    // Implemtation of the CIC
    for(int i=d;i<stride;i++)       // Iterate samples starting at sample d (first sample)
    {
        for(int ni=1;ni<n+1;ni++)   // Iterate blocks starting at line 1 (first block)
        {
            //fprintf(1,'Prev out: %d. Adding %d, subbing %d\n',out(1,i-1),x(i),x(i-d+1));
            double t=0;
            t = w[ni*stride+i-1];
            t += w[(ni-1)*stride+i];
            t -= w[(ni-1)*stride+i-d];
            w[ni*stride+i] = t;

        }
    }

    // Extract the result
    for(int i=0;i<size;i++)
    {
        fprintf(file_pri,"i: %d %f\n",i,w[n*stride+d+i]);
        xf[i] = w[n*stride+d+i];
    }

    return w;

}
#endif

/******************************************************************************
	HARDCIC - TEST FUNCTIONS - HARDCIC - TEST FUNCTIONS - HARDCIC - TEST FUNCTIONS -
********************************************************************************/


void hardcic_test_functional_wsingle_rcpu(unsigned downsampling,unsigned order,short *in1,short *in2,int len)
{
	// Write data with CPU readout
	fprintf(file_pri,"====== Testing hardware CIC: single write, CPU read. Downsampling: %d. Order: %d ======\n",downsampling,order);
	//unsigned stm_hardcic_init(unsigned cicchannel,unsigned winsize,unsigned order,unsigned interleaved,unsigned dma,unsigned msbonly);
	stm_hardcic_init(0,downsampling-1,order,0,0,0);			// Channel 0
	stm_hardcic_init(1,downsampling-1,order,0,0,0);			// Channel 1


	unsigned r1,r2;
	for (int i = 0; i < len; i++)
	{
		fprintf(file_pri,"%02d. Prior write DMA4 CNDTR: %05d. DMA5 CNDTR: %05d  -|||-  ",i,(unsigned)DMA1_Channel4->CNDTR,(unsigned)DMA1_Channel5->CNDTR);

		// Read reg
		unsigned isr1a = DFSDM1_Filter0->FLTISR&2;
		DFSDM1_Channel0->CHDATINR=in1[i];
		unsigned isr1b = DFSDM1_Filter0->FLTISR&2;

		unsigned isr2a = DFSDM1_Filter1->FLTISR&2;
		DFSDM1_Channel1->CHDATINR=in2[i];
		unsigned isr2b = DFSDM1_Filter1->FLTISR&2;



		// Change the offset halfway through
		/*if(i==len/2)
		{
			stm_hardcic_offset(0,-100);
			stm_hardcic_offset(1,+100);
		}*/

		HAL_Delay(1);		// Either delay, or polling wait for completion.
		r1 = DFSDM1_Filter0->FLTRDATAR;
		r2 = DFSDM1_Filter1->FLTRDATAR;
		unsigned isr1c = DFSDM1_Filter0->FLTISR&2;
		unsigned isr2c = DFSDM1_Filter1->FLTISR&2;
		unsigned s2 = DMA1->ISR;
		fprintf(file_pri,"%05d | %05d. After write CNDTR: %05d|%05d. Out: %08x (%03d) | %08x (%03d). DMAISR: %08x (%x | %x)\n",in1[i],in2[i],(unsigned)DMA1_Channel4->CNDTR,(unsigned)DMA1_Channel5->CNDTR,r1,r1>>8,r2,r2>>8,s2,(s2>>12)&0b1111,(s2>>16)&0b1111);
		fprintf(file_pri,"ISR: %d %d %d %d %d %d\n",isr1a,isr1b,isr2a,isr2b,isr1c,isr2c);
	}
}
void hardcic_test_functional_winterleaved_rcpu(unsigned downsampling,unsigned order,short *in1,short *in2,int len)
{
	// Write data with CPU readout
	fprintf(file_pri,"====== Testing hardware CIC: interleaved write, CPU read. Downsampling: %d. Order: %d ======\n",downsampling,order);
	//unsigned stm_hardcic_init(unsigned cicchannel,unsigned winsize,unsigned order,unsigned interleaved,unsigned dma,unsigned msbonly);
	stm_hardcic_init(0,downsampling-1,order,1,0,0);			// Channel 0
	stm_hardcic_init(1,downsampling-1,order,1,0,0);			// Channel 1

	unsigned r1,r2;
	for (int i = 0; i < len; i+=2)
	{
		fprintf(file_pri,"%02d. Prior write DMA4 CNDTR: %05d. DMA5 CNDTR: %05d  -|||-  ",i,(unsigned)DMA1_Channel4->CNDTR,(unsigned)DMA1_Channel5->CNDTR);

		// Compose the 32-bit write
		uint32_t s1,s2;
		s1 = ((unsigned int)in1[i+0])|(((unsigned int)in1[i+1])<<16);
		s2 = ((unsigned int)in2[i+0])|(((unsigned int)in2[i+1])<<16);

		// Read reg
		unsigned isr1a = DFSDM1_Filter0->FLTISR&2;
		DFSDM1_Channel0->CHDATINR=s1;
		unsigned isr1b = DFSDM1_Filter0->FLTISR&2;

		unsigned isr2a = DFSDM1_Filter1->FLTISR&2;
		DFSDM1_Channel1->CHDATINR=s2;
		unsigned isr2b = DFSDM1_Filter1->FLTISR&2;



		// Change the offset halfway through
		/*if(i==len/2)
		{
			stm_hardcic_offset(0,-100);
			stm_hardcic_offset(1,+100);
		}*/

		HAL_Delay(1);		// Either delay, or polling wait for completion.
		r1 = DFSDM1_Filter0->FLTRDATAR;
		r2 = DFSDM1_Filter1->FLTRDATAR;
		unsigned isr1c = DFSDM1_Filter0->FLTISR&2;
		unsigned isr2c = DFSDM1_Filter1->FLTISR&2;
		unsigned isr = DMA1->ISR;
		fprintf(file_pri,"%08x (%d %d) | %08x (%d %d). After write CNDTR: %05d|%05d. Out: %08x (%03d) | %08x (%03d). DMAISR: %08x (%x | %x)\n",(unsigned)s1,in1[i+0],in1[i+1],(unsigned)s2,in2[i+0],in2[i+1],(unsigned)DMA1_Channel4->CNDTR,(unsigned)DMA1_Channel5->CNDTR,r1,r1>>8,r2,r2>>8,isr,(isr>>12)&0b1111,(isr>>16)&0b1111);
		fprintf(file_pri,"ISR: %d %d %d %d %d %d\n",isr1a,isr1b,isr2a,isr2b,isr1c,isr2c);
	}
}

#define DMABUFSIZE 128
void hardcic_test_functional_wsingle_rdma(unsigned downsampling,unsigned order,short *in1,short *in2,int len)
{
	int dmaout1[DMABUFSIZE],dmaout2[DMABUFSIZE];

	fprintf(file_pri,"====== Testing hardware CIC: single write, DMA read. Downsampling: %d. Order: %d ======\n",downsampling,order);

	//unsigned stm_hardcic_init(unsigned cicchannel,unsigned winsize,unsigned order,unsigned interleaved,unsigned dma,unsigned msbonly);
	stm_hardcic_init(0,downsampling-1,order,0,1,0);			// Channel 0
	stm_hardcic_init(1,downsampling-1,order,0,1,0);			// Channel 1

	// DMA read setup
	for(int i=0;i<DMABUFSIZE;i++)
	{
		dmaout1[i] = dmaout2[i] = 0x5555aaaa;
	}



	// do twice
	//for(int j=0;j<2;j++)
	{
		stm_hardcic_process_dma_setup(0,(unsigned *)dmaout1,len/downsampling);
		stm_hardcic_process_dma_setup(1,(unsigned *)dmaout2,len/downsampling);

		// Write data with DMA readout
		unsigned cnt[2][40];
		for (int i = 0; i < len; i++)
		{
			DFSDM1_Channel0->CHDATINR=in1[i];
			//HAL_Delay(1);
			DFSDM1_Channel1->CHDATINR=in2[i];
			HAL_Delay(1);
			cnt[0][i] = DMA1_Channel4->CNDTR;
			cnt[1][i] = DMA1_Channel5->CNDTR;
		}
		// Wait
		fprintf(file_pri,"DMA transfer left 0: %d\n",stm_hardcic_process_dma_remaining(0));
		fprintf(file_pri,"DMA transfer left 1: %d\n",stm_hardcic_process_dma_remaining(1));
		fprintf(file_pri,"Wait DMA completion 0\n");
		stm_hardcic_process_dma_waitcomplete(0);
		fprintf(file_pri,"Wait DMA completion 1\n");
		stm_hardcic_process_dma_waitcomplete(1);

		fprintf(file_pri,"DMA out 1:"); for(int i=0;i<len;i++) fprintf(file_pri,"%08x ",dmaout1[i]); fprintf(file_pri,"\n");
		fprintf(file_pri,"DMA out 1:"); for(int i=0;i<len;i++) fprintf(file_pri,"%05d ",dmaout1[i]>>8); fprintf(file_pri,"\n\n");
		fprintf(file_pri,"DMA out 2:"); for(int i=0;i<len;i++) fprintf(file_pri,"%08x ",dmaout2[i]); fprintf(file_pri,"\n");
		fprintf(file_pri,"DMA out 2:"); for(int i=0;i<len;i++) fprintf(file_pri,"%05d ",dmaout2[i]>>8); fprintf(file_pri,"\n\n");

		fprintf(file_pri,"CNDTR DMA1:"); for(int i=0;i<len;i++) fprintf(file_pri,"%02d ",cnt[0][i]); fprintf(file_pri,"\n");
		fprintf(file_pri,"CNDTR DMA2:"); for(int i=0;i<len;i++) fprintf(file_pri,"%02d ",cnt[1][i]); fprintf(file_pri,"\n");
	}
}
void hardcic_test_functional_winterleaved_rdma(unsigned downsampling,unsigned order,short *in1,short *in2,int len)
{
	int dmaout1[DMABUFSIZE],dmaout2[DMABUFSIZE];

	fprintf(file_pri,"====== Testing hardware CIC: interleaved write, DMA read. Downsampling: %d. Order: %d ======\n",downsampling,order);

	//unsigned stm_hardcic_init(unsigned cicchannel,unsigned winsize,unsigned order,unsigned interleaved,unsigned dma,unsigned msbonly);
	stm_hardcic_init(0,downsampling-1,order,1,1,0);			// Channel 0
	stm_hardcic_init(1,downsampling-1,order,1,1,0);			// Channel 1

	// DMA read setup
	for(int i=0;i<DMABUFSIZE;i++)
	{
		dmaout1[i] = dmaout2[i] = 0x5555aaaa;
	}



	// do twice
	//for(int j=0;j<2;j++)
	{
		stm_hardcic_process_dma_setup(0,(unsigned *)dmaout1,len/downsampling);
		stm_hardcic_process_dma_setup(1,(unsigned *)dmaout2,len/downsampling);

		// Write data with DMA readout
		unsigned cnt[2][40];
		for (int i = 0; i < len; i+=2)
		{
			// Compose the 32-bit write
			uint32_t s1,s2;
			s1 = ((unsigned int)in1[i+0])|(((unsigned int)in1[i+1])<<16);
			s2 = ((unsigned int)in2[i+0])|(((unsigned int)in2[i+1])<<16);


			DFSDM1_Channel0->CHDATINR=s1;
			DFSDM1_Channel1->CHDATINR=s2;
			HAL_Delay(1);
			HAL_Delay(1);
			cnt[0][i/2] = DMA1_Channel4->CNDTR;
			cnt[1][i/2] = DMA1_Channel5->CNDTR;
		}
		// Wait
		fprintf(file_pri,"DMA transfer left 0: %d\n",stm_hardcic_process_dma_remaining(0));
		fprintf(file_pri,"DMA transfer left 1: %d\n",stm_hardcic_process_dma_remaining(1));
		fprintf(file_pri,"Wait DMA completion 0\n");
		stm_hardcic_process_dma_waitcomplete(0);
		fprintf(file_pri,"Wait DMA completion 1\n");
		stm_hardcic_process_dma_waitcomplete(1);

		fprintf(file_pri,"DMA out 1:"); for(int i=0;i<len/2;i++) fprintf(file_pri,"%08x ",dmaout1[i]); fprintf(file_pri,"\n");
		fprintf(file_pri,"DMA out 1:"); for(int i=0;i<len/2;i++) fprintf(file_pri,"%05d ",dmaout1[i]>>8); fprintf(file_pri,"\n\n");
		fprintf(file_pri,"DMA out 2:"); for(int i=0;i<len/2;i++) fprintf(file_pri,"%08x ",dmaout2[i]); fprintf(file_pri,"\n");
		fprintf(file_pri,"DMA out 2:"); for(int i=0;i<len/2;i++) fprintf(file_pri,"%05d ",dmaout2[i]>>8); fprintf(file_pri,"\n\n");

		fprintf(file_pri,"CNDTR DMA1:"); for(int i=0;i<len/2;i++) fprintf(file_pri,"%02d ",cnt[0][i]); fprintf(file_pri,"\n");
		fprintf(file_pri,"CNDTR DMA2:"); for(int i=0;i<len/2;i++) fprintf(file_pri,"%02d ",cnt[1][i]); fprintf(file_pri,"\n");
	}
}
/*
	Generate test signal for CIC
*/
void hardcic_test_gendata1(short *in,short *in2,int len)
{
	// Initialise the data to send
	for (int i = 0; i < len; i++)
	{
		int v;
		if(i<=21)
			v=i+1;	// Start at 1.
		else
			v=0;
		in[i] = v;
		in2[i] = 100+i;
	}
	fprintf(file_pri,"Input data 1: "); print_array_short(file_pri,in,len);
	fprintf(file_pri,"Input data 2: "); print_array_short(file_pri,in2,len);
}
void hardcic_test_gendata2(short *in,short *in2,int len)
{
	// Initialise the data to send
	for (int i = 0; i < len; i++)
	{
		in[i] = (i&0xFF);		// counter modulo 256
		if(in2)
			in2[i] = (i&0xFF);		// counter modulo 256
	}
	fprintf(file_pri,"Input data 1: "); print_array_short(file_pri,in,len);
	if(in2)
	{
		fprintf(file_pri,"Input data 2: "); print_array_short(file_pri,in2,len);
	}
}





void hardcic_test_functional()
{
	// Input data
	short in[40],in2[40];
	int len=sizeof(in)/sizeof(unsigned);

	fprintf(file_pri,"Input data length: %d\n",len);
	hardcic_test_gendata1(in,in2,len);


	unsigned downsampling = 2;
	unsigned order = 5;








	hardcic_test_functional_wsingle_rcpu(downsampling,order,in,in2,len);
	hardcic_test_functional_winterleaved_rcpu(downsampling,order,in,in2,len);
	hardcic_test_functional_wsingle_rdma(downsampling,order,in,in2,len);
	hardcic_test_functional_winterleaved_rdma(downsampling,order,in,in2,len);



	fprintf(file_pri,"------------------------------------\n");
	//hardcic_bench_singlew_dmaread();
	//dsp_real_to_iq_4x_cic();
}


/******************************************************************************
	HARDCIC - BENCHMARK - HARDCIC - BENCHMARK - HARDCIC - BENCHMARK - HARDCIC -
********************************************************************************/

void hardcic_bench()
{
	hardcic_bench_wsingle_rcpu();
}

void hardcic_bench_wsingle_rcpu()
{
	// Assume initialised
	short adc[ADCFAST_BUFFERSIZE];
	int cicout[ADCFAST_BUFFERSIZE];

	unsigned len = ADCFAST_BUFFERSIZE;
	unsigned long int t_last,t_cur;
	unsigned long int nsample;
	const unsigned long mintime=1;
	unsigned long tint1,tint2;


	unsigned downsampling=2;
	unsigned order=5;
	unsigned cicchannel=0;

	hardcic_test_gendata2(adc,0,len);
	for(unsigned i=0;i<len;i++)
		cicout[i]=0xffff;


	stm_hardcic_process_stream_16_32_cpu_init(cicchannel,downsampling,order);

	t_last=timer_s_wait();
	tint1=timer_ms_get_intclk();
	nsample=0;
	unsigned nr=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		nr = stm_hardcic_process_stream_16_32_cpu_process(cicchannel,adc,cicout,len);
		nsample++;
	}
	tint2=timer_ms_get_intclk();


	fprintf(file_pri,"Time: %lds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"Read out: %d samples\n",nr);

	// Print output data
	fprintf(file_pri,"Output:"); for(int i=0;i<len/downsampling;i++) fprintf(file_pri,"%08x ",cicout[i]); fprintf(file_pri,"\n");
	fprintf(file_pri,"Output:"); for(int i=0;i<len/downsampling;i++) fprintf(file_pri,"%05d ",cicout[i]>>8); fprintf(file_pri,"\n\n");
}

/******************************************************************************
	HARDCIC - OTHER - HARDCIC - OTHER - HARDCIC - OTHER - HARDCIC - OTHER -
********************************************************************************/


#if 0
__attribute__((optimize("unroll-loops")))
void hardcic_bench_interleavedw()
{
	// Assume initialised
	short adc[CICBUFSIZ];
	short cicout[CICBUFSIZ];

	unsigned long int t_last,t_cur;
	unsigned long int ctr,cps,nsample;
	const unsigned long mintime=1;
	unsigned long tint1,tint2;

	t_last=timer_s_wait();
	tint1=timer_ms_get_intclk();
	nsample=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		/*for(int i=0;i<CICBUFSIZ;i+=2)
		{
			// Push data: two data in for one data out
			DFSDM1_Channel0->CHDATINR=adc[i];
			DFSDM1_Channel0->CHDATINR=adc[i+1];
			cicout[i] = DFSDM1_Filter2->FLTRDATAR;
		}*/
		unsigned *restrict in = (unsigned *)adc;
		short * restrict out = (short *restrict)cicout;
		unsigned i = CICBUFSIZ/2;
		for(;i!=0;i--)
		{
			DFSDM1_Channel0->CHDATINR=adc[i];
			*out++ = DFSDM1_Filter2->FLTRDATAR;
		}
		/*short * restrict in = adc;
		short * restrict out = cicout;

		for(unsigned i=CICBUFSIZ;i!=0;i--)
		{
			// Push data: two data in for one data out
			DFSDM1_Channel0->CHDATINR=*in++;
			DFSDM1_Channel0->CHDATINR=*in++;
			*out++ = DFSDM1_Filter2->FLTRDATAR;
		}*/
		nsample++;
	}
	tint2=timer_ms_get_intclk();


	fprintf(file_pri,"Time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));


}

/*
	Use new API to readout data
*/
//__attribute__((optimize("unroll-loops")))
void hardcic_bench_singlew_dmaread()
{
#if 0
	// Assume initialised
	short adc[CICBUFSIZ];
	unsigned cicout[CICBUFSIZ+10];
	//
	unsigned downsampling = 2;
	int len=CICBUFSIZ/downsampling;		// Careful to ensure integer division. Len is number of output data

	unsigned long int t_last,t_cur;
	unsigned long int ctr,cps,nsample;
	const unsigned long mintime=1;
	unsigned long tint1,tint2;

	fprintf(file_pri,"Benchmarking hardCIC, dma out\n");

	// Init CIC
	stm_hardcic_init(0,downsampling-1,2,0,1,0);
	// Init buffers
	// Fill  out buffer to verify DMA output
	for(int i=0;i<len+10;i++) cicout[i] = 0x5555aaaa;
	for(int i=0;i<len;i++) fprintf(file_pri,"%08x ",cicout[i]); fprintf(file_pri,"\n");
	for(int i=len;i<len+10;i++) fprintf(file_pri,"%08x ",cicout[i]); fprintf(file_pri,"\n");

	// Feed data
	t_last=timer_s_wait();
	tint1=timer_ms_get_intclk();
	nsample=0;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		// Init processing: awaits len words in dmaout
		stm_hardcic_process_setup(0,cicout,len);

		//for(int i=0;i<CICBUFSIZ;i+=4)
		//for(int i=0;i<CICBUFSIZ;i+=2)
		for(int i=0;i<CICBUFSIZ;i+=1)
		{
			// Push data
			DFSDM1_Channel0->CHDATINR=adc[i];
			//DFSDM1_Channel0->CHDATINR=adc[i+1];
			//DFSDM1_Channel0->CHDATINR=adc[i+2];
			//DFSDM1_Channel0->CHDATINR=adc[i+3];

		}
		// Wait for dma completion
		//fprintf(file_pri,"Wait DMA completion\n");
		// Read status
		/*fprintf(file_pri,"DMA1_Channel4 CNDTR: %08x\n",DMA1_Channel4->CNDTR);
		fprintf(file_pri,"DMA1->ISR: %08x\n",DMA1->ISR);
		fprintf(file_pri,"DMA1->IFCR: %08x\n",DMA1->IFCR);*/
		stm_hardcic_process_waitcomplete(0);
		nsample++;
	}
	tint2=timer_ms_get_intclk();


	fprintf(file_pri,"Time: %ds (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,nsample/(t_cur-t_last));

	// Verify DMA buffer
	fprintf(file_pri,"DMA buffer:");
	for(int i=0;i<len;i++) fprintf(file_pri,"%08x ",cicout[i]); fprintf(file_pri,"\n");
	for(int i=len;i<len+10;i++) fprintf(file_pri,"%08x ",cicout[i]); fprintf(file_pri,"\n");

#endif

}
#endif






#if 0
void megalol_dsp_hardcic_irq()
{
	//unsigned r = _stm_hardcic_filter0->FLTRDATAR;
	unsigned s2 = DMA1->ISR;
	fprintf(file_pri,"DMAI: DMAISR: %08x (%x %d)\n",s2,(s2>>12)&0b1111,(s2>>16)&0b1111);
	DMA1->IFCR|=(1<<12) | (1<<16);			// GIF4|GIF5

}
#endif
