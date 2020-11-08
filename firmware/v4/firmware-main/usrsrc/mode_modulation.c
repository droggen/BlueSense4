/*
 * mode_modulation.c
 *
 *  Created on: 10 oct. 2020
 *      Author: droggen
 */

#include <stdio.h>
#include <string.h>
#include "global.h"
#include "mode_main.h"
#include "command.h"
#include "commandset.h"
#include "mode_modulation.h"
#include "mode.h"
#include "stmdac.h"
#include "wait.h"
#include "stmdfsdm.h"
#include "kiss_fft.h"


const COMMANDPARSER CommandParsersModulation[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{'1', CommandParserV21,"V21 modulation"},
	{'2', CommandParserV21Demod,"V21 demodulation"},
	{'S', CommandParserModulationStop,"Stop modulation"},
	{'B', CommandParserModulationBit,"B,<0|1> Modulate a 0 or 1"},
	{'K', CommandParserV21Bench,"Benchmark V21 modulation"},
	{'k', CommandParserV21Bench2,"Another benchmark V21 modulation"},
};
const unsigned char CommandParsersModulationNum=sizeof(CommandParsersModulation)/sizeof(COMMANDPARSER);
// THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG
#define V21_PAYLOAD_MAX 	64
//#define V21_IDLE_WAIT		5			// Number of bits to wait between stop and start
//#define V21_IDLE_WAIT		2			// Number of bits to wait between stop and start
#define V21_IDLE_WAIT		2			// Number of bits to wait between stop and start

unsigned v21_bps;
unsigned v21_dac_lut_incr[2];			// LUT increment (phase increment) in 16.16 for symbol 0 and 1
unsigned v21_dac_lut_index;				// A single index is needed. Format: 16.16
unsigned v21_bit_index_increment;		// How many fractional bits to advance the index in 16.16 format. When v21_bit_index_increment>=65536 go to next bit
unsigned v21_bit_index;					// Current bit index in 16.16 format
unsigned v21_bit_current;				// Current bit in the bitstream - this is the effective bit to be transmitted now
unsigned char v21_payload[V21_PAYLOAD_MAX];			// Data to transmit
unsigned char _v21_siggen_bit_bit;		// Bit to transmit by the v21_siggen_bit function

void mode_modulation(void)
{
	mode_run("MODULATION",CommandParsersModulation,CommandParsersModulationNum);
}

unsigned char CommandParserModeModulation(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	// Change to modulation mode
	CommandChangeMode(APP_MODE_MODULATION);
	return 0;
}

void v21_init()
{
	//unsigned sr = 16000;
	unsigned sr = 32000;
	//unsigned sr = 100000;
	//unsigned sr = 200000;
	//unsigned sr = 4000;

	// Ensure DAC is off.
	stmdac_deinit();

	// Start generating at the specified sample rate with the built-in cosine generator
	//stmdac_init(sr,v21_siggen);
	stmdac_init(sr,v21_siggen_8n1);

	fprintf(file_pri,"DAC Clock: %u\n",_stm_dac_dacclock);

	// Init the increments based on sample rate
	v21_dac_lut_incr[0] = _stm_dac_computeincr(1080+100,_stm_dac_dacclock,STMDAC_LUTSIZ);
	v21_dac_lut_incr[1] = _stm_dac_computeincr(1080-100,_stm_dac_dacclock,STMDAC_LUTSIZ);
	// Get effective frequency
	fprintf(file_pri,"Frequency 0: %u\n",_stm_dac_computefrqfrominc(v21_dac_lut_incr[0],_stm_dac_dacclock,STMDAC_LUTSIZ));
	fprintf(file_pri,"Frequency 1: %u\n",_stm_dac_computefrqfrominc(v21_dac_lut_incr[1],_stm_dac_dacclock,STMDAC_LUTSIZ));
	// Init the index
	v21_dac_lut_index=0;
	//
	//v21_bps = 300;
	//v21_bps = 30;
	v21_bps = 15;
	//v21_bps = 1;

	// Calculate how many samples lead to a new bit in .16 format
	// sr=16KHz. bps=300 -> sr/bps = 16KHz/300 = 53.3 samples for a bit -> bit increment: bps/sr = 1/53.3 -> new bit whenever bit index >= 1
	// In .16 format: bit increment = bps*65536/sr = 1228. Round to nearest: bit increment [bps*65536+sr/2]/sr
	// In .16 format: bit increment = 1/53.3*65536 -> new bit whenever bit index >= 65536
	// Calculate the fractional bit increment.
	v21_bit_index_increment = (v21_bps*65536+(_stm_dac_dacclock>>1))/_stm_dac_dacclock;
	fprintf(file_pri,"Bit index increment: %u\n",v21_bit_index_increment);
	v21_bit_index=0;
	v21_bit_current=0;

	memset(v21_payload,0,V21_PAYLOAD_MAX);
	for(int i=0;i<4;i++)
		v21_payload[i]=0xaa;
	for(int i=0;i<4;i++)
			v21_payload[4+i]=0x55;
	//strcpy(v21_payload+8,"HELLO!");
	strcpy(v21_payload+8,"THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG!");		// 44
	v21_payload[51] = 0xff;
	v21_payload[52] = 0xff;
	v21_payload[53] = 0xff;
	v21_payload[54] = 0xff;
	v21_payload[55] = 0x00;
	v21_payload[56] = 0x00;
	v21_payload[57] = 0x00;
	v21_payload[58] = 0x00;
	v21_payload[59] = 0xff;
	v21_payload[60] = 0xff;
	v21_payload[61] = 0xff;
	v21_payload[62] = 0xff;
	v21_payload[63] = 0x1A;

}

unsigned char CommandParserModulationBit(char *buffer,unsigned char size)
{
	(void) size;
	unsigned bit;

	if(ParseCommaGetInt(buffer,1,&bit))
		return 2;

	if(bit>1)
		return 2;

	_v21_siggen_bit_bit = bit;
	v21_dac_lut_index=0;
	v21_dac_lut_incr[0]=v21_dac_lut_incr[1]=0;



	unsigned sr = 32000;
	//unsigned sr = 100000;
	//unsigned sr = 200000;
	//unsigned sr = 4000;

	// Ensure DAC is off.
	stmdac_deinit();

	// Start generating at the specified sample rate with the built-in cosine generator
	stmdac_init(sr,v21_siggen_bit);

	fprintf(file_pri,"DAC Clock: %u\n",_stm_dac_dacclock);
	// Init the increments based on sample rate
	v21_dac_lut_incr[0] = _stm_dac_computeincr(1080+100,_stm_dac_dacclock,STMDAC_LUTSIZ);
	v21_dac_lut_incr[1] = _stm_dac_computeincr(1080-100,_stm_dac_dacclock,STMDAC_LUTSIZ);
	// Get effective frequency
	fprintf(file_pri,"Frequency 0: %u\n",_stm_dac_computefrqfrominc(v21_dac_lut_incr[0],_stm_dac_dacclock,STMDAC_LUTSIZ));
	fprintf(file_pri,"Frequency 1: %u\n",_stm_dac_computefrqfrominc(v21_dac_lut_incr[1],_stm_dac_dacclock,STMDAC_LUTSIZ));
	fprintf(file_pri,"Modulating a %d\n",bit);

	return 0;
}

unsigned char CommandParserModulationStop(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	fprintf(file_pri,"Stop modulation\n");

	stmdac_deinit();

	return 0;
}

unsigned char CommandParserV21(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;



	v21_init();


	return 0;
}


unsigned char CommandParserV21Bench(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	// Initialise the DAC generation
	v21_init();
	// Stop the DAC generation, but keep the initialisation parameters
	stmdac_deinit();

	unsigned t1 = timer_ms_get();
	unsigned n=1000;
	for(unsigned i=0;i<n;i++)
	{
		//v21_siggen(_stm_dac_dmabuf,DACDMAHALFBUFSIZ);
		v21_siggen_8n1(_stm_dac_dmabuf,DACDMAHALFBUFSIZ);
	}
	unsigned t2 = timer_ms_get();

	fprintf(file_pri,"Time for %u iterations: %u ms. Each iteration generates %u samples\n",n,t2-t1,DACDMAHALFBUFSIZ);
	fprintf(file_pri,"Max sample per second: %u\n",DACDMAHALFBUFSIZ*n*1000/(t2-t1));

	return 0;
}
unsigned char CommandParserV21Bench2(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	unsigned refperf = system_perfbench(2);
	fprintf(file_pri,"Reference performance: %lu\n",refperf);

	// Initialise and start the DAC generation
	v21_init();

	unsigned perf = system_perfbench(2);

	// Stop the DAC generation
	stmdac_deinit();


	// Compute overhead
	unsigned long o = (refperf-perf)*100/refperf;
	fprintf(file_pri,"Performance with siggen: %lu. Overhead: %lu%%\n",perf,o);


	return 0;
}

/*
	v21_siggen: does not include start and stop bit
*/
void v21_siggen(unsigned short *buffer,unsigned n)
{
	/*for(unsigned i=0;i<n;i++)
		buffer[i]=i&0xfff;*/
	static unsigned curbit=0;
	static unsigned c=0;


	for(unsigned i=0;i<n;i++)
	{
		// Fetch the bit to transmit
		unsigned char byte = v21_payload[(v21_bit_current>>3) & (V21_PAYLOAD_MAX-1)];		// Fetch the current byte transmitted; wraparound at 63 bytes
		unsigned char bit = (byte>>(7-(v21_bit_current&0b111)))&1;							// Fetch the current bit to transmit

		curbit = bit;

		buffer[i] = _stm_dac_cos_lut[v21_dac_lut_index>>16];		// Index is in 16.16 format
		v21_dac_lut_index += v21_dac_lut_incr[curbit];
		v21_dac_lut_index &= STMDAC_LUTINDEXMASK;					// Wraparound

		// Change amplitude
		buffer[i] >>= 4;		// Divide by 8: 0-256
		buffer[i] += 1920;		// Center on 2048

		// Increment the bit
		v21_bit_index += v21_bit_index_increment;
		if(v21_bit_index>=65536)
		{
			/*fprintf(file_pri,"%d ",bit);		// Print the last sent bit
			c++;
			if((c&0b111)==0)
				fprintf(file_pri,"\n");*/



			v21_bit_index-=65536;									// Wrap around keeping fractional part
			//
			// The bit in the bitstream is v21_bit_index>>16
			// The byte within the bitstream is v21_bit_index>>19
			// The bit within the byte is (v21_bit_index>>16)&0b111;

			v21_bit_current++;
			unsigned byteidx = v21_bit_current>>3;
			unsigned bitwithinbyteidx = 7-(v21_bit_current&0b111);		// Starting with msb
			//fprintf(file_pri,"\n%d %d\n",byteidx,bitwithinbyteidx);


			// Toggle bit
			//curbit=1-curbit;
		}
	}
}

/*
	v21_siggen_8n1: 1 start bit, 8 data bit, 1 stop bit


	idle:
		idle=0:	ongoing transmission (any of start, data or stop)
		idle>0:	idle, i.e. waiting for next byt to transmit.
				idle is a counter incremented at each bit up to V21_IDLE_WAIT
				If idle>=V21_IDLE_WAIT+1: attempts to read next byte. Clamp value of idle.
				Otherwise: increment idle.


	- delay in multiple of bits between stop bit and next start bit. controlled by "idle
	-

*/
void v21_siggen_8n1(unsigned short *buffer,unsigned n)
{
	static int idle=1;
	static unsigned short byte8n1;				// 10 bit byte to transfer which is tweaked to include start bit (LSB) data and stop bit (MSB): |stp|data7|...|data0|start|
	static unsigned short byte8n1ctr;			// Counter from 0 to 9 indicating which bit to transmit.
	static unsigned curbit=1;					// Current bit is initially 1 ie stop bit
	static unsigned bitcounter=0;				// Bit counter: every time above 65535 a new bit must be issued.
	static unsigned char byte;

	for(unsigned i=0;i<n;i++)					// Iterate the DAC buffer
	{
		// Wave generation according to current phaseincrement
		buffer[i] = _stm_dac_cos_lut[v21_dac_lut_index>>16];		// Index is in 16.16 format
		v21_dac_lut_index += v21_dac_lut_incr[curbit];				// Increment index in wave generation LUT according to current bit.
		v21_dac_lut_index &= STMDAC_LUTINDEXMASK;					// Wraparound

		// Change amplitude
		buffer[i] >>= 4;		// Divide by 8: 0-256
		buffer[i] += 1920;		// Center on 2048

		// Increment the bit
		bitcounter += v21_bit_index_increment;
		if(bitcounter>=65536)
		{
			bitcounter-=65536;									// Wrap around keeping fractional part
			// Start by fetching new data to transmit if idle
			if(idle)
			{
				// If idle, check if we waited V21_IDLE_WAIT bits before querying next data byte
				// If V21_IDLE_WAIT=0 there is no wait between stop and start bit.
				// In practice this function is called in an ISR therefore if v21_getnext indicates no byte it will always return no byte in subsequent calls -> could be optimised
				if(idle>=V21_IDLE_WAIT+1)
				{

					unsigned rv = v21_getnext(&byte);
					// v21_getnext returns 0 if success, or 1 if no data. If no data, remain in idle mode.
					if(rv==0)
					{
						idle = 0;		// Received data - must transmit now.
						// Manipulate the data to add the start and stop bit. Start bit is LSB and is 0. Stop bit is MSB (bit 9) and high.
						byte8n1 = byte;
						byte8n1<<=1;		// Bit 0 = start = 0
						byte8n1|=0x200;		// Bit 9 = stop = 1
						// Initialise the bit counter within the byte to transfer
						byte8n1ctr=0;
					}
					// Otherwise, we remain in idle.
				}
				else
				{
					idle++;
				}
			}

			// If not idle - get bit to send
			if(idle==0)
			{
				curbit = byte8n1 & 0b1;			// Current bit to transmit, starting from LSB
				byte8n1>>=1;					// Shift right
				byte8n1ctr++;					// byte8n1ctr indicates the count of the currently transmitted bit

				// Debug info
				fprintf(file_pri,"%d ",curbit);	// Print the bit being sent

				if(byte8n1ctr==10)				// If currently transmitting bit 10 flag that we will be idle at the next bit
				{
					if(byte<33 || byte>=128)
						fprintf(file_pri,"[%02X] ",byte);
					else
						fprintf(file_pri,"'%c' ",byte);
					idle=1;
				}

			}
			else
			{
				// If idle nothing to do: curbit will be 1 as an idle line is identical to the stop bit

				// Debug info
				fprintf(file_pri,"* ");			// Indicate idle - bit being sent is anyways 1 in idle.
			}


		}

	}
}

/*
	v21_siggen_bit: generates one bit continuously
*/
void v21_siggen_bit(unsigned short *buffer,unsigned n)
{
	static unsigned curbit=1;					// Current bit is initially 1 ie stop bit

	for(unsigned i=0;i<n;i++)					// Iterate the DAC buffer
	{
		// Wave generation according to current phaseincrement
		buffer[i] = _stm_dac_cos_lut[v21_dac_lut_index>>16];		// Index is in 16.16 format
		v21_dac_lut_index += v21_dac_lut_incr[_v21_siggen_bit_bit];				// Increment index in wave generation LUT according to current bit.
		v21_dac_lut_index &= STMDAC_LUTINDEXMASK;					// Wraparound

		// Change amplitude
		buffer[i] >>= 4;		// Divide by 8: 0-256
		buffer[i] += 1920;		// Center on 2048
	}
}

/*
	v21_getnext: Return the next data byte

	return:		0	A data byte was returned
				1	No data byte available
*/
unsigned v21_getnext(unsigned char *byte)
{
	static unsigned index = 0;
	unsigned rv=0;

	// Simulate "no data" for the last 4 bytes, i.e. index 28 and above
	//if(index>=28)
		//rv = 1;

	*byte=v21_payload[index];
	index=(index+1)&(V21_PAYLOAD_MAX-1);

	fprintf(file_pri,"\n%d: %02X %d\n",index,(unsigned)*byte,rv);
	return rv;
}

unsigned char CommandParserV21Demod(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	STM_DFSMD_TYPE audbuf[STM_DFSMD_BUFFER_SIZE*2];		// Two buffer for audio data
	unsigned long audbufms,audbufpkt;					// Audio metadata
	unsigned char audleftright;							// Audio metadata: left or right mic

	unsigned ws = 128;
	float pdiffhist[8];									// History of power difference of 8 samples - moving average of 8

	unsigned winperhalfbit = 17;
	unsigned winperbit = 33;							// Number of windows with JS of 16 per bit for 15 bps at 8KHz: 8000Hz/15bps/16s = 33.3 windows
	// State:
	//   0=idle:         Wait for falling edge
	//   1=waitmiddle:   Wait for middle of bit by waiting winperbit/2 cycles
	//   2=getbits:      Get the next 9 bits
	unsigned state = 0;          //
	unsigned statectr=0;         // State counter for various purposes
	unsigned bitctr=0;           // Count until 9 bit received
	unsigned frame[9];			// 9-bit frame

	/*


		Code designed with sliding window of 16. Each audio buffer of 128 -> 8 FFT with jumping window
		1 1-128
		2 17-144
		3 33-160
		4 49-176
		5 65-192
		6 81-208
		7 97-224
		8 113-240



		Init: assume 256 samples in audio buffer (technically: empty)
			Get most recent 128 samples aud[128:257] <- getbuf
			Repeat i=0:7 (repeat 8 times):
				FFT(aud[i*16:i*16+127])
				Process result of FFT
			Shift by 128: aud[1:128] <- aud[129:256]
	 */

	kiss_fft_cpx din[ws],dout[ws];

	if(STM_DFSMD_BUFFER_SIZE!=ws)
	{
		fprintf(file_pri,"Error: code designed for audio buffers of %d bytes only\n",ws);
		return 1;
	}


	kiss_fft_cfg cfg = kiss_fft_alloc(ws,0,0,0 );
	if(cfg==0)
	{
		fprintf(file_pri,"FFT memory allocation error\n");
		return 1;
	}

	unsigned fi1 = 17;			// Bin in which the 1 is located: assume 8KHz, ws=128, 980Hz
	unsigned fi0 = 20;			// Bin in which the 0 is located: assume 8KHz, ws=128, 1180Hz

	fprintf(file_pri,"Demodulating: press any key to interrupt\n");


	// Init sampling
	stm_dfsdm_init(STM_DFSMD_INIT_8K,STM_DFSDM_LEFT);

	while(1)
	{
		if(fgetc(file_pri)!=-1)
			break;


		//if(stm_dfsdm_data_getnext(audbuf,&audbufms,&audbufpkt,&audleftright))
		if(stm_dfsdm_data_getnext(&audbuf[STM_DFSMD_BUFFER_SIZE],&audbufms,&audbufpkt,&audleftright))			// Get new data in recent-most 128 samples
			continue;			// No data, continue

		// Jumping window 16 -> 8 iterations
		unsigned tt1 = timer_us_get();
		for(unsigned ji = 0;ji<8;ji++)
		{
			// Copy the data to the kissfft structure
			for(unsigned i=0;i<ws;i++)
			{
				din[i].r = audbuf[ji*16+i];
				din[i].i = 0;
			}

			//unsigned t1 = timer_us_get();
			kiss_fft(cfg,din,dout);
			//unsigned t2 = timer_us_get();

			//fprintf(file_pri,"JI %d: FFT: %u us\n",ji,t2-t1);

			// Compute power only on bin 0 and 1
			float p0 = dout[fi0].r*dout[fi0].r+dout[fi0].i*dout[fi0].i;
			float p1 = dout[fi1].r*dout[fi1].r+dout[fi1].i*dout[fi1].i;



			// Moving average on 8 bit power
			// Shift out pdiffist
			for(int pi = 0;pi<7;pi++)
				pdiffhist[pi]=pdiffhist[pi+1];
			pdiffhist[7] = p1-p0;

			// Moving average
			float pcur = 0;
			for(int pi = 0;pi<8;pi++)
				pcur += pdiffhist[pi];

			// Current bit value
			unsigned bit = pcur>0;

			//fprintf(file_pri,"\t%f %f %d\t %d\n",p0,p1,(p1>p0)?1:0,bit);
			//fprintf(file_pri,"%d %d. %d. %d\n",state,statectr,bitctr,bit);


			// Run state machine
			switch(state)
			{
			case 0:
				// Idle: detect if falling edge
				if(bit==0)
				{
					// Detected a falling edge
					fprintf(file_pri,"\\");

					// Need to wait winperbit/2 for the "middle" of the bit
					state = 1;
					statectr=0;
				}
				// If bit isn't 0, do nothing.
				break;
			case 1:
				// Waitmiddle
				// Wait for the middle of the bit
				statectr=statectr+1;
				if(statectr == winperhalfbit)
				{
					// Middle of bit - check value to see if still 0
					if(bit == 0)
					{
						// Still 0: could be valid start bit
						state = 2;
						statectr=0;
						bitctr=0;

						fprintf(file_pri,"*");
					}
					else
					{
						// Not zero: frame error, return to idle
						fprintf(file_pri,"FE\n");
						state = 0;
					}
				}
				break;
			case 2:
				// getbits: get 9 bits
				statectr=statectr+1;
				if(statectr==winperbit)
				{
					// Middle of next bit - read it
					frame[bitctr] = bit;
					bitctr=bitctr+1;
					statectr=0;
					// When 9 bits received check and return to idle
					if(bitctr==9)
					{
						if(frame[8] == 1)
						{
							// Last bit is 1: potentially valid frame!
							// Store time at which received and bits
							// Print the frame
							for(unsigned fi=0;fi<9;fi++)
							{
								fprintf(file_pri,"%d ",frame[fi]);
							}
							// Decode number
							unsigned n=0;
							for(int fi=7;fi>=0;fi--)
							{
								n<<=1;
								n|=frame[fi];
							}
							if(n<33 || n>128)
								fprintf(file_pri,"[%02X]",n);
							else
								fprintf(file_pri,"%c",n);

							fprintf(file_pri,"\n");
						}
						else
						{
							// Discard
							fprintf(file_pri,"FE\n");
						}


						// Return to idle
						state = 0;

					}
				}
				break;
			default:
				fprintf(file_pri,"Err\n");
			}



		}
		// Shift by 128: aud[1:128] <- aud[129:256]
		for(int i=0;i<ws;i++)
		{
			audbuf[i] = audbuf[ws+i];
		}
		unsigned tt2 = timer_us_get();

		// Repeat
		//fprintf(file_pri,"Buffer process time: %u\n",tt2-tt1);


	}
	fprintf(file_pri,"End\n");


	return 0;

}
