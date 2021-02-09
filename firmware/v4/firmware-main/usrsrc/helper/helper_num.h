#ifndef __HELPER_NUM_H
#define __HELPER_NUM_H

// Select which conversion function to use.
// On ARM with -O2 the best is u32toa_div6 and u16toa_div6 (with -O3 the version 5 is faster)
//#define u32toa u32toa_sub
#define u32toa u32toa_div6
//#define u16toa u16toa_sub
#define u16toa u16toa_div6




void u32toa_div1(unsigned v,unsigned char *ptr);
void u32toa_div2(unsigned v,unsigned char *ptr);
void u32toa_div3(unsigned v,unsigned char *ptr);
void u32toa_div4(unsigned v,unsigned char *ptr);
void u32toa_div5(unsigned v,unsigned char *ptr);
void u32toa_div6(unsigned v,unsigned char *ptr);
void u32toa_sub(unsigned v,unsigned char *ptr);
void u16toa_div5(unsigned short v,unsigned char *ptr);
void u16toa_div6(unsigned short v,unsigned char *ptr);
void u16toa_sub(unsigned short v,unsigned char *ptr);
void s16toa(signed short v,char *ptr);
void s32toa(signed int v,char *ptr);
void n16tobin(unsigned short n,char *ptr);
unsigned log2rndfloor(unsigned val);
unsigned log2rndceil(unsigned val);

#endif
