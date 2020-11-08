#ifndef MEGALOL_STMDAC_H_
#define MEGALOL_STMDAC_H_


#define STMDAC_LUTSIZ 1024
#define STMDAC_LUTINDEXMASK 0x3ffffff				// 0x3ff=1023 (max val) and preserve fractional part

#define DACMAXWAVEFORMS	5							// Number of simultaneous waveforms

//#define DACDMAHALFBUFSIZ	512						// 512: requires more than 1ms to fill at 20MHz -> issues with other interrupts such as timer.
//#define DACDMAHALFBUFSIZ	256
#define DACDMAHALFBUFSIZ	128						// Works up to at least 250KHz/1waveform.

extern const unsigned short _stm_dac_cos_lut[];
extern unsigned short _stm_dac_dmabuf[];			// DMA tempory buffer - DMA uses double buffering so buffer twice the size of DACDMABUFSIZ

extern unsigned int _stm_dac_dacclock;				// DAC clock in Hz


void stmdac_init(unsigned int samplerate,void (*cb)(unsigned short *buffer,unsigned int n));
void stmdac_deinit();
int stm_dac_cosinegenerator_addwaveform(unsigned freq, unsigned vol);
void _stm_dac_clearwaveforms();
void stm_dac_clearstat();
void stmdac_setval();
unsigned stmdac_getval();
void stm_dac_cosinegenerator_init();
void stm_dac_cosinegenerator_siggen(unsigned short *buffer,unsigned n);
#if 0
void _stm_dac_cosinegenerator_initwaveforms(unsigned n,...);
#endif
unsigned _stm_dac_computeincr(unsigned targetfrq,unsigned dacclock,unsigned lutsize);
unsigned _stm_dac_computefrqfrominc(unsigned increment,unsigned dacclock,unsigned lutsize);
void _stm_dac_deinit_timer();
void _stm_dac_init_timer(unsigned prescaler,unsigned reload);
unsigned long stmdac_stat_totframes();
unsigned long stmdac_stat_lostframes();
void stmdac_benchmaxspeed();
void stmdac_benchcpuoverhead();

#endif
