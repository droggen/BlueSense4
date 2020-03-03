/*
 * isr.c


 *
 *  Created on: 6 oct. 2019
 *      Author: droggen
 */

#include "main.h"
#include "mpu.h"

//
void EXTI15_10_IRQHandler()
{
	// Must deactivate the interrupt
	//EXTI->PR = (1<<10);
	//EXTI->PR = (1<<15);
	EXTI->PR1 = (1<<13);

	//void system_led_toggle(unsigned char led)
	//itmprintf("I\n");
	//fprintf(file_pri,"I\n");

	//system_led_toggle(0b100);

	// call the mpu isr
	mpu_isr();

}

#if 0
void EXTI9_5_IRQHandler()
{
	//system_led_toggle(0b100);
	fprintf(file_pri,"I95 %08X\n",EXTI->PR1);
	//itmprintf("I\n");


	// Only SD_DETECT=PA8
	EXTI->PR1 = (1<<8);

	// Must manually clear interrupt
	//EXTI->PR = (1<<5);			// Only PB5

}
#endif




/*void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
	fprintf(file_pri,"SDTX\n");
}
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
	fprintf(file_pri,"SDRX\n");
}
void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
	fprintf(file_pri,"SDERR\n");
}
void HAL_SD_AbortCallback(SD_HandleTypeDef *hsd)
{
	fprintf(file_pri,"SDABT\n");
}
*/

#if 0
void BSP_SD_AbortCallback(void)
{
	fprintf(file_pri,"SDABT\n");
}

/**
  * @brief BSP Tx Transfer completed callback
  * @retval None
  */
void BSP_SD_WriteCpltCallback(void)
{
	fprintf(file_pri,"SDTX\n");
}

/**
  * @brief BSP Rx Transfer completed callback
  * @retval None
  */
void BSP_SD_ReadCpltCallback(void)
{
	fprintf(file_pri,"SDRX\n");
}


#endif


/*void DFSDM1_FLT0_IRQHandler()
{
	fprintf(file_pri,"F\n");
}
*/
