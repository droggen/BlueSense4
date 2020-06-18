// system-extra: system functions suitable for application code only (not for bootloader)





#include "wait.h"
#include "global.h"
//#include "interface.h"
//#include "megalol/i2c_internal.h"
//#include "ltc2942.h"
//#include "ds3232.h"
//#include "mpu.h"
#include "usrmain.h"
/*#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_cdc_if.h"
#include "usbd_desc.h"*/

#include "system.h"
#include "system-extra.h"
//#include "init.h"
#include "ltc2942.h"
#include "eeprom-m24.h"
#include "serial_itm.h"
#include "stmrtc.h"
#include "main.h"
#include "interface.h"
#include "ufat.h"

volatile unsigned char usb_must_enable=0;

/******************************************************************************
	function: system_getrtcint
*******************************************************************************	
	Returns the status of the DS3232 INT#/SQW.
	
	This pin is active low. The DS3232 seconds counter is incremented at the
	same time as high to low transitions.
	
	This pin is connected to PA6 in all hardware versions.
	
	Parameters:

	Returns:
		Status of the INT#/SQW pin (1 or 0)
******************************************************************************/
/*unsigned char system_getrtcint(void)
{
	return (PINA>>6)&1;
}*/
/******************************************************************************
	function: system_settimefromrtc
*******************************************************************************	
	Sets the system time returned by timer_ms_get and timer_us_get from the RTC.
	
	
	The epoch for millisecond time is midnight of the first day of the month.
	The epoch for the microsecond time is the current hour.
	
	
	Parameters:

	Returns:

******************************************************************************/
void system_settimefromrtc(void)
{
	unsigned char h,m,s;		// hour, minutes, seconds
	unsigned char wd,day,month,year;
	unsigned long t1;
	
	fprintf(file_pri,"Setting time from RTC... ");
	
	stmrtc_readdatetime_conv_int(1,&h,&m,&s,&wd,&day,&month,&year);
	//ds3232_readdatetime_conv_int(1,&h,&m,&s,&day,&month,&year);
	unsigned long epoch_s = ((day-1)*24l+h)*3600l+m*60l+s;
	unsigned long epoch_s_frommidnight = h*3600l+m*60l+s;
	unsigned long epoch_us = (m*60l+s)*1000l*1000l;
	timer_init(epoch_s,epoch_s_frommidnight,epoch_us);
	t1=timer_ms_get();
	fprintf(file_pri,"done: %02d.%02d.%02d %02d:%02d:%02d = %lu ms\n",day,month,year,h,m,s,t1);

}
/******************************************************************************
	function: system_storepoweroffdata
*******************************************************************************	
	Store the current power/voltate/date/time to EEPROM. This is used before 
	powering off the system to compute the power used in off mode.
	
	Interrupts:
		Not suitable for use in interrupts.
	
	
	Parameters:

	Returns:

******************************************************************************/
/*void system_storepoweroffdata(void)
{
	// Read time synchronously
	unsigned char h,m,s,day,month,year;
	ds3232_readdatetime_conv_int(1,&h,&m,&s,&day,&month,&year);
	// Set the charge counter at mid-point
	ltc2942_setchargectr(32768);	// Set charge counter at mid-point
	// Read the voltage and charge	
	unsigned long charge = ltc2942_getcharge();	
	unsigned long chargectr = ltc2942_getchargectr();
	unsigned short voltage = ltc2942_getvoltage();
	
	printf("charge: %lu chargectr: %lu voltage: %u\n",charge,chargectr,voltage);
	
	// Store charge, voltage and time
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_VALID,1);
	eeprom_write_dword((uint32_t*)STATUS_ADDR_OFFCURRENT_CHARGE0,charge);
	eeprom_write_word((uint16_t*)STATUS_ADDR_OFFCURRENT_VOLTAGE0,voltage);
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_H,h);
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_M,m);
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_S,s);
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_DAY,day);
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_MONTH,month);
	eeprom_write_byte((uint8_t*)STATUS_ADDR_OFFCURRENT_YEAR,year);	
}*/
/******************************************************************************
	function: system_storepowerdata
*******************************************************************************
	Store the current power/voltate/date/time to EEPROM.
	Assumes that the data has been previously read from the fuel gauge.

	This is used before powering off the system to compute the power used in off mode.

	Interrupts:
		Suitable for interrupts, but does not guarantee success of EEPROM write.

	Parameters:
		addr		-		Address where to write: typically STATUS_ADDR_OFFCURRENT_VALID or STATUS_ADDR_ONCURRENT_VALID

	Returns:

******************************************************************************/
void system_storepowerdata()
{
	// Get voltage and charge
	unsigned long charge = ltc2942_last_charge();	
	unsigned short voltage = ltc2942_last_mV();
	unsigned long time = ltc2942_last_updatetime();
	
	// Store charge, voltage and time

	//fprintf(file_pri,"last up ti: %ld\n",time);	// debug


	// The eeprom_write_xxx functions are not suitable for use in interrupts.
	// Therefore use eeprom_write_buffer_try_nowait instead.
	unsigned char buffer[13];
	buffer[0] = 1;		// Valid
	*(unsigned long*)(&buffer[1]) = charge;
	*(unsigned short*)(&buffer[5]) = voltage;
	*(unsigned long*)(&buffer[7]) = time;
	buffer[11] = 0x55;		// Termination
	buffer[12] = 0xAA;

	unsigned char rv = eeprom_write_buffer_try_nowait(STATUS_ADDR_OFFCURRENT_VALID,buffer,13);

	//eeprom_write_buffer_try_nowait(STATUS_ADDR_OFFCURRENT_VALID,"0123456789ABCD",13);

}

void system_loadpowerdata(_POWERUSE_DATA *pu)
{
	// Load charge, voltage and time
	pu->valid = eeprom_read_byte(STATUS_ADDR_OFFCURRENT_VALID,1,0);
	pu->charge = eeprom_read_dword(STATUS_ADDR_OFFCURRENT_CHARGE0,1,0);
	pu->voltage = eeprom_read_word(STATUS_ADDR_OFFCURRENT_VOLTAGE0,1,0);
	pu->time = eeprom_read_dword(STATUS_ADDR_OFFCURRENT_TIME,1,0);


	
	
}



/******************************************************************************
	system_getbattery
*******************************************************************************	
	Returns the battery voltage in mV. 
	The battery voltage is sampled in the background through a timer callback.
******************************************************************************/
unsigned short system_getbattery(void)
{
	
	return ltc2942_last_mV();
}
/******************************************************************************
	system_getbatterytime
*******************************************************************************	
	Returns the battery voltage in mV. 
	The battery voltage is sampled in the background through a timer callback.
	
******************************************************************************/
/*unsigned long system_getbatterytime(void)
{
	unsigned long t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t = system_battery_updatetime;	
	}
	return t;
}
*/

/*void system_adcpu_off(void)
{
#if (HWVER==7) || (HWVER==9)
	// 5 ADC are set as input, with pull-up off
	DDRA &= 0b01110000;							// 5 extension ADC as input
	PORTA = init_porta&0b01110000;				// disable pull-up on 4 ADC input
#else
	DDRA &= 0xf0;								// 4 extension ADC as input
	PORTA = init_porta&0xf0;					// disable pull-up on 4 ADC input
#endif
}
void system_adcpu_on(void)
{
	// No need to test HW version - porta goes to default settings which is pull-up.
	//DDRA &= 0xf0;									// 4 extension ADC as input
	PORTA = init_porta;								// enable pull-up on 4 ADC input
}*/

/******************************************************************************
	function: system_batterystat
*******************************************************************************
	Uses the red LED (led0) to indicate battery level and whether the power-button 
	is pressed.
	
	If the power-button is pressed, the RED led turns on for the first 4 seconds, then 
	starts blinking until power is cut.
	
	Also handles storing battery state during power off.
	If a long-press on the power button occurs:
	- at 4 seconds, issues a background read of battery state.
	- at 4.5 seconds, save battery state.
	
	Must be called from a timer callback at 10Hz.
	
	If the power button is pressed, turn on the red LED. 
	If the power button is not pressed, then every 4 seconds:
	- T=0...5: 	either does not blink (battery on the full side)
	- 			blinks 1, 2 or 3 times to indicate how low the battery is.

	Handles SD card detection notification
	
	Requires the ltc2942_last_mV function to return the battery voltage.
	
	
	Parameters:
		unused	-	Unused parameter passed by the timer callback

	Returns:
		0 (unused)
******************************************************************************/
unsigned char system_batterystat(unsigned char unused)
{
	(void) unused;

	static unsigned char counter=0;		// Counter counts from 0 to 39 tenth of seconds and wraps around (4 second period).
	static unsigned char nblinks;
	static unsigned char lastpc=0;
	unsigned char newpc;
	static unsigned char pressduration=0;
	static unsigned char last_sddetect=0;
	unsigned char new_sddetect=0;
	static unsigned char do_blink_sd=0;
	

	// Run interface change detection on the same interrupt
	interface_change_detect_periodic();

	// Run SD card detection
	new_sddetect = system_issdpresent();
	if(new_sddetect^last_sddetect)
	{
		system_led_on(LED_RED);
		last_sddetect = new_sddetect;
		do_blink_sd=1;
		ufat_notifysdcardchange(new_sddetect);
	}
	else
	{
		if(do_blink_sd==1)
		{
			system_led_off(LED_RED);
			do_blink_sd=0;
		}
	}


	newpc = system_buttonpress();
	// Check if pin has changed state and update LED accordingly
	if( (newpc^lastpc))
	{
		// HACK
		//system_buttonchange(newpc);

		// pin has changed
		pressduration=0;			// Reset the press duration
		// If push button is pressed, turn on the LED, otherwise off.
		if(newpc==1)
		{
			system_led_on(0);
		}
		else
		{
			system_led_off(0);

			system_led_off(LED_GREEN);		// Power debug
		}
		
		lastpc=newpc;
		counter=9;	// Reset the counter to somewhere after the blinks
	}
	// If the push button is pressed, do not do the battery display, but increase the press duration counter
	if(newpc==1)
	{
		pressduration++;
		if(pressduration==40)
		{
			// Issue a background read of the battery state
			ltc2942_backgroundgetstate(0);
		}
		if(pressduration==44)
		{
			// Store the battery info
			system_storepowerdata(STATUS_ADDR_OFFCURRENT_VALID);
			
			//system_led_on(LED_GREEN);	// DEBUG: Turn on green LED once power data stored

		}
		if(pressduration>40 && pressduration<45)		// Blink if greater than 4 seconds then stay on
		//if(pressduration>40)							// Blink forever if greater than 4 seconds
		{
			system_led_toggle(0b001);
		}
		return 0;
	}
	
	if(counter==0)
	{
		// If counter is zero, we initialise the blink logic.
		// nblinks indicate the number of blinks to issue.
		nblinks=0;
		//unsigned short mv = ltc2942_last_mV();
		unsigned short mv = 4200;
		
		
		nblinks=0;
		if(mv<BATTERY_LOW)
			nblinks++;
		if(mv<BATTERY_VERYLOW)
			nblinks++;
		if(mv<BATTERY_VERYVERYLOW)
			nblinks++;
			
		// Must blink nblinks time.		
	}
	// When nblinks is nonzero, the led is turned on when the lsb is 0, and turned off when the lsb is 1.
	if(nblinks)
	{
		if( (counter&1) == 0)
		{
			system_led_on(0);
		}
		else
		{
			system_led_off(0);
			nblinks--;
		}
	}
	
	counter++;
	if(counter>39)
		counter=0;
	return 0;
}

unsigned char *system_getdevicename(void)
{
	return system_devname;
}
/*#if HWVER==1
void system_off(void)
{
}
#endif
#if HWVER==4
void system_off(void)
{
	// Clear PC7
	PORTC&=0b01111111;
}
#endif
#if HWVER==5
void system_off(void)
{
	// Set PC7
	PORTC|=0b10000000;
}
#endif
#if (HWVER==6) || (HWVER==7) || (HWVER==9)
void system_off(void)
{
	// Clear PC7
	PORTC&=0b01111111;
}
#endif

*/

/******************************************************************************
	system_gettemperature
*******************************************************************************	
	Returns the temperature from the DS332 in hundredth of degree.
	The temperature is sampled in the background through a timer callback.
	
******************************************************************************/
/*signed short system_gettemperature(void)
{
	signed short t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t = system_temperature;	
	}
	return t;
}*/
/******************************************************************************
	system_gettemperaturetime
*******************************************************************************	
	Returns the temperature from the DS332 in hundredth of degree.
	The temperature is sampled in the background through a timer callback.	
******************************************************************************/
/*unsigned long system_gettemperaturetime(void)
{
	unsigned long t;
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		t = system_temperature_time;	
	}
	return t;
}

unsigned char system_sampletemperature(unsigned char p)
{
	ds3232_readtemp_int_cb(system_sampletemperature_cb);
	return 0;
}

void system_sampletemperature_cb(unsigned char s,signed short t)
{
	system_temperature = t;
	system_temperature_time = timer_ms_get();
}
*/

/******************************************************************************
	function: system_perfbench
*******************************************************************************	
	Benchmarks the processor, here using simple counting. Use to assess
	CPU load during sampling.
	
	Parameters:
		mintime		-		Minimum duration of the benchmark in ms. 
							If 0 (default if no parameters) does a 1000ms test.
	
	Return value:	performance result
******************************************************************************/
unsigned long system_perfbench(unsigned long mintime)
{
	unsigned long int t_last,t_cur;
	unsigned long int ctr,cps;

	ctr=0;

	t_last=timer_s_wait();
	unsigned long tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		ctr++;
	}
	unsigned long tint2=timer_ms_get_intclk();
	cps = ctr/(t_cur-t_last);
	(void)tint1; (void) tint2;

	//fprintf(file_pri,"main_perfbench: %lu perf (%lu intclk ms)\n",cps,tint2-tint1);

	return cps;
}

//extern USBD_HandleTypeDef hUsbDeviceFS;
void system_buttonchange(unsigned char newpc)
{
	static int toggle=0;

	//fprintf(file_pri,"button change: %d\n",newpc);

	// Only do stuff if the current state is pressed
	if(newpc==0)
		return;


	toggle=1-toggle;


	//fprintf(file_pri,"toggle: %d\n",toggle);

	if(toggle==1)
	{
		itmprintf("enable periph\n");
		//fprintf(file_pri,"enable usb\n");
		system_led_on(LED_GREEN);

		system_periphvcc_enable();

		// Initialise the USB
		//usb_must_enable=1;
		//MX_USB_DEVICE_Init();

//		HAL_PCD_DevDisconnect(

		//MX_USB_DEVICE_Init();
		// Init Device Library, add supported class and start the library.
		/*if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
		{
		//Error_Handler();
		}
		if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
		{
		//Error_Handler();
		}
		if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
		{
		//Error_Handler();
		}
		if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
		{
		//Error_Handler();
		}*/
	}
	else
	{
		//itmprintf("disable usb\n");
		itmprintf("disable periph\n");
		//fprintf(file_pri,"disable usb\n");
		system_led_off(LED_GREEN);

		system_periphvcc_disable();

		//HAL_PCD_DevConnect

		//USBD_DeInit(&hUsbDeviceFS);
	}

}
unsigned char system_issdpresent()
{
	return HAL_GPIO_ReadPin(SD_DETECT_GPIO_Port, SD_DETECT_Pin)==GPIO_PIN_RESET ? 1:0;
}

void system_info_cpu()
{
	fprintf(file_pri,"sizeof(long long): %d\n",sizeof(long long));
	fprintf(file_pri,"sizeof(long): %d\n",sizeof(long));
	fprintf(file_pri,"sizeof(int): %d\n",sizeof(int));
	fprintf(file_pri,"sizeof(short): %d\n",sizeof(short));
	fprintf(file_pri,"sizeof(char): %d\n",sizeof(char));
	unsigned short d = 0x1234;
	fprintf(file_pri,"Byte 0: %02X\n",(unsigned)*((unsigned char*)&d));
	if(*((unsigned char*)&d) == 0x12)
		fprintf(file_pri,"BIG\n");
	else
		fprintf(file_pri,"LITTLE\n");
}


