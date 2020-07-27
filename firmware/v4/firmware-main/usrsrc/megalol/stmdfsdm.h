#ifndef __STMDFSDM_H
#define __STMDFSDM_H


//#define STM_DFSMD_BUFFER_NUM	8
//#define STM_DFSMD_BUFFER_NUM	16
#define STM_DFSMD_BUFFER_NUM	32
#define STM_DFSMD_BUFFER_MASK	(STM_DFSMD_BUFFER_NUM-1)
#define STM_DFSMD_BUFFER_SIZE	256
//#define STM_DFSMD_BUFFER_SIZE	1024
//#define STM_DFSMD_BUFFER_SIZE	512


#define STM_DFSMD_INIT_OFF	0
#define STM_DFSMD_INIT_8K	1
#define STM_DFSMD_INIT_16K	2
#define STM_DFSMD_INIT_20K	3
#define STM_DFSMD_INIT_24K	4
#define STM_DFSMD_INIT_32K	5
#define STM_DFSMD_INIT_48K	6
//#define STM_DFSMD_INIT_MAXMODES STM_DFSMD_INIT_48K
#define STM_DFSMD_INIT_MAXMODES STM_DFSMD_INIT_24K

//#define STM_DFSMD_TYPE		int
#define STM_DFSMD_TYPE		short

extern unsigned int _stm_dfsdm_rightshift;

void stm_dfsdm_init(unsigned mode);
void stm_dfsdm_initsampling(unsigned order,unsigned fosr,unsigned iosr,unsigned div);
void stm_dfsdm_clearstat();
unsigned long stm_dfsdm_stat_totframes();
unsigned long stm_dfsdm_stat_lostframes();
unsigned char stm_dfsdm_isdata();
unsigned char stm_dfsdm_isfull(void);
unsigned char stm_dfsdm_data_getnext(STM_DFSMD_TYPE *buffer,unsigned long *timems,unsigned long *pktctr);
void stm_dfsdm_data_clear(void);
void _stm_dfsdm_data_storenext(int *buffer,unsigned long timems);
unsigned char stm_dfsdm_data_level(void);
void stm_dfsdm_data_test();
unsigned long stm_dfsdm_acq_poll_internal(int *buffer,unsigned n);
int stm_dfsdm_calib_zero_internal();
int stm_dfsdm_acquirdmaeon();
int stm_dfsdm_acquirdmaeoff();
char *dfsdm_chcfgr1(unsigned reg);
char *dfsdm_chcfgr2(unsigned reg);
unsigned long stm_dfsmd_perfbench_withreadout(unsigned long mintime);
void stm_dfsdm_printmodes(FILE *file);
void stm_dfsdm_getmodename(unsigned char mode,char *buffer);
void stm_dfsdm_data_printstat(void);

#endif
