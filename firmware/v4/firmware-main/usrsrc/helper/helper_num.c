/*
 * helper_num.c
 *
 *  Created on: 29 sept. 2019
 *      Author: droggen
 */

#include <stdio.h>
#include "usrmain.h"
#include "global.h"
#include "helper_num.h"

/******************************************************************************
	function: u32toa
*******************************************************************************
Converts a 32-bit unsigned number into a 11 byte ascii string (10 bytes for the
number + one trailing null).

	Parameters:
		v		-	32-bit unsigned value
		ptr		-	pointer where the data is placed


	Returns:
		-
*******************************************************************************/
void u32toa_div1(unsigned v,unsigned char *ptr)
{
	char buf[16];
	utoa(v,buf,10);
	int l = strlen(buf);
	// Shift the number so that the rightmost digit is at ptr[9]
	for(unsigned i=0;i<l;i++)
	{
		ptr[10-l+i] = buf[i];
	}
	for(unsigned i=0;i<10-l;i++)
		ptr[i]='0';
	ptr[10]=0;
}
void u32toa_div2(unsigned v,unsigned char *ptr)
{
	unsigned d = 1000000000;
	unsigned n;

	for(int i=0;i<10;i++)
	{
		n = v/d;
		v=v-d*n;
		d=d/10;
		ptr[i]='0'+n;
	}
	ptr[10]=0;
}
void u32toa_div3(unsigned v,unsigned char *ptr)
{
	unsigned d[10] = {1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1};
	unsigned n;

	for(int i=0;i<10;i++)
	{
		n = v/d[i];
		v=v-d[i]*n;
		ptr[i]='0'+n;
	}
	ptr[10]=0;
}
unsigned __d[10] = {1000000000,100000000,10000000,1000000,100000,10000,1000,100,10,1};
void u32toa_div4(unsigned v,unsigned char *ptr)
{
	unsigned n;

	for(int i=0;i<10;i++)
	{
		n = v/__d[i];
		v=v-__d[i]*n;
		ptr[i]='0'+n;
	}
	ptr[10]=0;
}
void u32toa_div5(unsigned v,unsigned char *ptr)
{
	unsigned n;

	for(int i=0;i<9;i++)
	{
		n = v/__d[i];
		v=v-__d[i]*n;
		ptr[i]='0'+n;
	}
	ptr[9] = '0'+v;
	ptr[10]=0;
}

void u32toa_sub(unsigned v,unsigned char *ptr)
{
	unsigned char r;
	unsigned long long vo;

	//fprintf(file_pri,"Input: %u\n",v);

	// This function should make use of the carry flag to be fast.
	// In C we use 64-bits to obtain the carry
	vo=v;
	//fprintf(file_pri,"sizeof unsigned long. %u\n",sizeof(unsigned long long));



	r = -1+'0';
//_u32toa_bcd1:
	do
	{
		r++;
		vo-=1000000000;

		//fprintf(file_pri,"r: %u vo: %llu\n",r,vo);
	}
	// Loop until carry set
	while(!(vo&0x8000000000000000));
	//fprintf(file_pri,"end loop: r: %c\n",r);
	*ptr=r;
	ptr++;


	r = 10+'0';
//_u32toa_bcd2:
	do
	{
		r--;
		vo+=100000000;
		//fprintf(file_pri,"r: %u vo: %llu\n",r,vo);
	}
	// Loop until carry clear
	while((vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = -1+'0';
//_u32toa_bcd3:
	do
	{
		r++;
		vo-=10000000;
	}
	// Loop until carry set
	while(!(vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = 10+'0';
//_u32toa_bcd4:
	do
	{
		r--;
		vo+=1000000;
	}
	// Loop until carry clear
	while((vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = -1+'0';
//_u32toa_bcd5:
	do
	{
		r++;
		vo-=100000;
	}
	// Loop until carry set
	while(!(vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = 10+'0';
//_u32toa_bcd6:
	do
	{
		r--;
		vo+=10000;
	}
	// Loop until carry clear
	while((vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = -1+'0';
//_u32toa_bcd7:
	do
	{
		r++;
		vo-=1000;
	}
	// Loop until carry set
	while(!(vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = 10+'0';
//_u32toa_bcd8:
	do
	{
		r--;
		vo+=100;
	}
	// Loop until carry clear
	while((vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = -1+'0';
//_u32toa_bcd9:
	do
	{
		r++;
		vo-=10;
	}
	// Loop until carry set
	while(!(vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	r = 10+'0';
//_u32toa_bcd10:
	do
	{
		r--;
		vo+=1;
	}
	// Loop until carry clear
	while((vo&0x8000000000000000));
	*ptr=r;
	ptr++;

	*ptr=0;

	//fprintf(file_pri,"v: %u '%s'\n",v,ptro);

	//
}

/******************************************************************************
	function: u16toa
*******************************************************************************
Converts a 16-bit unsigned number into a 6 byte ascii string (5 bytes for the
number + one trailing null).

	Parameters:
		v		-	16-bit unsigned value
		ptr		-	pointer where the data is placed


	Returns:
		-
*******************************************************************************/
unsigned ___d[10] = {10000,1000,100,10,1};
void u16toa_div5(unsigned short v,unsigned char *ptr)
{
	unsigned n;
	//unsigned *___d = __d+5; // Get the correct divider table

	for(int i=0;i<4;i++)
	{
		n = v/___d[i];
		v=v-___d[i]*n;
		ptr[i]='0'+n;
	}
	ptr[4] = '0'+v;
	ptr[5]=0;
}
void u16toa_sub(unsigned short v,unsigned char *ptr)
{
	unsigned char *ptro = ptr;
	unsigned char r;
	unsigned long vo;

	//fprintf(file_pri,"Input: %u\n",v);

	// This function should make use of the carry flag to be fast.
	// In C we use 32-bits to obtain the carry
	vo=v;
	//fprintf(file_pri,"sizeof unsigned long. %u\n",sizeof(unsigned long long));



	r = -1+'0';
//_u32toa_bcd1:
	do
	{
		r++;
		vo-=10000;
	}
	// Loop until carry set
	while(!(vo&0x80000000));
	*ptr=r;
	ptr++;


	r = 10+'0';
//_u32toa_bcd2:
	do
	{
		r--;
		vo+=1000;
	}
	// Loop until carry clear
	while((vo&0x80000000));
	*ptr=r;
	ptr++;


	r = -1+'0';
//_u32toa_bcd3:
	do
	{
		r++;
		vo-=100;
	}
	// Loop until carry set
	while(!(vo&0x80000000));
	*ptr=r;
	ptr++;


	r = 10+'0';
//_u32toa_bcd4:
	do
	{
		r--;
		vo+=10;
	}
	// Loop until carry clear
	while((vo&0x80000000));
	*ptr=r;
	ptr++;

	r = -1+'0';
//_u32toa_bcd5:
	do
	{
		r++;
		vo-=1;
	}
	// Loop until carry set
	while(!(vo&0x80000000));
	*ptr=r;
	ptr++;


	*ptr=0;

	//fprintf(file_pri,"v: %u '%s'\n",v,ptro);

	//
}

/******************************************************************************
	function: s16toa
*******************************************************************************
	Converts a signed 16-bit number into a 7 byte ascii string (6 for the number
	and sign + 1 null-terminator).
******************************************************************************/
void s16toa(signed short v,char *ptr)
{
	if(v&0x8000)
	{
		ptr[0]='-';
		v=-v;
	}
	else
		ptr[0]=' ';
	u16toa(v,ptr+1);
}
/******************************************************************************
	function: s32toa
*******************************************************************************
	Converts a signed 32-bit number into a 12 byte ascii string (11 for the number
	and sign + 1 null-terminator).
******************************************************************************/
void s32toa(signed int v,char *ptr)
{
	if(v&0x80000000)
	{
		ptr[0]='-';
		v=-v;
	}
	else
		ptr[0]=' ';
	u32toa(v,ptr+1);
}
/******************************************************************************
	function: n16tobin
*******************************************************************************
	Converts a 16-bit number into a 16-bit binary bitstring followed by a
	terminating null.
	The buffer must have space for at least 17 bytes.
******************************************************************************/
void n16tobin(unsigned short n,char *ptr)
{
	for(unsigned char i=0;i<16;i++)
	{
		if(n&0x8000)
			*ptr='1';
		else
			*ptr='0';
		ptr++;
		n<<=1;
	}
	*ptr=0;
}

/******************************************************************************
	function: log2rndfloor
*******************************************************************************
	Returns floor(log2(val))
******************************************************************************/
unsigned log2rndfloor(unsigned val)
{
	for(int i=31;i>=0;i--)
	{
		if(val&(1<<i))
			return i;
	}
	return 0;
}
/******************************************************************************
	function: log2rndceil
*******************************************************************************
	Returns ceil(log2(val))
******************************************************************************/
unsigned log2rndceil(unsigned val)
{
	for(int i=31;i>=0;i--)
	{
		if(val&(1<<i))
		{
			// Must round up if any bits right of i is set. Use a mask to select all bits right of i.
			if(val & ((1<<i)-1))
				i++;
			return i;
		}
	}
	return 0;
}
