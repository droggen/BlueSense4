/*
 * stm_sdio.c
 *
 *  Created on: 10 Oct 2019
 *      Author: droggen
 */
#include <stdio.h>
#include "main.h"
#include "sdmmc.h"

#include "global.h"
#include "wait.h"
#include "stm_sdio.h"

unsigned _stm_sdio_desired_divider,_stm_sdio_desired_4b;

/*void stm_sdio_init()
{
	SD_HandleTypeDef hsd;


	// Init the peripheral
	GPIO_InitTypeDef GPIO_InitStruct = {0};


	// SDIO clock enable
	__HAL_RCC_SDIO_CLK_ENABLE();

	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	/SDIO GPIO Configuration
	//PC8     ------> SDIO_D0
	//PC12     ------> SDIO_CK
	//PD2     ------> SDIO_CMD

	GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	// SDIO interrupt Init
	//HAL_NVIC_SetPriority(SDIO_IRQn, 0, 0);
	//HAL_NVIC_EnableIRQ(SDIO_IRQn);


*/
/*

	hsd.Instance = SDIO;
	hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
	hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
	hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
	hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
	hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
	//hsd.Init.ClockDiv = 255;
	hsd.Init.ClockDiv = 2;

	hsd->State = HAL_SD_STATE_BUSY;

	// Initialize the Card parameters
	HAL_SD_InitCard(hsd);

	// Initialize the error code
	hsd->ErrorCode = HAL_DMA_ERROR_NONE;

	// Initialize the SD operation
	hsd->Context = SD_CONTEXT_NONE;

	// Initialize the SD state
	hsd->State = HAL_SD_STATE_READY;

*/
/*
	SDIO->POWER = 0b11;		// Power on the card
//	SDIO->CLKCR =


}*/
// Set initialisation parameters but does not do the initialisation
void stm_sdm_setinitparam(int divider,int en4bw)
{
	// Copy these settings to override the ST BSP driver linked to FatFS
	_stm_sdio_desired_divider = divider;
	_stm_sdio_desired_4b = en4bw;
}

/*
	Returns:
		0		-		Ok
		Other	-		Failure
 */
int stm_sdm_init()
{
	// Low level initialisation and card info
	HAL_StatusTypeDef s;

	fprintf(file_pri,"SD init: init interface with clkdiv %d and %c-bit bus\n",_stm_sdio_desired_divider,_stm_sdio_desired_4b?'4':'1');




	hsd1.Instance = SDMMC1;
	hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
	hsd1.Init.ClockBypass = SDMMC_CLOCK_BYPASS_DISABLE;
	hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
	hsd1.Init.BusWide = SDMMC_BUS_WIDE_1B;							// This MUST be set to 1B for the initialisation
	//hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
	hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_ENABLE;
	hsd1.Init.ClockDiv = _stm_sdio_desired_divider;

	if( (s=HAL_SD_Init(&hsd1)))
	{
		fprintf(file_pri,"\tHAL_SD_Init failed: %d\n",s);
		return s;
	}

	// Configure SD Bus width (4 bits mode selected)
	if(_stm_sdio_desired_4b)
	{
		fprintf(file_pri,"\tEnable 4-bit bus... ");
		// Enable wide operation
		if( (s=HAL_SD_ConfigWideBusOperation(&hsd1, SDMMC_BUS_WIDE_4B)) != HAL_OK)
		{
			fprintf(file_pri,"error\n");
			return s;
		}
		else
			fprintf(file_pri,"Ok\n");
	}
	else
		fprintf(file_pri,"\t1-bit bus\n");




	return s;

	/*fprintf(file_pri,"Init SD interface: %d\n",s);

	// This card init is not necessary to get the CSD, CID data
	fprintf(file_pri,"Init card\n");
	s = HAL_SD_InitCard(&hsd1);
	fprintf(file_pri,"Init card: %d\n",s);*/

}
void stm_sdm_printcardinfo()
{
	HAL_StatusTypeDef s;
	HAL_SD_CardCSDTypeDef csd;
	HAL_SD_CardCIDTypeDef cid;

	fprintf(file_pri,"Getting CSD\n");
	s = HAL_SD_GetCardCSD(&hsd1,&csd);
	fprintf(file_pri,"Getting CSD: %d\n",s);

	sd_print_csd(file_pri,&csd);

	fprintf(file_pri,"Getting CID\n");
	s = HAL_SD_GetCardCID(&hsd1,&cid);
	fprintf(file_pri,"Getting CID: %d\n",s);

	sd_print_cid(file_pri,&cid);


	fprintf(file_pri,"Getting card info\n");

	HAL_SD_CardInfoTypeDef ci;
	s = HAL_SD_GetCardInfo(&hsd1,&ci);

	fprintf(file_pri,"Getting card info: %d\n",s);

	fprintf(file_pri,"\tBlock number: %u\n",(unsigned)ci.BlockNbr);
	fprintf(file_pri,"\tBlock size: %u\n",(unsigned)ci.BlockSize);
	fprintf(file_pri,"\tCard type: %u\n",(unsigned)ci.CardType);
	fprintf(file_pri,"\tCard version: %u\n",(unsigned)ci.CardVersion);
	fprintf(file_pri,"\tClass: %u\n",(unsigned)ci.Class);
	fprintf(file_pri,"\tLogical block number: %u\n",(unsigned)ci.LogBlockNbr);
	fprintf(file_pri,"\tLogical block size: %u\n",(unsigned)ci.LogBlockSize);
	fprintf(file_pri,"\tRelCardAdd: %u\n",(unsigned)ci.RelCardAdd);



	// OCR ? not available in STM HAL
}
/******************************************************************************
	function: sd_block_read
*******************************************************************************
	Reads a sector of data from sector addr.

	The block size is fixed at 512 bytes, i.e. one sector.
	Uses internally the single block write function.

	Parameters:
		addr		-	Address in sector (0=first sector, 1=second sector, ...)
		buffer		-	Buffer of 512 bytes which receives the data

	Returns:
		0			- 	Success
		nonzero		- 	Error
******************************************************************************/
unsigned char sd_block_read(unsigned long addr,char *buffer)
{
	HAL_StatusTypeDef s = HAL_SD_ReadBlocks(&hsd1,buffer,addr,1,MMC_TIMEOUT_READWRITE);
	if(s)
	{
		return s;
	}

	HAL_SD_CardStateTypeDef s2;
	s2 = HAL_SD_GetCardState(&hsd1);

	fprintf(file_pri,"s: %d s2: %d. buffer[0]: %02X\n",s,s2,buffer[0]);

//	HAL_Delay(100);
//	s2 = HAL_SD_GetCardState(&hsd1);

//	fprintf(file_pri,"\ts: %d s2: %d\n",s,s2);


	return s2;

	// Wait until the card leaves the program state
	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while(s2 == HAL_SD_CARD_PROGRAMMING && ((t2=timer_ms_get())-t1<MMC_TIMEOUT_READWRITE));
	fprintf(file_pri,"Read done (status: %d; time: %u ms)\n",s2,t2-t1);

	return (s2==HAL_SD_CARD_PROGRAMMING)?0:1;
}
unsigned char sd_block_read_n(unsigned long addr,char *buffer,int nsector)
{
	HAL_StatusTypeDef s = HAL_SD_ReadBlocks(&hsd1,buffer,addr,nsector,MMC_TIMEOUT_READWRITE);
	if(s)
	{
		return s;
	}

	HAL_SD_CardStateTypeDef s2;
	s2 = HAL_SD_GetCardState(&hsd1);

	fprintf(file_pri,"s: %d s2: %d. buffer: %02X:%02X%02X %02X  %02X:%02X%02X %02X\n",s,s2,buffer[0],buffer[1],buffer[2],buffer[3],buffer[509],buffer[510],buffer[511],buffer[508]);

//	HAL_Delay(100);
//	s2 = HAL_SD_GetCardState(&hsd1);

//	fprintf(file_pri,"\ts: %d s2: %d\n",s,s2);


	return s2;

	// Wait until the card leaves the program state
	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while(s2 == HAL_SD_CARD_PROGRAMMING && ((t2=timer_ms_get())-t1<MMC_TIMEOUT_READWRITE));
	fprintf(file_pri,"Read done (status: %d; time: %u ms)\n",s2,t2-t1);

	return (s2==HAL_SD_CARD_PROGRAMMING)?0:1;
}
unsigned char sd_block_read_n_it(unsigned long addr,char *buffer,int nsector)
{
	HAL_StatusTypeDef s = HAL_SD_ReadBlocks_IT(&hsd1,buffer,addr,nsector);
	if(s)
	{
		return s;
	}

	HAL_SD_CardStateTypeDef s2;

	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while(s2 == HAL_SD_CARD_SENDING && ((t2=timer_ms_get())-t1<MMC_TIMEOUT_READWRITE));
	fprintf(file_pri,"Read done (status: %d; time: %u ms; cnt %u). ",s2,t2-t1,cnt);
	fprintf(file_pri,"s: %d s2: %d. buffer: %02X:%02X%02X %02X  %02X:%02X%02X %02X\n",s,s2,buffer[0],buffer[1],buffer[2],buffer[3],buffer[509],buffer[510],buffer[511],buffer[508]);
	return s2;
}
unsigned char sd_block_read_n_dma(unsigned long addr,char *buffer,int nsector)
{
	HAL_StatusTypeDef s = HAL_SD_ReadBlocks_DMA(&hsd1,buffer,addr,nsector);
	if(s)
	{
		fprintf(file_pri,"s %d%d\n",s);
		return s;
	}

	HAL_SD_CardStateTypeDef s2;

	s2 = HAL_SD_GetCardState(&hsd1);
	fprintf(file_pri,"s %d s2: %d\n",s,s2);
	HAL_Delay(500);
	s2 = HAL_SD_GetCardState(&hsd1);
	fprintf(file_pri,"s %d s2: %d\n",s,s2);



	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while(s2 == HAL_SD_CARD_SENDING && ((t2=timer_ms_get())-t1<MMC_TIMEOUT_READWRITE));
	fprintf(file_pri,"Read done (status: %d; time: %u ms; cnt %u). ",s2,t2-t1,cnt);
	fprintf(file_pri,"s: %d s2: %d. buffer: %02X:%02X%02X %02X  %02X:%02X%02X %02X\n",s,s2,buffer[0],buffer[1],buffer[2],buffer[3],buffer[509],buffer[510],buffer[511],buffer[508]);
	return s2;
}

/******************************************************************************
	function: sd_block_write
*******************************************************************************
	Writes a sector of data at sector addr.

	The block size is fixed at 512 bytes, i.e. one sector.
	Uses internally the single block write function.

	Parameters:
		addr		-	Address in sector (0=first sector, 1=second sector, ...)
		buffer		-	Buffer of 512 bytes of data to write

	Returns:
		0			- 	Success
		nonzero		- 	Error
******************************************************************************/
unsigned char sd_block_write(unsigned long addr,char *buffer)
{
	unsigned char response;

	//DRESULT r = SD_write(0,block,sector,1);
	HAL_StatusTypeDef s = HAL_SD_WriteBlocks(&hsd1,buffer,addr,1,MMC_TIMEOUT_READWRITE);
	if(s)
	{
		return s;
	}

	HAL_SD_CardStateTypeDef s2;
	s2 = HAL_SD_GetCardState(&hsd1);

	//fprintf(file_pri,"s2: %d\n",s2);

	// Wait until the card leaves the program state
	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while(s2 == HAL_SD_CARD_PROGRAMMING && ((t2=timer_ms_get())-t1<MMC_TIMEOUT_READWRITE));
	//fprintf(file_pri,"Write done (status: %d; time: %u ms)\n",s2,t2-t1);

	return (s2==HAL_SD_CARD_PROGRAMMING)?1:0;
}

unsigned char sd_block_write_n(unsigned long addr,char *buffer,int nsector)
{
	unsigned char response;

	HAL_SD_CardStateTypeDef sprev = HAL_SD_GetCardState(&hsd1);

	//DRESULT r = SD_write(0,block,sector,1);
	HAL_StatusTypeDef s = HAL_SD_WriteBlocks(&hsd1,buffer,addr,nsector,MMC_TIMEOUT_READWRITE);
	if(s)
	{
		fprintf(file_pri,"err %d. error code: %u state: %u. sprev: %u\n",s,hsd1.ErrorCode,hsd1.State,sprev);
		return s;
	}

	HAL_SD_CardStateTypeDef s2;
	/*s2 = HAL_SD_GetCardState(&hsd1);

	fprintf(file_pri,"s: %d s2: %d.\n",s,s2);

	HAL_Delay(500);

	s2 = HAL_SD_GetCardState(&hsd1);

	fprintf(file_pri,"s: %d s2: %d.\n",s,s2);

	return s2;*/

	//fprintf(file_pri,"s2: %d\n",s2);

	HAL_SD_CardStateTypeDef s0 = HAL_SD_GetCardState(&hsd1);

	// Wait until the card leaves the program state
	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while( ((t2=timer_ms_get())-t1<MMC_TIMEOUT_READWRITE) && (s2 == HAL_SD_CARD_PROGRAMMING) );
	fprintf(file_pri,"Write done (status: %d; time: %u ms; cnt %u)\n",s2,t2-t1,cnt);
	//HAL_Delay(50);
	//HAL_SD_CardStateTypeDef s3 = HAL_SD_GetCardState(&hsd1);
	//fprintf(file_pri,"\t%d %d\n",s0,s3);

	//HAL_Delay(6);
	//HAL_Delay(1);


	return (s2==HAL_SD_CARD_PROGRAMMING)?1:0;
}


// 0: success, otherwise error
unsigned char sd_erase(unsigned long addr1,unsigned long addr2)
{
	HAL_StatusTypeDef s1 = HAL_SD_Erase(&hsd1,addr1,addr2);
	if(s1!=HAL_OK)
	{
		fprintf(file_pri,"Erase request failed (%d)\n",s1);
		return 1;
	}

	HAL_SD_CardStateTypeDef s2;
	s2 = HAL_SD_GetCardState(&hsd1);

	//fprintf(file_pri,"s1: %d s2: %d\n",s1,s2);

	//if(s1!=HAL_OK || s2!=HAL_SD_CARD_READY)
		//return 1;

	// Wait until the card leaves the program state
	unsigned t1 = timer_ms_get(),t2;
	unsigned cnt=0;
	do
	{
		s2 = HAL_SD_GetCardState(&hsd1);
		cnt++;
	}
	while(s2 == HAL_SD_CARD_PROGRAMMING && ((t2=timer_ms_get())-t1<SD_ERASE_TIMEOUT));
	fprintf(file_pri,"Erase done (status: %d; time: %u)\n",s2,t2-t1);


	return (s2==HAL_SD_CARD_PROGRAMMING)?0:1;
}

/******************************************************************************
	function: sd_print_csd
*******************************************************************************
	Prints the CSD fields to a stream.

	Parameters:
		f			-	Stream on which to print
		csd			-	Pointer to CSD
******************************************************************************/
void sd_print_csd(FILE *f,HAL_SD_CardCSDTypeDef *csd)
{
	fprintf(f,"CSD:\n");
	fprintf(f,"\tCSD structure: %d\n",csd->CSDStruct);
	fprintf(f,"\tTAAC: %d\n",csd->TAAC);
	fprintf(f,"\tNSAC: %d\n",csd->NSAC);
	//fprintf(f,"\tTRAN_SPEED: %X\n",csd->TRAN_SPEED);
	fprintf(f,"\tMaxBusClkFrq: %d\n",csd->MaxBusClkFrec);			// Could be TRAN_SPEED
	//fprintf(f,"\tCCC: %X\n",csd->CCC);
	fprintf(f,"\tCardCommandClass: %d\n",csd->CardComdClasses);		// Could be CCC
	fprintf(f,"\tREAD_BL_LEN: %d\n",csd->RdBlockLen);
	fprintf(f,"\tREAD_BL_PARTIAL: %d\n",csd->PartBlockRead);
	fprintf(f,"\tWRITE_BLK_MISALIGN: %d\n",csd->WrBlockMisalign);
	fprintf(f,"\tREAD_BLK_MISALIGN: %d\n",csd->RdBlockMisalign);
	fprintf(f,"\tDSR_IMP: %d\n",csd->DSRImpl);
	if(csd->CSDStruct==0)
	{
		//fprintf(f,"\tC_SIZE: %04X\n",csd->C_SIZE);
		fprintf(f,"\tC_SIZE: %u\n",csd->DeviceSize);				// DeviceSize is C_SIZE
		fprintf(f,"\tVDD_R_CURR_MIN: %X\n",csd->MaxRdCurrentVDDMin);
		fprintf(f,"\tVDD_R_CURR_MAX: %X\n",csd->MaxRdCurrentVDDMax);
		fprintf(f,"\tVDD_W_CURR_MIN: %X\n",csd->MaxWrCurrentVDDMin);
		fprintf(f,"\tVDD_W_CURR_MAX: %X\n",csd->MaxWrCurrentVDDMax);
		fprintf(f,"\tC_SIZE_MULT: %X\n",csd->DeviceSizeMul);
	}
	else
	{
		fprintf(f,"\tC_SIZE: %08lX (%ld KB)\n",csd->DeviceSize,(csd->DeviceSize+1)*512);
	}
	//fprintf(f,"\tERASE_BLK_EN: %X\n",csd->ERASE_BLK_EN);		// ERASE_BLK_EN is EraseGrSize
	fprintf(f,"\tEraseGrSize: %d\n",csd->EraseGrSize);			// ERASE_BLK_EN is EraseGrSize
	//fprintf(f,"\tSECTOR_SIZE: %X\n",csd->SECTOR_SIZE);		// SECTOR_SIZE is EraseGrMul
	fprintf(f,"\tEraseGrMul: %d\n",csd->EraseGrMul);			// SECTOR_SIZE is EraseGrMul

	fprintf(f,"\tWP_GRP_SIZE	: %d\n",csd->WrProtectGrSize);
	fprintf(f,"\tWP_GRP_ENABLE: %d\n",csd->WrProtectGrEnable);
	fprintf(f,"\tManDeflECC: %d\n",csd->ManDeflECC);		// New in STM
	//fprintf(f,"\tR2W_FACTOR: %X\n",csd->R2W_FACTOR);		// could be wrspeed?
	fprintf(f,"\tWrSpeedFact: %d\n",csd->WrSpeedFact);
	fprintf(f,"\tWRITE_BL_LEN: %d\n",csd->MaxWrBlockLen);
	fprintf(f,"\tWRITE_BL_PARTIAL: %d\n",csd->WriteBlockPaPartial);
	fprintf(f,"\tContentProtectAppl: %d\n",csd->ContentProtectAppli);
	fprintf(f,"\tFILE_FORMAT_GRP: %d\n",csd->FileFormatGroup);
	fprintf(f,"\tCOPY: %d\n",csd->CopyFlag);
	fprintf(f,"\tPERM_WRITE_PROTECT: %d\n",csd->PermWrProtect);
	fprintf(f,"\tTMP_WRITE_PROTECT: %d\n",csd->TempWrProtect);
	fprintf(f,"\tFILE_FORMAT: %d\n",csd->FileFormat);
	fprintf(f,"\tECC: %d\n",csd->ECC);						// New in STM
	fprintf(f,"\tCRC: %X\n",csd->CSD_CRC);
}
/******************************************************************************
	function: sd_print_cid
*******************************************************************************
	Prints the CID fields to a stream.

	Parameters:
		f			-	Stream on which to print
		cid			-	Pointer to CID
******************************************************************************/
void sd_print_cid(FILE *f,HAL_SD_CardCIDTypeDef *cid)
{
	fprintf(f,"CID:\n");
	fprintf(f,"\tMID: %02X\n",cid->ManufacturerID);
	fprintf(f,"\tOID: %04X\n",cid->OEM_AppliID);
	//fprintf(f,"\tProduct name: %s\n",cid->PNM);
	fprintf(f,"\tProduct name: %c%c%c%c%c\n",(cid->ProdName1>>24)&0xff,(cid->ProdName1>>16)&0xff,(cid->ProdName1>>8)&0xff,(cid->ProdName1>>0)&0xff,cid->ProdName2);
	fprintf(f,"\tProduct revision: %02X\n",cid->ProdRev);
	fprintf(f,"\tProduct serial: %02X\n",cid->ProdSN);
	fprintf(f,"\tMDT: %02X %d/%d\n",cid->ManufactDate,cid->ManufactDate&0b1111,2000+(cid->ManufactDate>>4));
}




