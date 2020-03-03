/*
 * stm_sdio.h
 *
 *  Created on: 10 Oct 2019
 *      Author: droggen
 */

#ifndef SD_STM_SDIO_H_
#define SD_STM_SDIO_H_

#include <stdio.h>

extern unsigned _stm_sdio_desired_divider,_stm_sdio_desired_4b;

//#define MMC_TIMEOUT_ICOMMAND					50				// Timeout to receive an answer to simple commands (ms)
#define MMC_TIMEOUT_ICOMMAND					500				// Timeout to receive an answer to simple commands (ms)
#define MMC_TIMEOUT_READWRITE					1500			// Timeout to receive an answer to read/write commands (ms)
#define SD_ERASE_TIMEOUT						20000			// Timeout for erase command (ms)
#define SD_TIMEOUT_ACMD41 						800
#define MMC_RETRY								3				// Number of times commands are retried
#define SD_DELAYBETWEENCMD	1


//void stm_sdio_init();

int stm_sdm_init();
void stm_sdm_setinitparam(int divider,int en4bw);
void stm_sdm_printcardinfo();
unsigned char sd_block_read(unsigned long addr,char *buffer);
unsigned char sd_block_write(unsigned long addr,char *buffer);
unsigned char sd_block_write_n(unsigned long addr,char *buffer,int nsector);

unsigned char sd_erase(unsigned long addr1,unsigned long addr2);

unsigned char sd_block_read_n(unsigned long addr,char *buffer,int nsector);
unsigned char sd_block_read_n_it(unsigned long addr,char *buffer,int nsector);
unsigned char sd_block_read_n_dma(unsigned long addr,char *buffer,int nsector);

void sd_print_csd(FILE *f,HAL_SD_CardCSDTypeDef *csd);
void sd_print_cid(FILE *f,HAL_SD_CardCIDTypeDef *cid);


#endif /* SD_STM_SDIO_H_ */
