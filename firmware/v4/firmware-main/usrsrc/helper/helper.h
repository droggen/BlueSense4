#ifndef __HELPER_H
#define __HELPER_H

#include <stdio.h>
#include <string.h>


//extern const char crlf[] PROGMEM;

/*char *trim(char *string);
void bin2bcd(char *digits,unsigned int val );
void bin2bcd2(char *digit,unsigned short inValue);
void utoaz (unsigned char *outstring, unsigned int number);
void u16toascii(char *digits,unsigned int number);
void s16toascii(char *digits,unsigned int number);

unsigned char hex2chr(unsigned char v);

void helper_test(void);

extern unsigned char ht2(unsigned short v,unsigned char *ptr);
extern unsigned char ht3(unsigned short v,unsigned char *ptr);
extern unsigned char ht3s(unsigned short v,unsigned char *ptr);
*/
//#ifdef __cplusplus
//extern "C" void u16toa(unsigned short v,char *ptr);
//extern "C" void u32toa(unsigned long v,char *ptr);
//#else

//#endif


/*
#ifdef __cplusplus*/
void floatqtoa(float a,char *ptr);
void floattoa(float a,char *ptr);
/*#endif
#ifndef __cplusplus
void fract16toa(_Fract a,char *ptr);
#endif*/

//int peek(FILE *file);
//void swalloweol(FILE *file);
unsigned char checkdigits(const char *str,unsigned char n);
unsigned char ParseComma(const char *str,unsigned char n,...);
unsigned char ParseCommaGetNumParam(const char *str);
unsigned char ParseCommaGetInt(const char *str,int n,...);
unsigned char ParseCommaGetUnsigned(const char *str,int n,...);
/*unsigned char ParseCommaGetLong(const char *str,int n,...);*/

/*unsigned char TimeAddSeconds(unsigned short hour, unsigned short min, unsigned short sec, unsigned short ds,unsigned short *ohour, unsigned short *omin, unsigned short *osec);
*/
char *format3s16(char *strptr,signed short x,signed short y,signed short z);
char *format1u32(char *strptr,unsigned long a);
char *format1u16(char *strptr,unsigned short a);
/*#ifdef __cplusplus*/
char *format4qfloat(char *strptr,float q0,float q1,float q2,float q3);
char *format3float(char *strptr,float q0,float q1,float q2);
/*#endif
#ifndef __cplusplus
char *format4fract16(char *strptr,float q0,float q1,float q2,float q3);
#endif*/




void str2xxyyzz(char *string,unsigned char *x,unsigned char *y,unsigned char *z);

void prettyprint_hexascii(FILE *f,char *string,unsigned short n,unsigned char nl);

void hist_init(unsigned long *hist,unsigned short n);
void hist_insert(unsigned long *hist,unsigned short n,unsigned short width,unsigned short value);
void hist_print(FILE *file_pri,unsigned long *hist,unsigned short n,unsigned short width);
/*
void slist_add(unsigned long *slist,int n,unsigned long v);
*/
void print_bin(FILE *f,char *buffer,unsigned s);


void print_array_short(FILE *file_pri,short *data,unsigned datalen);
void print_array_char(FILE *file_pri,char *data,unsigned datalen);
void print_array_int(FILE *file_pri,int *data,unsigned datalen);
void print_array_inth(FILE *file_pri,int *data,unsigned datalen);
void scope_plot(FILE *file_pri,short *data,unsigned len,unsigned div,unsigned maxval,unsigned overlay);

#endif
