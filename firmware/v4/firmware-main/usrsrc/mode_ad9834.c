/*
 * mode_ad9834.c
 *
 *  Created on: 28 mai 2021
 *      Author: droggen
 */



#include <stdio.h>
#include <string.h>
#include <math.h>
#include "main.h"

#include "global.h"
#include "mode_main.h"
#include "command.h"
#include "commandset.h"
#include "mode_ad9834.h"
#include "mode.h"
#include "wait.h"
#include "helper.h"
#include "serial.h"
#include "wait.h"
#include "serial_usart3.h"
#include "ufat.h"
#include "fatfs.h"

/*
	AD9834 interface on BlueSense4 v4:
		Reset:		X_5				PC6
		FSEL:		X_3_ADC3		PC4
		PSEL:		X_2_ADC2		PC3

		FSYNC:		X_SAIA2_FS_NSS	PB12
		SDATA:		X_SAIA2_SD_MOSI	PB15
		SCLK:		X_SAIA2_SCK		PB13

	SPI interface:
		Idle: FSYNC=1; SCK = 1. SCK could be idle in any state; but must be 1 before clearing FSYNC
		Data outputted: on SCK rising/high
		Data read: on falling SCK

	Frequency:
		f = 75M/(2^28) * m
		f*2^28/75M = m


*/


const COMMANDPARSER CommandParsersAD9834[] =
{
	{'H', CommandParserHelp,help_h},
	{'!', CommandParserQuit,help_quit},
	{0, 0, "--- Low-level ---"},
	{'I', CommandParserAD9834InitPort,"Init I/O ports"},
	{'i', CommandParserAD9834InitAD9834,"i,<frqreg0>[,<phreg0>[,<frqreg1>,<phreg1>]]: Initialise AD9834 registers with specified values.\n\t\tfout = fMCLK/2^28 x frqn"},
	{'C', CommandParserAD9834CalcFrq,"C,<desiredfrq>: calculates the frqreg value to achieve the desired frquency"},
	{'R', CommandParserAD9834Reset,"R,<0|1> Sets reset pin"},
	{'F', CommandParserAD9834Frq,"F,<0|1> Sets frequency pin"},
	{'P', CommandParserAD9834Phase,"P,<0|1> Sets phase pin"},
	{'W', CommandParserAD9834Write,"W,<word> Writes the 16-bit <word>"},
	//{'1', CommandParserAD9834Test1,"Test 1"},
	{'A', CommandParserAD9834ModulateFSK,"M,f0,f1,tb: FSK modulation alternating bits at f0 and f1 Hz every tb microseconds"},
	{'U', CommandParserAD9834InitUart,"Initialise the UART"},
	{'u', CommandParserAD9834UartWrite,"u,<byte> UART write byte"},
	{'Q', CommandParserAD9834Sqw,"S,0|1 enable or disable square wave output"},
	{0, 0, "--- Data transfer: initialise with M then use data transfer commands ---"},
	{'M', CommandParserAD9834ModulateFSK2,"m,f0,f1,bitrate,stop: Initialise FSK modulation at f0 and f1 Hz with specific UART baud rate and stop bits (1 or 2)"},
	{'T', CommandParserAD9834ModulateFSKTerminal,"Terminal mode"},
	// Send sequence, max speed
	{'S', CommandParserAD9834ModulateFSKSequence,"S,<seq>,<delay>,<parity> Send a sequence every <delay> ms. seq=0: bytes 0-255; seq=1: fox. parity: 0 disabled; 1=bit 0 for even parity"},
	{'s', CommandParserAD9834ModulateFSKSine,"s,<sr>,<frq>,<parity> Send an 8-bit sine wave of frequency <frq> Hz at a sample rate of <sr> Hz. parity: 0 disabled; 1=bit 0 for even parity"},
	{'t', CommandParserAD9834ModulateFSKTune,"<sr>,<tuneno>,<parity> Transmit tune tun<tuneno>.aud at <sr> Hz. parity: 0 disabled; 1=bit 0 for even parity"},
};
const unsigned char CommandParsersAD9834Num=sizeof(CommandParsersAD9834)/sizeof(COMMANDPARSER);

FILE * _file_fsk=0;

unsigned char CommandParserModeAD9834(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;
	// Change to modulation mode
	CommandChangeMode(APP_MODE_AD9834);
	return 0;
}

void mode_ad9834(void)
{
	mode_run("AD9834",CommandParsersAD9834,CommandParsersAD9834Num);
}


unsigned char CommandParserAD9834InitPort(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;

	AD9834InitPort();






	return 0;
}
unsigned char CommandParserAD9834Reset(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>1)
		return 2;


	fprintf(file_pri,"Reset: %d\n",x);
	if(x)
		HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);

	return 0;
}
unsigned char CommandParserAD9834Frq(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>1)
		return 2;



	fprintf(file_pri,"Frq: %d\n",x);

	AD9834SetFrqPin(x);


	return 0;
}
unsigned char CommandParserAD9834Phase(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>1)
		return 2;




	fprintf(file_pri,"Phase: %d\n",x);

	if(x)
		HAL_GPIO_WritePin(X_2_ADC2_GPIO_Port, X_2_ADC2_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(X_2_ADC2_GPIO_Port, X_2_ADC2_Pin, GPIO_PIN_RESET);

	return 0;
}
unsigned char CommandParserAD9834Write(char *buffer,unsigned char size)
{
	unsigned x;

	if(ParseCommaGetUnsigned(buffer,1,&x))
		return 2;
	if(x>65535)
		return 2;


	fprintf(file_pri,"Write: %04X (%05d)\n",x,x);

	AD9834Write(x);

	return 0;
}
unsigned char CommandParserAD9834InitAD9834(char *buffer,unsigned char size)
{
	unsigned frq0,ph0,frq1,ph1;

	unsigned np = ParseCommaGetNumParam(buffer);
	// Valid parameters: 1, 2 or 4.
	if(np!=1 && np!=2 && np!=4)
		return 2;

	// Get 1st param
	if(ParseCommaGetUnsigned(buffer,1,&frq0))
		return 2;
	// Test 1st param
	if(frq0>0x0FFFFFFF)
		return 2;
	if(np>=2)
	{
		// Get 2nd param
		if(ParseCommaGetUnsigned(buffer,2,&frq0,&ph0))
			return 2;
		// Test 2nd param
		if(ph0>0x00000FFF)
			return 2;
		if(np==4)
		{
			// Get 3rd, 4th param
			if(ParseCommaGetUnsigned(buffer,4,&frq0,&ph0,&frq1,&ph1))
				return 2;
			// Test 3rd, 4th param
			if(frq1>0x0FFFFFFF || ph1>0x00000FFF)
				return 2;

			fprintf(file_pri,"frq0, ph0: %u, %u. frq1, ph1: %u, %u\n",frq0, ph0, frq1, ph1);
			AD9834InitFP0FP1(frq0,ph0,frq1,ph1);
		}
		else
		{
			fprintf(file_pri,"frq0, ph0: %u, %u\n",frq0, ph0);
			AD9834InitFP0(frq0,ph0);
		}
	}
	else
	{
		fprintf(file_pri,"frq0: %u\n",frq0);
		AD9834InitF0(frq0);
	}


	return 0;
}



void AD9834InitPort(void)
{
	// Initialise PC3 (PSEL), PC4 (FSEL), PC6 (RST) as output push-pull
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = X_2_ADC2_Pin|X_3_ADC3_Pin|X_5_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);		// All are on GPIOC

	// Write 0 to all 3 pins
	HAL_GPIO_WritePin(X_2_ADC2_GPIO_Port, X_2_ADC2_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(X_3_ADC3_GPIO_Port, X_3_ADC3_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);


	// Initialise the SPI interface - we use a software interface as speed isn't critical
	GPIO_InitStruct.Pin = X_SAIA2_FS_NSS_Pin|X_SAIA2_SD_MOSI_Pin|X_SAIA2_SCK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);		// All are on GPIOC

	// Write idle to all 3 pins
	HAL_GPIO_WritePin(X_SAIA2_FS_NSS_GPIO_Port, X_SAIA2_FS_NSS_Pin, GPIO_PIN_SET);				// FSYNC: Idle is 1
	HAL_GPIO_WritePin(X_SAIA2_SD_MOSI_GPIO_Port, X_SAIA2_SD_MOSI_Pin, GPIO_PIN_RESET);			// MOSI: Idle is x
	HAL_GPIO_WritePin(X_SAIA2_SCK_GPIO_Port, X_SAIA2_SCK_Pin, GPIO_PIN_SET);					// SCK: Idle is 1
}
void AD9834SetFrqPin(unsigned x)
{
	if(x)
		HAL_GPIO_WritePin(X_3_ADC3_GPIO_Port, X_3_ADC3_Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(X_3_ADC3_GPIO_Port, X_3_ADC3_Pin, GPIO_PIN_RESET);
}
void AD9834InitControlReg(int ensqw)
{
	// Enable sine wave and sign bit output
	// Format: (msb) 0 0 B28 HLB FSEL PSEL PIN/SW RESET SLEEP1 SLEEP12 OPBITEN SIGN/PIB DIV2 0 Mode 0 (lsb)
	// Mode = 0 (sine)
	if(ensqw)
		AD9834Write(0b0010001100101000);		// Sine, opbiten enabled
	else
		AD9834Write(0b0010001100001000);		// Sine, opbiten disabled
	//AD9834Write(0b0010001100001010);		// Triangle (opbiten must be 0 for triangle)

}
void AD9834InitFP0FP1(unsigned int frq0, unsigned short ph0, unsigned int frq1, unsigned short ph1)
{

	// Initialise the control word
	AD9834InitControlReg(0);



	// Reset (may not be necessary)
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);

	// Set frq0 in 2 14-bit writes
	AD9834Write(0x4000| (frq0&0x3FFF));			// FRQ0 LSB
	AD9834Write(0x4000| ((frq0>>14)&0x3FFF));	// FRQ0 MSB

	// Set frq1 in 2 14-bit writes
	AD9834Write(0x8000| (frq1&0x3FFF));			// FRQ1 LSB
	AD9834Write(0x8000| ((frq1>>14)&0x3FFF));	// FRQ1 MSB

	// Phase
	AD9834Write(0xC000  | (ph0&0xfff));		// PH0
	AD9834Write(0xE000  | (ph1&0xfff));		// PH1
}
void AD9834InitFP0(unsigned int frq0, unsigned short ph0)
{

	// Initialise the control word
	AD9834InitControlReg(0);

	// Reset (may not be necessary)
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);

	// Set frq0 in 2 14-bit writes
	AD9834Write(0x4000| (frq0&0x3FFF));			// FRQ0 LSB
	AD9834Write(0x4000| ((frq0>>14)&0x3FFF));	// FRQ0 MSB


	// Phase
	AD9834Write(0xC000  | (ph0&0xfff));		// PH0
}
void AD9834InitF0(unsigned int frq0)
{
	// Initialise the control word
	AD9834InitControlReg(0);

	// Reset (may not be necessary)
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_SET);
	HAL_Delay(1);
	HAL_GPIO_WritePin(X_5_GPIO_Port, X_5_Pin, GPIO_PIN_RESET);

	// Set frq0 in 2 14-bit writes
	AD9834Write(0x4000| (frq0&0x3FFF));			// FRQ0 LSB
	AD9834Write(0x4000| ((frq0>>14)&0x3FFF));	// FRQ0 MSB
}

void AD9834Write(unsigned int word)
{
	// Writes the 16-bit word
	// Assumes IDLE state on start and end, i.e. FSYNC=1, SCK=1

	unsigned n=10000;
	unsigned t1 = timer_ms_get();
	for(int i=0;i<n;i++)
		timer_us_wait(10);
	unsigned t2 = timer_ms_get();
	fprintf(file_pri,"%d: %d\n",n,t2-t1);

	fprintf(file_pri,"w: %02X\n",word);

	// Clear FSYNC
	HAL_GPIO_WritePin(X_SAIA2_FS_NSS_GPIO_Port, X_SAIA2_FS_NSS_Pin, GPIO_PIN_RESET);
	for(int i=0;i<16;i++)
	{
		// Write data & wait
		unsigned d = (word&(1<<(15-i))) ? 1:0;		// Get bit i, first bit is bit 15
		fprintf(file_pri,"%d ",d);
		HAL_GPIO_WritePin(X_SAIA2_SD_MOSI_GPIO_Port, X_SAIA2_SD_MOSI_Pin, d);
		timer_us_wait(50);

		// Lower clock & wait
		HAL_GPIO_WritePin(X_SAIA2_SCK_GPIO_Port, X_SAIA2_SCK_Pin, GPIO_PIN_RESET);
		timer_us_wait(50);


		// Raise clock
		HAL_GPIO_WritePin(X_SAIA2_SCK_GPIO_Port, X_SAIA2_SCK_Pin, GPIO_PIN_SET);

	}
	// Wait and set FSYNC
	timer_us_wait(50);
	HAL_GPIO_WritePin(X_SAIA2_FS_NSS_GPIO_Port, X_SAIA2_FS_NSS_Pin, GPIO_PIN_SET);

	fprintf(file_pri,"\n");
}

unsigned long AD9834Frq2Reg(unsigned long frq)
{
	unsigned long long frqreg;

	frqreg = frq;
	frqreg *= 0x10000000ull;
	frqreg /= 75000000ull;

	return (unsigned long)frqreg;
}
unsigned long AD9834Reg2Frq(unsigned long reg)
{
	unsigned long long frq;

	frq = reg;
	frq *= 75000000ull;
	frq /= 0x10000000ull;

	return (unsigned long)frq;
}

#if 0
unsigned char CommandParserAD9834Test1(char *buffer,unsigned char size)
{
	(void) buffer; (void) size;


	fprintf(file_pri,"Test 1\n");

	AD9834InitPort();

	AD9834InitFP0FP1(4,0,8,0x800);


	return 0;
}
#endif



unsigned char CommandParserAD9834ModulateFSK(char *buffer,unsigned char size)
{
	unsigned frq0,frq1,tb;
	WAITPERIOD wp;
	unsigned long long mull0,mull1;
	unsigned long mul0,mul1;

	if(ParseCommaGetUnsigned(buffer,3,&frq0,&frq1,&tb))
		return 2;

	// Convert the frequency into divider


	fprintf(file_pri,"SysTick->LOAD: %u\n",(unsigned int)SysTick->LOAD);

	fprintf(file_pri,"sizeof: %d\n",sizeof(mull0));
	//mul0 = frq0*2^28/75M = m = frq0*2^22/1171875
	mull0 = ((unsigned long long)frq0)*0x400000ull/1171875ull;
	mull1 = ((unsigned long long)frq1)*0x400000ull/1171875ull;
	mul0 = (unsigned long)mull0;
	mul1 = (unsigned long)mull1;

	fprintf(file_pri,"%lu %lu\n",mul0,mul1);

	// Initialise
	AD9834InitFP0FP1(mul0,0,mul1,0);

	// Alternate every tb microsecond

	wp=0;
	while(!fischar(file_pri))
	{
		AD9834SetFrqPin(0);
		//timer_waitperiod_us(tb,&wp);
		timer_us_wait(tb);
		AD9834SetFrqPin(1);
		//timer_waitperiod_us(tb,&wp);
		timer_us_wait(tb);
	}




	return 0;





}

unsigned char CommandParserAD9834InitUart(char *buffer,unsigned char size)
{
	serial_usart3_deinit();
	//serial_usart3_init(245,LL_USART_STOPBITS_1);
	serial_usart3_init(245,LL_USART_STOPBITS_1);
	return 0;
}



unsigned char CommandParserAD9834UartWrite(char *buffer,unsigned char size)
{
	unsigned byte;

	if(ParseCommaGetInt(buffer,1,&byte))
		return 2;

	fprintf(file_pri,"Sending %02h continuously until keypress\n",byte);

	while(!fischar(file_pri))
		serial_usart3_send(byte);


	return 0;
}


unsigned char CommandParserAD9834CalcFrq(char *buffer,unsigned char size)
{
	unsigned frq;

	if(ParseCommaGetInt(buffer,1,&frq))
		return 2;

	unsigned reg = AD9834Frq2Reg(frq);
	unsigned frq1 = AD9834Reg2Frq(reg);
	unsigned frq2 = AD9834Reg2Frq(reg+1);

	fprintf(file_pri,"Frequency register: %u -> %u Hz\n",reg,frq1);
	fprintf(file_pri,"Frequency register: %u -> %u Hz\n",reg+1,frq2);


	return 0;
}



unsigned char CommandParserAD9834ModulateFSK2(char *buffer,unsigned char size)
{
	unsigned f0,f1,bitrate,stop;
	if(ParseCommaGetInt(buffer,4,&f0,&f1,&bitrate,&stop))
	{
		return 2;
	}
	if(bitrate<245)
	{
		fprintf(file_pri,"Invalid bitrate\n");
		return 1;
	}
	if(stop != 1 && stop != 2)
	{
		fprintf(file_pri,"Invalid stop bits\n");
		return 1;
	}

	// Calculate the registers
	unsigned r0 = AD9834Frq2Reg(f0);
	unsigned r1 = AD9834Frq2Reg(f1);
	unsigned f0eff = AD9834Reg2Frq(r0);
	unsigned f1eff = AD9834Reg2Frq(r1);

	fprintf(file_pri,"Effective DDS frequency: %u, %u\n",f0eff,f1eff);

	AD9834InitPort();
	AD9834InitFP0FP1(r0,0,r1,0);
	_file_fsk = serial_usart3_init(bitrate,(stop==1)?LL_USART_STOPBITS_1:LL_USART_STOPBITS_2);		// Opens a file.


	fprintf(file_pri,"DDS and UART initialised (%p). Use data transfer commands now\n",_file_fsk);


	/*fprintf(file_pri,"Sending pattern %u until keypress\n",pattern);


	while(!fischar(file_pri))
	{
		unsigned t1=timer_us_get();
		for(unsigned i=0;i<256;i++)
			serial_usart3_send(i);
		unsigned t2=timer_us_get();

		fprintf(file_pri,"Elapsed time for 256 bytes: %u us\n",t2-t1);
		fprintf(file_pri,"Time per byte: %u us\n",(t2-t1)/256);

	}*/

	return 0;
}

void CommandParserAD9834ModulateFSKTerminal_Help(void)
{
	fprintf(file_usb,"Terminal mode. The following commands are available:\n");
	fprintf(file_usb,"\t???: Help\n");
	fprintf(file_usb,"\tXXX: Exit\n");
}
unsigned CheckFSKInit()
{
	if(_file_fsk==0)
	{
		fprintf(file_pri,"FSK modulation not initialised\n");
		return 1;
	}
	return 0;
}
unsigned char CommandParserAD9834ModulateFSKTerminal(char *buffer,unsigned char size)
{
	(void)buffer; (void)size;
	unsigned char cmdchar=0;
	unsigned char cmdn=0;			// Potential command (triple char)
	unsigned char shouldrun=1;
	int c;

	if(CheckFSKInit())
		return 1;


	CommandParserAD9834ModulateFSKTerminal_Help();


	while(shouldrun)
	{
		if((c=fgetc(file_pri))!=-1)
		{
			cmddecodestart:
			if( (c=='X' || c=='x' || c=='?') || cmdn)
			{
				if(cmdn==0)
				{
					// Store command
					cmdchar=c;
					cmdn++;
				}
				else
				{
					if(c==cmdchar)
					{
						// New character is repetition - increment
						cmdn++;
						if(cmdn==3)
						{
							// command recognised
							switch(cmdchar)
							{
								case 'X':
								case 'x':
									fprintf(file_pri,"<Exit>\n");
									shouldrun=0;
									break;
								case '?':
								default:
									CommandParserAD9834ModulateFSKTerminal_Help();
									break;
							}
							cmdn=0;
						}
					}
					else
					{
						// Not same as previous characters, output previous and current
						for(unsigned char i=0;i<cmdn;i++)
							fputc(cmdchar,_file_fsk);
						cmdn=0;
						goto cmddecodestart;
					}
				}
			}
			else
			{
				fputc(c,_file_fsk);
				//_delay_ms(10);
			}
		}
	}

	return 0;
}


unsigned char CommandParserAD9834Sqw(char *buffer,unsigned char size)
{
	int en;
	if(ParseCommaGetInt(buffer,1,&en))
	{
		return 2;
	}
	if(en)
		AD9834InitControlReg(1);
	else
		AD9834InitControlReg(0);
	return 0;
}

void AD9834ModulateFSKSendSequence(char *seq,unsigned len,unsigned delay)
{
	unsigned totw=0;
	fprintf(file_pri,"Sending sequence until keypress\n");

	unsigned tstart=timer_ms_get();
	while(1)
	{
		if(fgetc(file_pri)!=-1)
			break;

		// Write sequence
		//unsigned t1=timer_us_get();
		unsigned n = (unsigned)fwrite(seq,1,len,_file_fsk);
		//unsigned t2=timer_us_get();
		totw+=n;
		unsigned tcur = timer_ms_get();
		fprintf(file_pri,"Wrote: % 10u bytes in % 10u ms: % 10u bps\n",totw,tcur-tstart,totw*1000/(tcur-tstart));

		if(delay)
			HAL_Delay(delay);
	}
}

void wait_txdone()
{
	while(!buffer_isempty(&_serial_usart3_param.txbuf));
}


unsigned char CommandParserAD9834ModulateFSKSequence(char *buffer,unsigned char size)
{
	unsigned type,delay,len,parity;
	char seq[256];

	if(ParseCommaGetInt(buffer,3,&type,&delay,&parity))
		return 2;

	if(type>1)
		return 2;
	if(delay>1000)
		return 2;

	if(CheckFSKInit())
		return 1;

	if(type==0)
	{
		for(unsigned i=0;i<256;i++)
			seq[i]=i;
		len=256;
	}
	else
	{
		strcpy(seq,"The quick brown fox jumps over the lazy dog.\n");
		len=strlen(seq);
	}

	// Activate the parity
	serial_usart3_enablecksum(parity);
	AD9834ModulateFSKSendSequence(seq,len,delay);

	wait_txdone(); serial_usart3_enablecksum(0);

	return 0;
}

unsigned char CommandParserAD9834ModulateFSKSine(char *buffer,unsigned char size)
{
	unsigned frq,sr,parity;
	unsigned totw=0;
	unsigned dlyus;
	char seq[256];
	float phase=0;
	float phincr;
	WAITPERIOD wp=0;

	if(ParseCommaGetInt(buffer,3,&sr,&frq,&parity))
		return 2;
	if(CheckFSKInit())
		return 1;

	// Initialise the us to something which will make it crash soon
	//timer_init_us()
	//timer_init(1000,2000,4283000000);
	timer_init(4294967286,0,4293000000);



	phincr = 2*3.14159265*(float)frq/(float)sr;

	dlyus = (256*1000000+(sr/2))/sr;		// Calculate microsecond delay between packet writes

	serial_usart3_enablecksum(parity);

	fprintf(file_pri,"Sending sequence until keypress\n");

	/*unsigned long tl=timer_us_get();
	unsigned ctr=0;
	while(1)
	{
		unsigned long t=timer_us_get();

		fprintf(file_pri,"tus: %u tl: %u. t-tl: %u. monotonit: %u. us: %u\n",t,tl,t-tl,_timer_time_us_monotonic, _timer_time_us);
		tl=t;
		HAL_Delay(5);
		ctr++;
		if(ctr==1000)
		{
			fprintf(file_pri,"===================== BACKWARD\n");
			// Simulate going backward by 2 seconds, i.e. simulating a correction of the time
			_timer_time_us-=2000000;
		}
		if(ctr==2000)
		{
			fprintf(file_pri,"===================== FORWARD\n");
			// Simulate going forward by 2 seconds, i.e. simulating a correction of the time
			_timer_time_us+=2000000;
		}

	}*/

	unsigned tstart=timer_ms_get();
	unsigned nit=0;
	float piwrap=M_TWOPI*10000.0;
	while(1)
	{
		if(fgetc(file_pri)!=-1)
			break;

		// Generate the sequence
		for(unsigned i=0;i<256;i++)
		{
			seq[i] = (int)(cos(phase)*127.5+127.5);
			phase+=phincr;
		}
		// Limit phase growth to avoid float issues
		if(phase>piwrap)
			phase-=piwrap;
		/*for(unsigned i=0;i<256;i++)
		{	// Triangle wave
			seq[i] = (i&31)*8;
		}*/

		// Wait a multiple of the delay
		unsigned long t = timer_waitperiod_us(dlyus,&wp);
		//unsigned long t = timer_waitperiod_ms(dlyus/1000,&wp);

		// Write sequence
		unsigned t1=timer_us_get();
		unsigned n = (unsigned)fwrite(seq,1,256,_file_fsk);
		unsigned t2=timer_us_get();
		totw+=n;
		unsigned tcur = timer_ms_get();
		unsigned bl = buffer_level(&_serial_usart3_param.txbuf);
		fprintf(file_pri,"%05u. Wrote: % 10u bytes in % 10u ms: % 10u bps. Last frame write time: %u. Buffer level: %u. ph: %f. phincr: %f\n",nit,totw,tcur-tstart,totw*1000/(tcur-tstart),t2-t1,bl,phase,phincr);

		//if(delay)
			//HAL_Delay(delay);
		nit++;
	}

	wait_txdone(); serial_usart3_enablecksum(0);

	return 0;
}
unsigned char CommandParserAD9834ModulateFSKTune(char *buffer,unsigned char size)
{
	unsigned sr,tune,parity;
	unsigned totw=0;
	unsigned dlyus;
	char seq[256];
	WAITPERIOD wp=0;
	FRESULT res;
	UINT br;

	if(ParseCommaGetInt(buffer,3,&sr,&tune,&parity))
		return 2;
	if(CheckFSKInit())
		return 1;

	// Check the filesystem
	if(ufat_initcheck())
		return 1;

	// Open the file
	FIL fil;            // File object
	int fsize;
	unsigned pos=0;
	char fn[64];
	sprintf(fn,"tun%d.aud",tune);
	res = f_open(&fil, fn, FA_READ);
	if(res)
	{
		fprintf(file_pri,"Error opening %s\n",fn);
		_ufat_unmount();
		return 1;
	}
	else
		fprintf(file_pri,"Opened %s\n",fn);

	// File has been opened - get size
	fsize = f_size(&fil);
	fprintf(file_pri,"File size: %d\n",fsize);


	dlyus = (256*1000000+(sr/2))/sr;		// Calculate microsecond delay between packet writes

	serial_usart3_enablecksum(parity);

	fprintf(file_pri,"Sending sequence until keypress\n");

	unsigned tstart=timer_ms_get();
	while(1)
	{
		if(fgetc(file_pri)!=-1)
			break;

		// Generate the sequence
		/*for(unsigned i=0;i<256;i++)
		{
			seq[i] = i;
		}*/
		// Check position in file
		if(pos+256>=fsize)
		{
			// Seek to start

			res = f_lseek (&fil,0);
			fprintf(file_pri,"Rewinding: %d\n",res);
			pos=0;
		}


		// Read data from file!
		UINT br;
		 res = f_read(&fil, seq, 256, &br);
		//unsigned t2 = timer_us_get();
		//fprintf(file_dbg,"Read time: %u us %u B/S\n",t2-t1,send_size*1000000/(t2-t1));
		if(res || br!=256)
		{
			fprintf(file_dbg,"Read error: abort. br=%d\n",br);
			break;
		}
		pos+=256;

		// Wait a multiple of the delay
		unsigned long t = timer_waitperiod_us(dlyus,&wp);

		// Write sequence
		unsigned t1=timer_us_get();
		unsigned n = (unsigned)fwrite(seq,1,256,_file_fsk);
		unsigned t2=timer_us_get();
		totw+=n;
		unsigned tcur = timer_ms_get();
		fprintf(file_pri,"Wrote: % 10u bytes in % 10u ms: % 10u bps. Last frame: %u\n",totw,tcur-tstart,totw*1000/(tcur-tstart),t2-t1);


	}
	f_close(&fil);
	_ufat_unmount();

	wait_txdone(); serial_usart3_enablecksum(0);

	return 0;
}

