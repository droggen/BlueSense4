#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "stm32l496xx.h"
#include "megalol_dsp.h"
#include "global.h"
#include "main.h"
#include "helper.h"
#include "wait.h"
#include "atomicop.h"
#include "serial.h"
#include "serial_uart.h"
#include "circbuf.h"
#include "megalol_comm_softuart.h"
#include "stm32l4xx_hal.h"


/******************************************************************************
	file: megalol_comm_softuart.c
*******************************************************************************

	Provides a software-implemented UART (RX-only).

	Data can be decoded and read using direct access to the decode buffer, or
	via an stdio FILE interface.

	* Usage via decode buffer *

	Call comm_softuart_decode_init to initialise the decoding.
	Call comm_softuart_decode_frame whenever new binary frames come in.
	The decoded data is stored in the buffer _comm_softuart_rx_buffer. This
	is wrapped around by a CIRCBUF in _comm_softuart_serialparam.rxbuf.
	The CIRCBUF methods can then be used to retrieve the data.

	For example:
	Reading: buffer_get(&_comm_softuart_serialparam.rxbuf)
	Buffer not-empty: buffer_isempty(&_comm_softuart_serialparam.rxbuf)
	Buffer level: buffer_level(&_comm_softuart_serialparam.rxbuf)

 	* Usage via stdio FILE

	Call comm_softuart_open to intialise the decoding and open the file.
	Call comm_softuart_decode_frame whenever new binary frames come in.
	Read-out data with fgets, fgetc, fscanf, etc.

 	* Implementation details *

	Processing is done frame-by-frame. An internal buffer larger than a frame is
	used to store the last (byte - 1 sample) of a frame, which can't be decoded
	with the current frame, for decoding with the next frame.

 	* Main functions*

		*Core functions*

		* comm_softuart_decode_init						Initialise the decoding, specifying binary sample rate and bit rate. Data must be read directly from the CIRCBUF.
		* comm_softuart_decode_frame					Decode a frame of binary signal. Frame can be of arbitrary size and is internally chunked according to internal buffer sizes.
		* comm_softuart_open							Initialise the decoding, specifying binary sample rate and bit rate, and returns a file. Data can be read using stdio FILE functions.
		* comm_softuart_printstat						Statistics on total received data and data overrun
	* Test/benchmark*
		* comm_softuart_test_decode						Decode test using direct access to buffer
		* comm_softuart_test_decode_file				Decode test using FILE access to buffer
		* comm_softuart_bench_decode					Decode benchmark
*/

// Test signal for decoding
#define UARTSIGNALLEN 1536
unsigned char testbinarysignal_0[UARTSIGNALLEN]={0,0,0,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1};
unsigned char _comm_softuart_binaryin[2*COMM_SOFTUART_BINARYINBUFFERSIZE];		// Accepts maximum COMM_SOFTUART_BUFFERSIZE at a time, but holds twice the space to account for the last byte (could be shrinked)
//unsigned char _comm_softuart_byteout[COMM_SOFTUART_BUFFERSIZE];			// Uses the same size as the input stream.

// Global variables
unsigned _comm_softuart_stop=2;				// number of stop bits
unsigned _comm_softuart_bpw; 				// bit per byte
unsigned char prevbit;						// Previous bit state
unsigned char newprevbit;					// Holds the "new" previous bit state. Used to be able to do "continue" and get the bit state update.
unsigned spbit_8;							// spbit: sample per bit in .8 format, rounded to nearest
unsigned sphbit;							// sphbit: sample per half bit in .0 format, rounded to nearest
unsigned sphbit_8;							// sphbit: sample per half bit in .8 format, rounded to nearest
unsigned stsb1;								// Sample to stop bit 1 and 2, from falling edge, rounded to nearest, in .0 format
unsigned stsb2;								// Sample to stop bit 1 and 2, from falling edge, rounded to nearest, in .0 format
unsigned _comm_softuart_decodebufferlen;	// Number of samples in the decode buffer, 0 to 2*COMM_SOFTUART_BUFFERSIZE.

unsigned _comm_softuart_stat_rx_cnt,_comm_softuart_stat_rx_ovrn;		// RX count and RX overrun

FILE *_comm_softuart_file=0;				// Cache the file if multiple calls to fopencookie done.

// ----------------------------------------------------------------------------------------------------------------
// FILE INTERFACE  -  FILE INTERFACE  -  FILE INTERFACE  -  FILE INTERFACE  -  FILE INTERFACE  -  FILE INTERFACE  -
// ----------------------------------------------------------------------------------------------------------------
// RX buffers
unsigned char _comm_softuart_rx_buffer[COMM_SOFTUART_RX_BUFFERSIZE];
SERIALPARAM _comm_softuart_serialparam;		// Serial parameters

/******************************************************************************
	function: comm_softuart_decode_init
*******************************************************************************
	Initialise UART decoding.
	Calculate various bit timings required for decoding and initialise data structures.

	Call this once prior to repeated calls to comm_softuart_decode_frame

	Input:
		sr:				Sample rate of the binary stream [Hz]
		br:				Bit rate [Hz]

	Returns:
		-

******************************************************************************/
void comm_softuart_decode_init(int sr,int br)
{
	//
	_comm_softuart_bpw = 9+_comm_softuart_stop;		// bit per byte


	// Account for bit period not being multiples of sample period

	spbit_8 = ((sr<<8)+br/2)/br;			// spbit: sample per bit in .8 format, rounded to nearest
	sphbit = (sr+br)/br/2;					// sphbit: sample per half bit in .0 format, rounded to nearest
	sphbit_8 = ((sr<<8)+br) / (2*br);		// sphbit: sample per half bit in .8 format, rounded to nearest
	// Sample to stop bit 1 and 2, from falling edge: 9.5*sample_per_bit and 10.5*sample_per_bit. Rounded to nearest, in .0 format
	stsb1 = (19*sr+br)/(2*br);							// 9.5 bits in samples, rounded to nearest
	stsb2 = (21*sr+br)/(2*br);							// 10.5 bits in samples, rounded to nearest
	newprevbit=1;							// Initialised at one to enable falling edge detection immediately
	_comm_softuart_decodebufferlen=0;		// No data in buffer

	fprintf(file_pri,"Softuart parameters:\n");
	fprintf(file_pri,"\tBit per word: %d\n",_comm_softuart_bpw);
	fprintf(file_pri,"\tSample per bit in .8: %d (%08x)\n",spbit_8,spbit_8);
	fprintf(file_pri,"\tSample per half bit in .0: %d\n",sphbit);
	fprintf(file_pri,"\tSample per half bit in .8: %d\n",sphbit_8);
	fprintf(file_pri,"\tSamples to stop bit 1 from falling edge in real samples: %d\n",stsb1);
	fprintf(file_pri,"\tSamples to stop bit 2 from falling edge in real samples: %d\n",stsb2);

	// Initialise statistics
	_comm_softuart_stat_rx_cnt=0;
	_comm_softuart_stat_rx_ovrn=0;

	// Initialise buffer for debugging
	for(unsigned i=0;i<2*COMM_SOFTUART_BINARYINBUFFERSIZE;i++)
	{
		_comm_softuart_binaryin[i]=0xff;
	}

	// Initialise the RX circular buffers with the data buffer
	buffer_init(&_comm_softuart_serialparam.rxbuf,_comm_softuart_rx_buffer,COMM_SOFTUART_RX_BUFFERSIZE);
}

/******************************************************************************
	comm_softuart_open
*******************************************************************************
	High-level soft UART. Opens the softuart for reading with stdio FILE functions.

	Internally calls comm_softuart_decode_init.


	Return value:
		0:				success
		nonzero:	error
******************************************************************************/
FILE *comm_softuart_open(int sr,int br)
{
	// Low-level initialisation of timing variables and buffers
	comm_softuart_decode_init(sr,br);

	// Initialise the high-level data structure
	_comm_softuart_serialparam.device=0;
	_comm_softuart_serialparam.blocking=0;
	_comm_softuart_serialparam.blockingwrite=0;
	_comm_softuart_serialparam.bufferwhendisconnected=0;
	_comm_softuart_serialparam.putbuf=0;							// Read-only, no putbuf
	_comm_softuart_serialparam.fischar=&serial_uart_fischar;		// Use common serial_uart_fischar from serial_uart
	_comm_softuart_serialparam.dma1_or_int0 = 0;							// N/A
	_comm_softuart_serialparam.dma1_or_int0_tx = 0;						// N/A

	if(_comm_softuart_file==0)
	{
		// First time called the file is created.
		// Open file
		cookie_io_functions_t iof;
		iof.read = &_comm_softuart_cookieread_int;
		iof.write = 0;			// No write
		iof.close = 0;
		iof.seek = 0;
		_comm_softuart_file = fopencookie((void*)&_comm_softuart_serialparam,"r",iof);

		// Buffering
		setvbuf (_comm_softuart_file, 0, _IONBF, 0 );	// No buffering
		//setvbuf (_comm_softuart_file, 0, _IOLBF, 64);	// Line buffer buffering



		// Or: big hack - _cookie seems unused in this libc - this is not portable.
		_comm_softuart_file->_cookie = &_comm_softuart_serialparam;
	}
	else
	{
		// comm_softuart_open already called previously

		// In order to avoid memory allocation/deallocation, keep the previously allocated FILE.
		// The internal CIRBUF is cleared. However, stdio FILE internally buffers data, and this
		// input data buffer must be flushed.
		// fflush on the FILE *may* flush the input buffer, depending on the C library, but here doesn't.
		// Instead, loop-read until no more data.

		// fflush(_comm_softuart_file);			//	Flush the file hoping to clear the FILE input buffer set by setvbuf. Does not work
		unsigned n=0;
		while(fgetc(_comm_softuart_file)!=-1) {
			n++;
		}

		fprintf(file_pri,"Warning: softuart already open. Reusing FILE. Flushed %u bytes\n",n);
		//fclose(comm_softuart_open);
		//comm_softuart_open=0;



	}

	return _comm_softuart_file;
}

/******************************************************************************
	function: comm_softuart_decode_frame
*******************************************************************************
	Decode a frame of data.

	The frame can be of arbitrary size. It is internally chunked according
	to the size of internal buffers.

	Call comm_softuart_decode_frame

	Input:
		frame:			Binary signal
		len:			Length of binary signal

	Returns:
		Number of decoded bytes in frame

******************************************************************************/
unsigned comm_softuart_decode_frame(unsigned char *frame,unsigned len)
{
	//
	unsigned ndec=0;
	//fprintf(file_pri,"==== comm_softuart_decode_frame: input length: %d. Existing buffer length: %d ====\n",len,_comm_softuart_decodebufferlen);
	//print_array_char(file_pri,(char*)_comm_softuart_binaryin,2*COMM_SOFTUART_BUFFERSIZE);
	//fprintf(file_pri,"\n\n====\n");
	while(len!=0)
	{
		//fprintf(file_pri,"==== comm_softuart_decode_frame: iterating. Length/remaining length now: %d. Existing buffer length: %d\n",len,_comm_softuart_decodebufferlen);
		// Decode by frames of COMM_SOFTUART_BUFFERSIZE. Allows to adapt the input frame which can be of arbitrary size, to the internal buffer size
		// n=min(len,COMM_SOFTUART_BUFFERSIZE)
		unsigned n = len;
		if(n>COMM_SOFTUART_BINARYINBUFFERSIZE)
			n=COMM_SOFTUART_BINARYINBUFFERSIZE;
		// Copy n samples to _comm_softuart_binaryin
		for(unsigned i=0;i<n;i++)
		{
			_comm_softuart_binaryin[_comm_softuart_decodebufferlen+i] = *frame++;
		}
		// Adjust input buffer len
		_comm_softuart_decodebufferlen+=n;
		// Adjust remaining data to process
		len-=n;
		// Call the internal decode function
		ndec+=_comm_softuart_decode_buffer();
		//fprintf(file_pri,"comm_softuart_decode_frame: iterating. decoded: %d bytes. New remaining length: %d\n",ndec,len);

		// Print result
		//print_array_char(file_pri,(char*)_comm_softuart_binaryin,2*COMM_SOFTUART_BUFFERSIZE);

		//fprintf(file_pri,"\n");
	}
	return ndec;

}

// Add a received byte to some buffer
void _comm_softuart_decode_addrxbyte(unsigned char byte)
{
	//fprintf(file_pri,"Byte: %d\n",byte);

	_comm_softuart_stat_rx_cnt++;
	// Check if buffer has space
	if(!buffer_isfull(&_comm_softuart_serialparam.rxbuf))
	{
		buffer_put(&_comm_softuart_serialparam.rxbuf,byte);
	}
	else
	{
		_comm_softuart_stat_rx_ovrn++;

	}


}

unsigned _comm_softuart_decode_buffer()
{
	unsigned ndec=0;
	//fprintf(file_pri,"Before frame decode. Buffer len: %d. stsb2: %d\n",_comm_softuart_decodebufferlen,stsb2);
	// Assume the frame is 512
	// _comm_softuart_decodebufferlen = 512;

	// Check the buffer length and return immediately if there are less samples than make a complete byte
	if(_comm_softuart_decodebufferlen<stsb2)
		return 0;

	// Loop starting from zero, excluding the last stsb2 samples (i.e. do not decode if the stop bit is outside of the frame)
	unsigned i;
	for(i=0;i<_comm_softuart_decodebufferlen-stsb2;i++)
	{
		//fprintf(file_pri,"%d.\n",i);
		prevbit=newprevbit;		// To allow to use "continue" to go to the next sample easily.
		newprevbit=_comm_softuart_binaryin[i];	// Next previous bit value (i.e. current one) at next iteration.

		// Look for high-to-low transition
		if(_comm_softuart_binaryin[i]==0 && prevbit==1)
		{
			// Potential start bid
			//fprintf(file_pri,"Potential start bit: %d... ",i);
			// 1. Check if "middle" of start bit is still 0
			if(_comm_softuart_binaryin[i + sphbit] != 0)
			{
				//fprintf(file_pri,"Invalid\n");

				continue;		// Automatically continues. prevbit will become the current bit.
			}
			//fprintf(file_pri,"Ok\n");

			// 2. Check if stop bits are 1
			unsigned s1 = _comm_softuart_binaryin[i + stsb1];
			unsigned s2 = _comm_softuart_binaryin[i + stsb2];
			//fprintf(file_pri,"Stop bits: %d %d... ",s1,s2);
			if(s1!=1 || s2!=1)
			{
				//fprintf(file_pri,"Invalid\n");

				continue;		// Automatically continues, assigns prevbit to 0 (as we found a startbit newprevbit=0), and wait for next falling edge
			}
			//fprintf(file_pri,"Ok\n");
			// 3. Decode.
			// Valid word - decode
			unsigned byte=0;

			//unsigned o = sphbit_8 + spbit_8 + 128;					// Precalculate the middle of the 1st bit, adding 128 for rounding.
			for(unsigned j=0;j<8;j++)
			{
				unsigned o = sphbit_8 + spbit_8 + j*spbit_8;		// Position of the center of the bit j in .8. Could be precalculated but no speed difference.

				//fprintf(file_pri,"Bit %d at offset %d .8, %d. absolute: %d\n",j,o,(o+128)>>8,i + ((o+128)>>8));
				unsigned b = _comm_softuart_binaryin[i+ ((o+128)>>8)];				// Get bit j
				//unsigned b = _comm_softuart_binaryin[i+ (o>>8)];				// Get bit j
				//o+=spbit_8;

				//fprintf(file_pri,"%d\n",b);
				byte>>=1;
				byte|=b<<7;
			}
			// 4. Add byte to receive buffer
			//fprintf(file_pri,"Byte at %d. ",i);
			_comm_softuart_decode_addrxbyte(byte);
			ndec++;

			// 5. Skip the samples making up the byte.
			//fprintf(file_pri,"Setting pointer to middle of stop bit: %d\n",i + stsb2);
			i = i + stsb2;
			// Update the new current bit to 1 (middle of stop bit is 1).
			newprevbit=1;

		}
	}
	//fprintf(file_pri,"Returning. First unprocessed bit: %d. _comm_softuart_decodebufferlen: %d. stsb2 %d\n",i,_comm_softuart_decodebufferlen,stsb2);
	// 6. Copy the last undecoded bits to the front of the buffer
	// Move data from i (last unprocesed bit) to end, to the to the start of buffer
	// Could be optimised with circular buffer at the expense of masking when fetching data.
	//int nc=0;
	for(unsigned j=i;j<_comm_softuart_decodebufferlen;j++)
	{
		_comm_softuart_binaryin[j-i] = _comm_softuart_binaryin[j];
		//nc++;
	}
	// Adapt the length of the binaryin: depends on how much data was decoded in current round.
	_comm_softuart_decodebufferlen=_comm_softuart_decodebufferlen-i;
	//fprintf(file_pri,"nc: %d\n",nc);
	//fprintf(file_pri,"After frame decode. Buffer len: %d. stsb2: %d. Decoded: %d\n",_comm_softuart_decodebufferlen,stsb2,ndec);
	// Return number of decoded bytes
	return ndec;
}










/******************************************************************************
	comm_softuart_cookieread_int
*******************************************************************************
	Used with fcookieopen to read a buffer from a stream.


	Return value:
		number of bytes read (0 if none)
******************************************************************************/
ssize_t _comm_softuart_cookieread_int(void *__cookie, char *__buf, size_t __n)
{
	__ssize_t nr=0;

	//fprintf(file_pri,"_comm_softuart_cookieread_int: n=%d\n",__n);

	// Convert cookie to SERIALPARAM
	SERIALPARAM *sp = (SERIALPARAM*)__cookie;

	// Read up to __n bytes
	for(size_t i=0;i<__n;i++)
	{
		if(!buffer_isempty(&(sp->rxbuf)))
		{
			__buf[i] = buffer_get(&(sp->rxbuf));
			nr++;
		}
		else
			break;
	}

	// If no data, should return error (-1) instead of eof (0).
	// Returning eof has side effects on subsequent calls to fgetc: it returns eof forever, until some write is done.
	if(nr==0)
		return -1;

	return nr;
}
/******************************************************************************
	comm_softuart_printstat
*******************************************************************************
	Print RX statistics.

******************************************************************************/
void comm_softuart_printstat()
{
	fprintf(file_pri,"Softuart RX total: %u\n",_comm_softuart_stat_rx_cnt);
	fprintf(file_pri,"Softuart RX overrun: %u\n",_comm_softuart_stat_rx_ovrn);
}




void comm_softuart_test_decode()
{
	int sr = 1198080/2;		// After downsampling
	int br=115200;

	comm_softuart_decode_init(sr,br);

	fprintf(file_pri,"Testing UART decoding using binary signal of length %d and buffer readout\n",UARTSIGNALLEN);

	unsigned long t1 = timer_us_get();
	unsigned nd = comm_softuart_decode_frame(testbinarysignal_0,UARTSIGNALLEN);
	//fsk_decodeframe(testbinarysignal_0+2*CICBUFSIZ/2,sr,br);
	unsigned long t2 = timer_us_get();
	fprintf(file_pri,"Decoded %u bytes in %lu us\n",nd,t2-t1);

	// Read-out bytes
	fprintf(file_pri,"Reading out decoded data\n");
	comm_softuart_printstat();
	fprintf(file_pri,"Buffer level: %u\n",buffer_level(&_comm_softuart_serialparam.rxbuf));
	fprintf(file_pri,"Data:\n");
	while(!buffer_isempty(&_comm_softuart_serialparam.rxbuf))
	{
		unsigned char c = buffer_get(&_comm_softuart_serialparam.rxbuf);
		fprintf(file_pri,"%02X ",c);
	}
	fprintf(file_pri,"\n");


}
void comm_softuart_test_decode_file()
{
	int sr = 1198080/2;		// After downsampling
	int br=115200;



	fprintf(file_pri,"Testing UART decoding using binary signal of length %d and FILE readout\n",UARTSIGNALLEN);

	FILE *f = comm_softuart_open(sr,br);

	unsigned long t1 = timer_us_get();
	unsigned nd = comm_softuart_decode_frame(testbinarysignal_0,UARTSIGNALLEN);
	//fsk_decodeframe(testbinarysignal_0+2*CICBUFSIZ/2,sr,br);
	unsigned long t2 = timer_us_get();
	fprintf(file_pri,"Decoded %u bytes in %lu us\n",nd,t2-t1);

	// Read-out bytes
	fprintf(file_pri,"Reading out decoded data\n");
	comm_softuart_printstat();
	fprintf(file_pri,"Data:\n");
	while(1)
	//for(int i=0;i<10;i++)		// only partial read
	{
		int c = fgetc(f);
		if(c==-1)
			break;
		fprintf(file_pri,"%02X ",c);
	}
	fprintf(file_pri,"\n");
}
void comm_softuart_bench_decode()
{
	int sr = 1198080/2;		// After downsampling
	int br=115200;

	comm_softuart_decode_init(sr,br);


	// Benchmarking

	fprintf(file_pri,"Decoding a total of %d\n",UARTSIGNALLEN);

	for(unsigned i=0;i<UARTSIGNALLEN;i++)		// Generate test signal
	{
		//testbinarysignal_0[i] = i;
	}
	//print_array_char(file_pri,(char*)testbinarysignal_0,UARTSIGNALLEN);


	unsigned long int t_last=timer_s_wait(),t_cur;
	unsigned long int tint1=timer_ms_get_intclk();
	unsigned long int nsample=0;
	unsigned mintime=1;
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		//comm_softuart_decode_frame(testbinarysignal_0,UARTSIGNALLEN);
		comm_softuart_decode_frame(testbinarysignal_0,512);			// 512 to keep in sync with demodulate bench
		nsample++;
		//break;
		//HAL_Delay(100);
	}
	unsigned long int tint2=timer_ms_get_intclk();


	HAL_Delay(100);
	fprintf(file_pri,"Time: %lds (%lu intclk ms) Frames: %lu. Frames of %d samples. Frames/sec: %lu\n",t_cur-t_last,tint2-tint1,nsample,UARTSIGNALLEN,nsample/(t_cur-t_last));

	fprintf(file_pri,"Frames of 512/sec: %lu\n",UARTSIGNALLEN/512*nsample/(t_cur-t_last));



}

unsigned comm_softuart_level()
{
	return buffer_level(&_comm_softuart_serialparam.rxbuf);
}
unsigned comm_softuart_get(char * restrict buffer,unsigned n)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check buffer level
		unsigned avail = buffer_level(&_comm_softuart_serialparam.rxbuf);
		// Maximum readout equal to buffer level
		if(n>avail)
			n=avail;

		for(unsigned i=0;i<n;i++)
		{
			// No need to test for available data as level sufficient
			buffer[i] = _buffer_get(&_comm_softuart_serialparam.rxbuf);		// Call _buffer_get which does not clear/set interrupts
		}
		return n;
	}
	return 0;
}
