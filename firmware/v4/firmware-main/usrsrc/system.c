/*
 * system.c
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

#include "main.h"
#include "system.h"

#define _delay_ms HAL_Delay

void system_delay_ms(unsigned short t)
{
	//#if BOOTLOADER==1
	//	unsigned long int tstart=timer_ms_get();
	//	while(timer_ms_get()-tstart<(unsigned long int)t);
	//#else
		_delay_ms(t);
	//#endif
}

void system_motionvcc_enable()
{
	// Hardware v4 does not have motionvcc enable pin -> dummy function for compatibility.
}
void system_motionvcc_disable()
{
	// Hardware v4 does not have motionvcc enable pin -> dummy function for compatibility.
}
void system_poweroff()
{
	// Hardware v4: turn off power by clearing PWR_ON pin
	HAL_GPIO_WritePin(PWR_ON_GPIO_Port, PWR_ON_Pin, GPIO_PIN_RESET);
}

void system_periphvcc_enable()
{
	// Turn on power switch
	HAL_GPIO_WritePin(PERIPH_EN_GPIO_Port, PERIPH_EN_Pin, GPIO_PIN_SET);
	//HAL_Delay(200);	// Wait power ok
	HAL_Delay(20);	// Wait power ok
}
void system_periphvcc_disable()
{
	// Turn off power switch
	HAL_GPIO_WritePin(PERIPH_EN_GPIO_Port, PERIPH_EN_Pin, GPIO_PIN_RESET);

#warning "deactivate UART RX, TX, CTS, RTS, BLUE_RST and BLUE_CONNECT"

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	// BLUE_Connect: PB5
	GPIO_InitStruct.Pin = BLUE_Connect_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BLUE_Connect_GPIO_Port, &GPIO_InitStruct);

	// BLUE_RST#: PB4
	GPIO_InitStruct.Pin = BLUE_RST__Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BLUE_RST__GPIO_Port, &GPIO_InitStruct);

	// CTS, RTS, TX, RX: PA0-PA3
	GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

#warning "Check if deactivate SD card"

}

void system_periph_setrn41resetpin(int set)
{
	if(set)
		HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_SET);
	else
		HAL_GPIO_WritePin(BLUE_RST__GPIO_Port, BLUE_RST__Pin, GPIO_PIN_RESET);
}

// Three LED version
void system_led_test(void)
{
	//for(unsigned char i=0;i<4;i++)
	{
		unsigned char v=0b100;
		for(unsigned char j=0;j<3;j++)
		{
			system_led_set(v);
			v>>=1;
			_delay_ms(50);
		}
		_delay_ms(50);
		v=1;
		for(unsigned char j=0;j<3;j++)
		{

			system_led_set(v);
			v<<=1;
			_delay_ms(50);
		}
		_delay_ms(50);
	}
}


// ledn: led number
// LED #0: Red
// LED #1: Yellow
// LED #2: Green
void system_led_on(unsigned char ledn)
{
	if(ledn==LED_GREEN)
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
	if(ledn==LED_YELLOW)
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
	if(ledn==LED_RED)
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
}
void system_led_off(unsigned char ledn)
{
	if(ledn==LED_GREEN)
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
	if(ledn==LED_YELLOW)
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
	if(ledn==LED_RED)
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
}
// led: bitmap corresponding to GYR
void system_led_toggle(unsigned char led)
{
	if(led&(1<<LED_GREEN))
		HAL_GPIO_TogglePin(LED_2_GPIO_Port, LED_2_Pin);
	if(led&(1<<LED_YELLOW))
		HAL_GPIO_TogglePin(LED_1_GPIO_Port, LED_1_Pin);
	if(led&(1<<LED_RED))
		HAL_GPIO_TogglePin(LED_0_GPIO_Port, LED_0_Pin);

}
// led: bitmap corresponding to GYR
void system_led_set(unsigned char led)
{
	if(led&(1<<LED_GREEN))
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_RESET);
	else
		HAL_GPIO_WritePin(LED_2_GPIO_Port, LED_2_Pin, GPIO_PIN_SET);
	if(led&(1<<LED_YELLOW))
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_RESET);
	else
		HAL_GPIO_WritePin(LED_1_GPIO_Port, LED_1_Pin, GPIO_PIN_SET);
	if(led&(1<<LED_RED))
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_RESET);
	else
		HAL_GPIO_WritePin(LED_0_GPIO_Port, LED_0_Pin, GPIO_PIN_SET);
}


/******************************************************************************
	function: system_lifesign
*******************************************************************************
	Blinks an LED to indicate aliveness.

	Called from a timer callback.

	Parameters:
		sec		-	Number of seconds elapsed since the epoch

	Returns:
		0 (unused)
******************************************************************************/
unsigned char system_lifesign(unsigned char sec)
{
	//
	if((sec&0b11)==00)
		system_led_on(1);
	else
		system_led_off(1);
	return 0;
}
unsigned char system_lifesign2(unsigned char sec)
{
	system_led_toggle(0b10);
	return 0;
}


void system_status_ok(unsigned char status)
{
	system_blink_led(status,150,150,1);
}
void system_blink(unsigned char n,unsigned char delay,unsigned char init)
{
	system_led_set(init);
	for(unsigned char i=0;i<n;i++)
	{
		system_delay_ms(delay);
		system_led_toggle(0b11);
	}
}
void system_blink_led(unsigned char n,unsigned char timeon,unsigned char timeoff,unsigned char led)
{
	system_led_off(led);
	system_delay_ms(timeoff*4);
	for(unsigned char i=0;i<n;i++)
	{
		system_led_on(led);
		system_delay_ms(timeon);
		system_led_off(led);
		system_delay_ms(timeoff);
	}
	system_delay_ms(timeoff*3);
}

unsigned system_buttonpress()
{
	return HAL_GPIO_ReadPin(PWR_PBSTAT_GPIO_Port,PWR_PBSTAT_Pin)?0:1;
}


unsigned char system_isbtconnected(void)
{
	return HAL_GPIO_ReadPin(BLUE_Connect_GPIO_Port,BLUE_Connect_Pin)?1:0;
}
unsigned char system_isusbconnected(void)
{
	return HAL_GPIO_ReadPin(USB_SENSE_GPIO_Port,USB_SENSE_Pin)?1:0;
}

void system_sleep()
{
	__WFI();
}
