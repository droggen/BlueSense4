/*
 * bs4-init.c
 *
 *  Created on: 30 Jul 2019
 *      Author: droggen
 */

#include "global.h"
#include "system.h"
#include "dsystick.h"
#include "serial.h"
#include "serial_itm.h"
#include "serial_usb.h"
#include "usrmain.h"
#include "system-extra.h"
#include "rtcwrapper.h"
//#include "stm32f4xx_exti.h"
//#include "stm32f4xx_syscfg.h"
#include "stm32l4xx.h"
#include "mpu.h"
#include "stmspi.h"
#include "bs4-init.h"
#include "eeprom-m24.h"
#include "m24xx.h"
#include "rn41.h"
#include "wait.h"
#include "i2c/stmi2c.h"
#include "ltc2942.h"
#include "interface.h"
#include "power.h"
#include "ufat.h"
#include "sd/stm_sdio.h"
#include "stmdfsdm.h"
#include "uiconfig.h"


void testperiphvcc()
{
	int tc=0;
	while(1)
	{
		fprintf(file_pri,"test %d\n",tc++);
		HAL_Delay(200);
		if( (tc%100) == 50 )
		{
			fprintf(file_pri,"Calling system_periphvcc_enable\n");
			HAL_Delay(100);
			system_periphvcc_enable();
			fprintf(file_pri,"Call done.\n");
		}
		if( (tc%100) == 99 )
		{
			fprintf(file_pri,"Calling system_periphvcc_disable\n");
			HAL_Delay(100);
			system_periphvcc_disable();
			fprintf(file_pri,"Call done.\n");
		}
	}
}
void testpoweroff()
{
	int tc=0;
	while(1)
	{
		fprintf(file_pri,"test %d\n",tc++);
		HAL_Delay(200);
		if( (tc%100) == 50 )
		{
			fprintf(file_pri,"system_poweroff\n");
			HAL_Delay(100);
			system_poweroff();
			fprintf(file_pri,"Call done.\n");
		}
		if( (tc%100) == 99 )
		{
			fprintf(file_pri,"Calling system_poweron\n");
			HAL_Delay(100);
			system_poweron();
			fprintf(file_pri,"Call done.\n");
		}
	}
}

void bs4_init()
{
	bs4_init_basic();
	bs4_init_extended();
}




void bs4_init_basic()
{
	// LED Test
	system_led_test();

	system_led_set(0b000);




	// Open serial ITM file
	file_itm = serial_open_itm();

	// Open the USB file
#if 1
	file_pri = file_usb =  serial_open_usb();
	/*itmprintf("after serial_open_usb: %p\n",file_usb);
	for(int i=0;i<5;i++)
	{
		fprintf(file_usb,"Writing to file_usb: %d\n",i);
		HAL_Delay(1000);
	}*/
#else
	file_pri = file_usb = 0;
#endif

	fprintf(file_usb,"BlueSense4\n");
	fprintf(file_itm,"BlueSense4\n");

#if 1
	i2c_init();
#endif
}


void bs4_init_extended()
{
#if 1
	fprintf(file_pri,"Checking EEPROM... ");
	if(m24xx_isok())
		fprintf(file_pri,"Ok\n");
	else
		fprintf(file_pri,"Fail\n");
#endif
	// Get number of boots
	unsigned long numboot = eeprom_read_dword(STATUS_ADDR_NUMBOOT0,1,0);
	fprintf(file_pri,"Boot %lu\n",numboot);
	eeprom_write_dword(STATUS_ADDR_NUMBOOT0,numboot+1);

	// Get the VT100 mode
	__mode_vt100=ConfigLoadVT100();

#if 0 // Legacy
#if ENABLE_RTC_INTERNAL==1
	// Init internal RTC
	stmrtc_init();

	// Default RTC irq handler calls the 1Hz timer
	stmrtc_setirqhandler(stmrtc_defaultirqhandler);

	// Set time from RTC
	system_settimefromrtc();
#endif
#if ENABLE_RTC_EXTERNAL==1
	// Init external RTC
	fprintf(file_pri,"Checking external RTC... ");
	if(max31343_isok())
	{
		fprintf(file_pri,"Ok\n");
		fprintf(file_pri,"Init external RTC...\n");
		max31343_init();

		// Default RTC irq handler calls the 1Hz timer
		max31343_setirqhandler(stmrtc_defaultirqhandler);

		// Set time from RTC
		system_settimefromrtc();
	}
	else
		fprintf(file_pri,"Fail\n");
#endif
#endif
	// Init RTC
	fprintf(file_pri,"Checking %s RTC... ",rtc_name());
	if(rtc_isok())
	{
		fprintf(file_pri,"Ok\n");
		fprintf(file_pri,"Init %s RTC...\n",rtc_name());
		rtc_init();

		// Default RTC irq handler calls the 1Hz timer
		rtc_setirqhandler(stmrtc_defaultirqhandler);

		// Set time from RTC
		system_settimefromrtc();
	}
	else
		fprintf(file_pri,"Fail\n");




	// -------------------------
	// Fuel gauge initialisation
	// Get the current power data
	// This is done before calling ltc2942_init: this is ok as (except the first boot) the ltc is always configured identically.
	// However as ltc2942_init resets the counter to mid-range, the counter reading must occur here.
	power_get_state(&_poweruse_data_aton);
	// Read power data stored during last off
	system_loadpowerdata(&_poweruse_data_atoff);
	// Mark the stored data as now invalid
	eeprom_write_byte(STATUS_ADDR_OFFCURRENT_VALID,0);

	power_print_off_on_info();

#if 1
	fprintf(file_pri,"Checking fuel gauge... ");
	if(ltc2942_isok())
	{
		fprintf(file_pri,"Ok\n");
		fprintf(file_pri,"Init fuel gauge...\n");
		ltc2942_init();
	}
	else
		fprintf(file_pri,"Fail\n");
#endif






	system_periphvcc_enable();


#if BUILD_BLUETOOTH==1
	// Open serial BT usart file

	fprintf(file_pri,"USART2: %p\n",USART2);

	int p;
	file_bt = serial_open_uart(USART2,&p);
	fprintf(file_pri,"after serial_open_uart: file %p\n",file_bt);
	if(file_bt==(FILE*)-1)
	{
		while(1)
		{
			itmprintf("usart init fail\n");
			HAL_Delay(1000);
		}
	}
	else
		itmprintf("usart init ok. p: %d. file: %p\n",p,file_bt);

	//testperiphvcc();

//goto toto1;

//#if NOBLUETOOTHINIT==1
	//system_devname[0]='D';system_devname[1]='B';system_devname[2]='G';system_devname[3]=0;
//#else
	rn41_Setup(file_usb,file_bt,system_devname);
//#endif

#else
	system_devname[0]='D';system_devname[1]='B';system_devname[2]='G';system_devname[3]=0;
#endif

	//_serial_usart_enable_debug_irq(1);

toto1:

	fprintf(file_pri,"going to initialise interface\n");

	//HAL_Delay(1000);
//#endif

	// Init the interface
	interface_init();
	interface_changedetectenable(1);
	fprintf(file_pri,"Signal change\n");
	//HAL_Delay(1000);
	interface_signalchange(system_isbtconnected(),system_isusbconnected());
	//interface_test();

	fprintf(file_pri,"After signal change\n");


	// Enable SPI1
	stmspi1_stm32l4_init(2500000);

	// Enable MPU power
	system_motionvcc_enable();

	// Check MPU
#if 1
	fprintf(file_pri,"Checking MPU... ");
	if(mpu_isok())
		fprintf(file_pri,"Ok\n");
	else
		fprintf(file_pri,"Fail\n");


	// MPU INITIALISATION
	fprintf(file_pri,"Initialise MPU\n");
	mpu_init();
#endif

#if 0
	// Estimate baseline performance (optional)
	fprintf(file_pri,"Estimating baseline performance... ");
	unsigned long system_perf = system_perfbench(1);
	fprintf(file_pri,"%lu\n",system_perf);
#endif


	// Init 50Hz timer
	//dsystick_init();		// don't call
	timer_register_callback(systick_cb_50hz,19);



	// Timer callbacks
	// Register the battery sample callback, and initiate an immediate read of the battery
	ltc2942_backgroundgetstate(0);
	timer_register_slowcallback(ltc2942_backgroundgetstate,9);		// Every 10 seconds get power
	timer_register_50hzcallback(system_batterystat,4);				// Battery status at 10Hz			+  interface change detection	+ SD card change detection + RTC interrupt clear
	timer_register_slowcallback(system_lifesign,0);					// Low speed blinking



	system_status_ok(1);

#if 0
	info_cpu();

#endif

#if 1
	// Init audio
	//stm_dfsdm_init(STM_DFSMD_INIT_16K);
	stm_dfsdm_init(STM_DFSMD_INIT_OFF,STM_DFSDM_STEREO);
#endif


	// Init the various interrupts
	init_interrupts();



#if 0
	//  Push button (PA10)
	/*EXTI->IMR |= (1<<10);
	EXTI->RTSR |= (1<<10);				// Rising edge enabled
	// Enable the line 15 IRQ (PA10)
	NVIC_SetPriority(EXTI15_10_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(EXTI15_10_IRQn);*/





	//EXTI->IMR|=(1<<17);		// Enable line 17
	//EXTI->RTSR|=(1<<17);	// Rising edge line 17

	/*GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = MOTION_INT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);*/
	//LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_10)

#endif

	// Init the SD interface
	//stm_sdm_setinitparam(8,1);			// Initialisation for 20MHz bus (2.5MHz SDIO)
	//stm_sdm_setinitparam(6,1);			// Initialisation for 16MHz bus (2.66MHz SDIO)

	stm_sdm_setinitparam(4,1);			// Initialisation for 16MHz bus (4MHz SDIO)


	//stm_sdm_init();					// This will be called by FatFs driver
	ufat_init();

#if 1
	// Load and set the boot script
	char buf[CONFIG_ADDR_SCRIPTLEN];
	ConfigLoadScript(buf);
	CommandSet(buf,strlen(buf));



#endif


}



void init_interrupts()
{
	uint32_t temp;
	// Codepath BlueSense 4 v3
	// Init the MOTION_INT (PC13) external interrupt
	// Assume pin is configured as input (e.g. from Cube init code)

	temp = SYSCFG->EXTICR[3];	// Assign EXTI13 to PC13
	temp &= 0xff0f;
	temp |= 0x0020;
	SYSCFG->EXTICR[3] = temp;

	EXTI->IMR1 |= (1<<13);				// Interrupt mask register: enable Line l3
	EXTI->RTSR1 |= (1<<13);				// Rising edge enabled line 13
	// Enable the line 10-15 IRQ
	NVIC_SetPriority(EXTI15_10_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(EXTI15_10_IRQn);


	// Enable interrupt on SD_DETECT (PA8). Assume PA8 is GPIO input.
#if 0
	temp = SYSCFG->EXTICR[2];	// Assign EXTI8 to PA8
	temp &= 0xfff0;
	temp |= 0x0000;				// PA8
	SYSCFG->EXTICR[2] = temp;

	EXTI->IMR1 |= (1<<8);				// Interrupt mask register: enable Line l3
	EXTI->RTSR1 |= (1<<8);				// Rising edge enabled line 13
	EXTI->FTSR1 |= (1<<8);				// Falling edge enabled line 13
	// Enable the line 10-15 IRQ
	NVIC_SetPriority(EXTI9_5_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(EXTI9_5_IRQn);
#endif


}
