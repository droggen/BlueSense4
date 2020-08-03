#ifndef __STMDFSDM_H
#define __STMDFSDM_H

#include "dfsdm.h"

//#define STM_DFSMD_BUFFER_NUM	8
//#define STM_DFSMD_BUFFER_NUM	16
#define STM_DFSMD_BUFFER_NUM	32
#define STM_DFSMD_BUFFER_MASK	(STM_DFSMD_BUFFER_NUM-1)
#define STM_DFSMD_BUFFER_SIZE	256
//#define STM_DFSMD_BUFFER_SIZE	1024
//#define STM_DFSMD_BUFFER_SIZE	512


#define STM_DFSDM_LEFT		0
#define STM_DFSDM_RIGHT		1
#define STM_DFSDM_STEREO	2


#define STM_DFSMD_INIT_OFF	0
#define STM_DFSMD_INIT_8K	1
#define STM_DFSMD_INIT_16K	2
#define STM_DFSMD_INIT_20K	3
#define STM_DFSMD_INIT_24K	4
#define STM_DFSMD_INIT_32K	5
#define STM_DFSMD_INIT_48K	6
//#define STM_DFSMD_INIT_MAXMODES STM_DFSMD_INIT_48K
//#define STM_DFSMD_INIT_MAXMODES STM_DFSMD_INIT_24K
#define STM_DFSMD_INIT_MAXMODES STM_DFSMD_INIT_20K
// 24K leads to overruns in stereo when writing to log.

//#define STM_DFSMD_TYPE		int
#define STM_DFSMD_TYPE		short

extern unsigned int _stm_dfsdm_rightshift;
extern DFSDM_Filter_HandleTypeDef *_stm_dfsdm_filters[2];
extern DFSDM_Channel_HandleTypeDef *_stm_dfsdm_channels[2];

void stm_dfsdm_init(unsigned mode,unsigned char left_right);
void stm_dfsdm_initsampling(unsigned char left_right,unsigned order,unsigned fosr,unsigned iosr,unsigned div);
void _stm_dfsdm_initsampling_internal(unsigned char left_right,unsigned order,unsigned fosr,unsigned iosr,unsigned div);
void stm_dfsdm_acquire_stop_all();
void stm_dfsdm_acquire_stop_poll();
void _stm_dfsdm_init_predef(unsigned mode,unsigned char left_right);
unsigned char stm_dfsdm_isdata();
unsigned char stm_dfsdm_isfull(void);
unsigned char stm_dfsdm_data_getnext(STM_DFSMD_TYPE *buffer,unsigned long *timems,unsigned long *pktctr,unsigned char *left_right);
void stm_dfsdm_data_clear(void);
void _stm_dfsdm_data_storenext(int *buffer,unsigned long timems,unsigned char left_right);
unsigned char stm_dfsdm_data_level(void);
void stm_dfsdm_data_test();
unsigned long stm_dfsdm_acq_poll_internal(unsigned char left_right,int *buffer,unsigned n);
void _stm_dfsdm_offset_calibrate(unsigned char left_right,unsigned char print);
int _stm_dfsdm_offset_measure(unsigned char left_right,int *offset_l,int *offset_r,unsigned char print);
int stm_dfsdm_acquire_start_dma(unsigned char left_right);
int stm_dfsdm_acquire_dma_stop();
char *dfsdm_chcfgr1(unsigned reg);
char *dfsdm_chcfgr2(unsigned reg);
unsigned long stm_dfsmd_perfbench_withreadout(unsigned long mintime);
void stm_dfsdm_printmodes(FILE *file);
void stm_dfsdm_getmodename(unsigned char mode,char *buffer);


void stm_dfsdm_acquire_start_poll(unsigned char left_right);
void stm_dfsdm_offset_set(unsigned char left_right,int offset);
void stm_dfsdm_rightshift_set(unsigned char left_right,int shift);


// Statistics
void stm_dfsdm_stat_clear();
unsigned long stm_dfsdm_stat_totframes();
unsigned long stm_dfsdm_stat_lostframes();
void stm_dfsdm_stat_print(void);
// Various
unsigned stm_dfsdm_memoryused();
void stm_dfsdm_state_print();
void stm_dfsdm_printreg();

#endif
