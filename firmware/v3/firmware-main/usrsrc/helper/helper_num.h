#ifndef __HELPER_NUM_H
#define __HELPER_NUM_H

void u32toa(unsigned v,unsigned char *ptr);
void u16toa(unsigned short v,unsigned char *ptr);
void s16toa(signed short v,char *ptr);
void s32toa(signed int v,char *ptr);
void n16tobin(unsigned short n,char *ptr);
unsigned log2rndfloor(unsigned val);
unsigned log2rndceil(unsigned val);

#endif
