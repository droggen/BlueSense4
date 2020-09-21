/*
 * stmadc.c
 *
 *  Created on: 12 janv. 2020
 *      Author: droggen
 */
#include <string.h>
#include <stdio.h>
#include "adc.h"
#include "atomicop.h"
#include "usrmain.h"
#include "global.h"
#include "stmadc.h"
#include "wait.h"
#include "stm32l4xx_ll_tim.h"
#include "serial_itm.h"

unsigned char _stm_adc_buffer_rdptr,_stm_adc_buffer_wrptr;

//
#define DMABUFSIZ STM_ADC_MAXCHANNEL*STM_ADC_MAXGROUPING*2						// We sample up to STM_ADC_MAXGROUPING times, before triggering an interrupt

unsigned short _stm_adc_dmabuf[DMABUFSIZ];										// DMA tempory buffer - DMA uses double buffering so buffer twice the size of the payload (maxchannel)

unsigned short _stm_adc_buffers[STM_ADC_BUFFER_NUM][STM_ADC_MAXCHANNEL];		// ADC buffer: filled by DMA/interrupts; emptied by user application. Buffer size: STM_ADC_BUFFER_NUM entries
unsigned long _stm_adc_buffers_time[STM_ADC_BUFFER_NUM];						// ADC buffer acquisition time
unsigned long _stm_adc_buffers_pktnum[STM_ADC_BUFFER_NUM];						// ADC buffer packet counter
unsigned _stm_adc_grouping;														// ADC performs grouping conversions before triggering an interrupt
unsigned long _stm_adc_ctr_acq,_stm_adc_ctr_loss;								// DMA acquisition statistics

// pins and ports hold the pin and port of each BS4 ADC input.
uint32_t _stm_adc_pins[5] = 		{X_0_ADC0_Pin,			X_1_ADC1_Pin,		X_2_ADC2_Pin,		X_3_ADC3_Pin,		X_4_ADC_DAC_Pin};
GPIO_TypeDef  *_stm_adc_ports[5]=	{X_0_ADC0_GPIO_Port,	X_1_ADC1_GPIO_Port,	X_2_ADC2_GPIO_Port,	X_3_ADC3_GPIO_Port,	X_4_ADC_DAC_GPIO_Port};
unsigned _stm_adc_channels;														// Holds the currenly select channels for conversions

/*
DMA must be configured as circular; DMA transfer must be twice the size of the payload to use double buffering.
Normal mode DMA stops (and doesn't respond to triggers) once the payload is transfered.


Behaviours:
	DMA circular & transfer 2x data: 		== works as expected for double buffering
		HAL_ADC_ConvHalfCpltCallback 1
		04E9 0432 0A32 06DF 069D 0643 0620 03BF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF
		t 4: 997
		HAL_ADC_ConvCpltCallback: dr 959 v 959. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 04A6 0429 0A38 06D7 0657 0641 061F 03BF 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 5: 992
		HAL_ADC_ConvHalfCpltCallback 2
		0453 03EF 0A0C 06AC 0626 0637 061F 03BF AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		t 6: 997
		HAL_ADC_ConvCpltCallback: dr 959 v 959. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 040C 03BF 09E6 068B 060B 062F 061F 03BF 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 7: 1020
		HAL_ADC_ConvHalfCpltCallback 3
		0402 03C6 09E7 0691 0617 0635 0621 03BF AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		usb_rcv_fs: 3

	DMA circular & transfer 3x data		== half callback after 2x data, full callback later - anyways weird
		HAL_ADC_ConvHalfCpltCallback 1
		04E9 0442 0A03 06DB 069A 0643 0620 03C2 04C1 045A 0A24 06EB 066F 0644 0620 03BF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF
		t 5: 984
		HAL_ADC_ConvCpltCallback: dr 960 v 960. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 0467 0429 09F6 06C1 0647 063C 0622 03C0 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 6: 992
		t 7: 1001
		HAL_ADC_ConvHalfCpltCallback 2
		03FD 03E7 09C4 0695 061C 0638 0621 03C2 03DB 03CE 09AD 0685 0615 0634 0621 03C1 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		t 8: 1028
		HAL_ADC_ConvCpltCallback: dr 961 v 961. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 0425 03FB 09D2 06A7 062B 0638 0620 03C1 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 9: 992
		t 10: 1001
		HAL_ADC_ConvHalfCpltCallback 3
		048F 0445 0A0C 06DD 0655 0637 0620 03C1 04C6 045F 0A25 06EF 0667 063D 0620 03C0 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		t 11: 984
		HAL_ADC_ConvCpltCallback: dr 962 v 962. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 046E 0424 09F0 06C1 063B 063F 0622 03C2 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 12: 994
		t 13: 1036
		HAL_ADC_ConvHalfCpltCallback 4
		040D 03DE 09C0 0695 0619 0636 0620 03C2 03DE 03D3 09AC 0686 060E 0639 0622 03C1 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		t 14: 983
		HAL_ADC_ConvCpltCallback: dr 962 v 962. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 0428 03F9 09D3 06AB 0633 0634 0620 03C2 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		usb_rcv_fs: 3

	DMA circular & transfer 4x data		== half callback after 2x data, full callback 4x data - strange as 4 timer interrupt initially
		usb_rcv_fs: 3
		t 1: 1282712286
		t 2: 0
		t 3: 1001
		t 4: 1001
		HAL_ADC_ConvHalfCpltCallback 1
		0467 03F7 09CC 06A8 0667 0642 0620 03C2 049F 042D 09FA 06CA 0657 063F 0620 03C3 FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF FFFF
		t 5: 983
		t 6: 1001
		HAL_ADC_ConvCpltCallback: dr 963 v 963. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 04DF 0466 0A27 06F5 0671 0641 0620 03C5 04B2 0447 0A0F 06E3 0655 0640 0621 03C3 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 7: 1021
		t 8: 1001
		HAL_ADC_ConvHalfCpltCallback 2
		0421 03F4 09D8 06A7 0631 063A 0620 03C4 040F 03DA 09B6 0696 0617 0638 0621 03C2 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		t 9: 984
		t 10: 1001
		HAL_ADC_ConvCpltCallback: dr 963 v 963. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 040E 03E5 09CC 069F 0629 063D 0621 03C3 0489 0437 0A08 06D9 0650 0638 0621 03C3 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 11: 983
		t 12: 1037
		HAL_ADC_ConvHalfCpltCallback 3
		04C7 045B 0A29 06F0 065D 0648 0620 03C4 047A 0439 0A00 06D5 064A 0641 0622 03C2 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		t 13: 983
		t 14: 1001
		HAL_ADC_ConvCpltCallback: dr 963 v 963. eoc: 0 eos: 1
		5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 0415 03EA 09D0 06A3 0628 0638 061F 03C3 03EA 03D3 09B2 068D 0618 0638 0621 03C3 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555 5555
		t 15: 993
		t 16: 1002
		HAL_ADC_ConvHalfCpltCallback 4
		0421 03F3 09D3 06A9 0628 0633 0620 03C3 04A8 0442 0A0B 06E0 064E 0644 0620 03C4 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
		usb_rcv_fs: 3



DMA normal



*/
/******************************************************************************
	function: stm_adc_acquire_start_us
*******************************************************************************
	Start data acquisition of specified channels.
	Specify the sample rate in uS.

	Setups GPIO as analog ADC.

	Parameters:
		channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

	Returns:
		0		-		Ok
		1		-		Error
******************************************************************************/
void stm_adc_acquire_start_us(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,unsigned periodus)
{
	// Calculate the prescaler and divider
	unsigned prescaler;

	// Prescaler: maps from TIM frequency to 10uS period (10uS <> 100KHz).

	int timclk = _stm_adc_gettimfrq();

	prescaler = (timclk/100000)-1;			// Subract one as frequency is divided by prescaler+1

	int divider;
	// Calculates divider, rouding to lower values;
	divider = (periodus/10)-1;				// Subtract one as frequency is divided by divider+1
	if(divider<0) divider=0;				// Minimum divider is 0 (10uS sample period)

	_stm_adc_acquire_start(channels,vbat,vref,temp,prescaler,divider);


}
/******************************************************************************
function: _stm_adc_acquire_start
*******************************************************************************
Start data acquisition of specified channels.
Specify the sample rate through dividers.

Setups GPIO as analog ADC.

Parameters:
	channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

Returns:
	0		-		Ok
	1		-		Error
******************************************************************************/
void _stm_adc_acquire_start(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp,unsigned prescaler,unsigned reload)
{
	unsigned grouping;
	unsigned maxips = 100;


	// Calculate the grouping of ADC conversions: interrupt triggered every _stm_adc_grouping to minimise interrupt overhead
	// Find grouping which allows for IPS as low as, but higher or equal than maxips (e.g. 100Hz)

	fprintf(file_pri,"Channels: %u vbat: %u vref: %u temp: %u Prescaler: %u reload: %u\n",channels,vbat,vref,temp,prescaler,reload);


	unsigned timerclock=_stm_adc_gettimfrq();
	unsigned ips = ips = timerclock/(prescaler+1)/(reload+1);	// Number of interrupt per seconds assuming one interrupt per sample

	fprintf(file_pri,"ADC interrupt/second prior to grouping: %u\n",ips);


	grouping = ips/maxips;
	if(grouping<1)
		grouping=1;
	if(grouping>STM_ADC_MAXGROUPING)
		grouping=STM_ADC_MAXGROUPING;

	ips = ips/grouping;



	fprintf(file_pri,"ADC interrupts/second: %u (groups of %u)\n",ips,grouping);


	//grouping = 1;
	//grouping = STM_ADC_MAXGROUPING;


	// Initialise the ADC with all the channels for timing reasons
	stm_adc_init(channels,vbat,vref,temp);

	// Initialise the timer at desired repeat rate
	_stm_adc_init_timer(prescaler,reload);

	// Start data acquisition
	stm_adc_acquirecontinuously(grouping);

	// Clear buffer and statistics
	stm_adc_data_clear();
}

void stm_adc_acquire_stop()
{
	HAL_ADC_Stop_DMA(&hadc1);
	// Stop timer
	_stm_adc_deinit_timer();
	// Deinit adc
	stm_adc_deinit();
}

unsigned _stm_adc_gettimfrq()
{
	unsigned timclk;
	unsigned pclk = HAL_RCC_GetPCLK2Freq();
	// Get the APB2 timer clock frequency: it is equal to APB2 only if the APB2 prescaler is 1, otherwise it is double.
	// Get PCLK2 prescaler
	if((RCC->CFGR & RCC_CFGR_PPRE2) == 0)
	{
		// PCLK2 prescaler equal to 1 => TIMCLK = PCLK2
		timclk = pclk;
	}
	else
	{
		// PCLK prescaler different from 1 => TIMCLK = 2 * PCLK
		timclk = 2*pclk;
	}
	fprintf(file_pri,"APB2 peripheral clock: %u APB2 timer clock: %u\n",pclk,timclk);
	return timclk;
}

/******************************************************************************
	function: stm_adc_init
*******************************************************************************
	Initialise ADC for scan conversion of several channels

	Setups GPIO as analog ADC.

	Parameters:
		channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

	Returns:
		0		-		Ok
		1		-		Error
******************************************************************************/
unsigned char stm_adc_init(unsigned char channels,unsigned char vbat,unsigned char vref,unsigned char temp)
{
	ADC_MultiModeTypeDef multimode = {0};
	ADC_ChannelConfTypeDef sConfig = {0};
	HAL_StatusTypeDef s;



	ADC_TypeDef *adc = hadc1.Instance;

	const unsigned bs2maxchannel = 8;

	// Mapping of bluesense channels to STM channels
	uint32_t bs2stmmap[8] = 	{ADC_CHANNEL_1, 		ADC_CHANNEL_2, 		ADC_CHANNEL_4, 		ADC_CHANNEL_13, 	ADC_CHANNEL_9, 		ADC_CHANNEL_VBAT, 	ADC_CHANNEL_VREFINT, 	ADC_CHANNEL_TEMPSENSOR};
	uint32_t bs2stmrank[8] = 	{ADC_REGULAR_RANK_1,	ADC_REGULAR_RANK_2,	ADC_REGULAR_RANK_3,	ADC_REGULAR_RANK_4,	ADC_REGULAR_RANK_5,	ADC_REGULAR_RANK_6,	ADC_REGULAR_RANK_7,		ADC_REGULAR_RANK_8};
	/*uint32_t *sqr[bs2maxchannel] = 		{ADC1_BASE+0x30,ADC1_BASE+0x30,ADC1_BASE+0x30,ADC1_BASE+0x30,
							ADC1_BASE+0x34,ADC1_BASE+0x34,ADC1_BASE+0x34,ADC1_BASE+0x34};
	unsigned sqroff[bs2maxchannel]=		{6,12,18,24,0,6,12,18};*/


	for(int i=0;i<bs2maxchannel;i++)
	{
		fprintf(file_pri,"Channel %d: %08X\n",i,bs2stmmap[i]);
	}

	channels&=0b11111;			// BlueSense has 5 external ADC inputs
	// Append the additional channels: vbat, vref, temp in this order
	if(vbat) channels|=0b100000;
	if(vref) channels|=0b1000000;
	if(temp) channels|=0b10000000;

	_stm_adc_channels = channels;		// Save this for future de-initialisation




	int nconv=0;
	for(int i=0;i<bs2maxchannel;i++)
	{
		if(_stm_adc_channels&(1<<i))
			nconv++;
	}

	fprintf(file_pri,"Channels: %X. Number of conversions in a scan: %u\n",_stm_adc_channels,nconv);


	// Initialise the GPIO in analog mode
	stm_adc_init_gpiotoadc(_stm_adc_channels);


	// Deinit


	//ADC_ConversionStop(&hadc1,ADC_REGULAR_INJECTED_GROUP);
	//ADC_Disable(&hadc1);
	//HAL_ADC_DeInit(&hadc1);

	// Common configuration
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
	hadc1.Init.LowPowerAutoWait = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.NbrOfConversion = nconv;
	hadc1.Init.DiscontinuousConvMode = DISABLE;

	//hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	//hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;

	hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T15_TRGO;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;


	//hadc1.Init.DMAContinuousRequests = DISABLE;
	hadc1.Init.DMAContinuousRequests = ENABLE;
	hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
	hadc1.Init.OversamplingMode = DISABLE;
	s = HAL_ADC_Init(&hadc1);
	if(s)
	{
		fprintf(file_pri,"Error initialising ADC\n");
		return 1;
	}
	/** Configure the ADC multi-mode
	*/
	multimode.Mode = ADC_MODE_INDEPENDENT;
	if(HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
	{
		fprintf(file_pri,"Error initialising ADC multimode\n");
		return 1;
	}


	// Config all the chnanels: hack to reset the sequence
	/*hadc1.Instance->SQR1=0x00;
	hadc1.Instance->SQR2=0x00;
	hadc1.Instance->SQR3=0x00;
	hadc1.Instance->SQR4=0x00;*/

	// Config all the chnanels
	int rankidx = 0;
	for(int i=0;i<bs2maxchannel;i++)
	{
		if( !(channels&(1<<i)) )
			continue;
		sConfig.Channel = bs2stmmap[i];
		sConfig.Rank = bs2stmrank[rankidx];
		// sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;			// Highest
		//sConfig.SamplingTime = ADC_SAMPLETIME_92CYCLES_5;
		sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;			// AHB=32MHz; ADC clock = 8MHz; sample time: 8MHz/247 = 32KHz
		//sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
		sConfig.SingleDiff = ADC_SINGLE_ENDED;
		sConfig.OffsetNumber = ADC_OFFSET_NONE;
		sConfig.Offset = 0;
		if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
		{
			fprintf(file_pri,"Error initialising ADC channel %d\n",i);
			return 1;
		}

		rankidx++;
	}




	return 0;
}
void stm_adc_deinit()
{
	// Denitialise the GPIO to analog mode
	stm_adc_deinit_gpiotoanalog(_stm_adc_channels);
#warning check if must deinitialise internal channels
}

/******************************************************************************
	function: stm_adc_init_gpiotoadc
*******************************************************************************
	Initialise the BS2 ADC pins to ADC mode.


	Parameters:
		channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

	Returns:
		-
******************************************************************************/
void stm_adc_init_gpiotoadc(unsigned char channels)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Iterate all the channels
	for(int i=0;i<5;i++)
	{
		if(channels&(1<<i))
		{
			// Channel is active -> set as analog input
			GPIO_InitStruct.Pin = _stm_adc_pins[i];
			GPIO_InitStruct.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
			GPIO_InitStruct.Pull = GPIO_NOPULL;
			HAL_GPIO_Init(_stm_adc_ports[i], &GPIO_InitStruct);
		}
	}
}
/******************************************************************************
	function: stm_adc_deinit_gpiotoanalog
*******************************************************************************
	Deinitialise the BS2 ADC pins back to analog mode.


	Parameters:
		channels	-	Bitmask of BlueSense channels: bit 0=X0_ADC0; bit 4 = X4_ADC4

	Returns:
		-
******************************************************************************/
void stm_adc_deinit_gpiotoanalog(unsigned char channels)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// Iterate all the channels
	for(int i=0;i<5;i++)
	{
		if(channels&(1<<i))
		{
			// Channel is active -> reset back to analog mode
			GPIO_InitStruct.Pin = _stm_adc_pins[i];
			GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
			GPIO_InitStruct.Pull = GPIO_NOPULL;
			HAL_GPIO_Init(_stm_adc_ports[i], &GPIO_InitStruct);
		}
	}
}


/******************************************************************************
	function: stm_adc_isdata
*******************************************************************************
	Checks if a read buffer is available to be read.

	Parameters:

	Returns:
		0		-		No read buffer filled yet
		1		-		A read buffer is available to be read
******************************************************************************/
unsigned char stm_adc_isdata()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( _stm_adc_buffer_wrptr != _stm_adc_buffer_rdptr )
			return 1;
	}
	return 0;	// To prevent compiler from complaining
}
/******************************************************************************
	function: stm_adc_isfull
*******************************************************************************
	Returns 1 if the buffer is full, 0 otherwise.
*******************************************************************************/
unsigned char stm_adc_isfull(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		if( ((_stm_adc_buffer_wrptr+1)&STM_ADC_BUFFER_MASK) == _stm_adc_buffer_rdptr )
			return 1;
		return 0;
	}
	return 0;	// To prevent compiler from complaining
}
/******************************************************************************
	function: stm_adc_data_getnext
*******************************************************************************
	Returns the next data in the buffer, if data is available.

	This function removes the data from the read buffer and the next call
	to this function will return the next available data.

	If no data is available, the function returns an error.

	The data is copied from the internal ADC buffers into the user provided
	buffer.

	The number of ADC sample copied is defined by the number of scan conversions
	set during initialisation.

	Parameters:
		buffer		-		Pointer to a buffer large enough for the number of scan conversions set during initialisation

	Returns:
		0	-	Success
		1	-	Error (no data available in the buffer)
*******************************************************************************/
unsigned char stm_adc_data_getnext(unsigned short *buffer,unsigned *nc,unsigned long *timems,unsigned long *pktctr)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is empty
		if(_stm_adc_buffer_wrptr==_stm_adc_buffer_rdptr)
			return 1;
		// Copy the data
		if(buffer)
		{
			// Faster to copy with loop than calling memcpy
			for(unsigned i=0;i<hadc1.Init.NbrOfConversion;i++)
			{
				buffer[i] = _stm_adc_buffers[_stm_adc_buffer_rdptr][i];
			}
			//memcpy((void*)buffer,(void*)_stm_adc_buffers[_stm_adc_buffer_rdptr],hadc1.Init.NbrOfConversion*sizeof(unsigned short));
		}
		if(nc)
			*nc = hadc1.Init.NbrOfConversion;
		if(timems)
			*timems = _stm_adc_buffers_time[_stm_adc_buffer_rdptr];
		if(pktctr)
			*pktctr = _stm_adc_buffers_pktnum[_stm_adc_buffer_rdptr];
		// Increment the read pointer
		_stm_adc_buffer_rdptr = (_stm_adc_buffer_rdptr+1)&STM_ADC_BUFFER_MASK;
		return 0;
	}
	return 1;	// To avoid compiler warning
}
/******************************************************************************
	function: _stm_adc_data_storenext
*******************************************************************************
	Internally called from audio data interrupts to fill the audio frame buffer.

	If the the audio frame buffer is full, this function removes the oldest frame
	to make space for the new frame provided to this function.
	The rationale for this is to guarantee that the audio frames are "recent", i.e.
	the frame buffers stores only the STM_DFSMD_BUFFER_NUM most recent audio frames.
	This allows for better alignment with other data sources if buffers were to overrun.

	Do not call from user code.

	Parameters:
		buffer		-		Pointer containing the new audio frame to store in the frame buffer

	Returns:
*******************************************************************************/
void _stm_adc_data_storenext(unsigned short *buffer,unsigned long timems)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		// Check if buffer is full
		if( ((_stm_adc_buffer_wrptr+1)&STM_ADC_BUFFER_MASK) == _stm_adc_buffer_rdptr )
		{
			// Drop the oldest frame by incrementing the read pointer
			_stm_adc_buffer_rdptr = (_stm_adc_buffer_rdptr+1)&STM_ADC_BUFFER_MASK;
			// Increment the lost counter
			_stm_adc_ctr_loss++;
		}
		// Here the buffer is not full: copy the data at the write pointer
		for(unsigned i=0;i<hadc1.Init.NbrOfConversion;i++)
		{
			_stm_adc_buffers[_stm_adc_buffer_wrptr][i] = buffer[i];
		}
		// Store time
		_stm_adc_buffers_time[_stm_adc_buffer_wrptr] = timems;
		// Store packet counter
		_stm_adc_buffers_pktnum[_stm_adc_buffer_wrptr] = _stm_adc_ctr_acq;

		// Increment the write pointer
		_stm_adc_buffer_wrptr = (_stm_adc_buffer_wrptr+1)&STM_ADC_BUFFER_MASK;
		// Increment the frame counter
		_stm_adc_ctr_acq++;
		return;
	}
}
/******************************************************************************
	function: stm_adc_data_level
*******************************************************************************
	Returns how many audio frames are in the buffer.
*******************************************************************************/
unsigned char stm_adc_data_level(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return (_stm_adc_buffer_wrptr-_stm_adc_buffer_rdptr)&STM_ADC_BUFFER_MASK;
	}
	return 0;	// To avoid compiler warning
}
/******************************************************************************
	function: stm_adc_data_clear
*******************************************************************************
	Resets the audio frame buffer
*******************************************************************************/
void stm_adc_data_clear(void)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_adc_buffer_wrptr=0;
		_stm_adc_buffer_rdptr=0;
		stm_adc_clearstat();
	}
}




/******************************************************************************
	function: stm_adc_clearstat
*******************************************************************************
	Clear audio frame acquisition statistics.

	Parameters:

	Returns:

******************************************************************************/
void stm_adc_clearstat()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		_stm_adc_ctr_acq=_stm_adc_ctr_loss=0;
	}
}
unsigned long stm_adc_stat_totframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_adc_ctr_acq;
	}
	return 0;
}
unsigned long stm_adc_stat_lostframes()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		return _stm_adc_ctr_loss;
	}
	return 0;
}
/******************************************************************************
	function: stm_adc_acquireonce
*******************************************************************************
	Start acquisition (DMA, scan conversion)

	Parameters:

	Returns:
		0			-		Ok
		Nonzero		-		Error

******************************************************************************/
unsigned char stm_adc_acquireonce()
{
	HAL_StatusTypeDef s;

	// Clear DMA buffer for debugging purposes
	memset(_stm_adc_dmabuf,0xff,STM_ADC_MAXCHANNEL*sizeof(unsigned short));

	s = HAL_ADC_Start_DMA(&hadc1,(uint32_t*)_stm_adc_dmabuf,hadc1.Init.NbrOfConversion);
	return s?1:0;
}
/******************************************************************************
	function: stm_adc_acquireonce
*******************************************************************************
	Start acquisition (DMA, scan conversion)

	Parameters:
		grouping	-		Number of ADC sampling before triggering an interrupt

	Returns:
		0			-		Ok
		Nonzero		-		Error

******************************************************************************/
unsigned char stm_adc_acquirecontinuously(unsigned grouping)
{
	HAL_StatusTypeDef s;

	if(grouping<1) grouping=1;
	if(grouping>STM_ADC_MAXGROUPING) grouping=STM_ADC_MAXGROUPING;

	fprintf(file_pri,"Num conv: %d Grouping: %u\n",hadc1.Init.NbrOfConversion,grouping);

	_stm_adc_grouping = grouping;

	// Clear DMA buffer for debugging purposes
	memset(_stm_adc_dmabuf,0xff,DMABUFSIZ*sizeof(unsigned short));

	s = HAL_ADC_Start_DMA(&hadc1,(uint32_t*)_stm_adc_dmabuf,hadc1.Init.NbrOfConversion*grouping*2);		// Twice number of conversion to use double buffering

	fprintf(file_pri,"HAL_ADC_Start_DMA: %d\n",s);

	return s?1:0;
}
/******************************************************************************
	function: HAL_ADC_ConvCpltCallback
*******************************************************************************
	Stores the acquired data in the ADC buffer
*******************************************************************************/
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	unsigned long t = timer_ms_get();

#if 0
	static hctr=0;
	hctr++;

	/*unsigned eoc = __HAL_ADC_GET_FLAG(hadc, ADC_FLAG_EOC);
	unsigned eos = __HAL_ADC_GET_FLAG(hadc, ADC_FLAG_EOS);

	unsigned dr = hadc->Instance->DR;
	unsigned v = HAL_ADC_GetValue(&hadc1);*/

	itmprintf("HAL_ADC_ConvCpltCallback %u %lu\n",hctr,t);
	for(int i=0;i<DMABUFSIZ;i++)
	{
		itmprintf("%04X ",(unsigned)_stm_adc_dmabuf[i]);
	}
	itmprintf("\n");
#endif


	// Point to second half of DMA buffer
	unsigned short *dmabufptr = _stm_adc_dmabuf + hadc1.Init.NbrOfConversion*_stm_adc_grouping;
	// Copy _stm_adc_grouping samples
	for(unsigned i=0;i<_stm_adc_grouping;i++)
	{
		_stm_adc_data_storenext(dmabufptr,t);		// Copy data from 2nd half of buffer.
		// Go to next group
		dmabufptr+=hadc1.Init.NbrOfConversion;
	}

	//_stm_adc_data_storenext(_stm_adc_dmabuf+hadc1.Init.NbrOfConversion,t);		// Copy data from 2nd half of buffer.


	// Clear DMA buffer for debugging purposes
	//memset(_stm_adc_dmabuf,0xaa,DMABUFSIZ*sizeof(unsigned short));

}

/******************************************************************************
	function: HAL_ADC_ConvHalfCpltCallback
*******************************************************************************
	Double buffering: copy first half of DMA buffer
*******************************************************************************/
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
	unsigned long t = timer_ms_get();
#if 0
	static hctr=0;
	hctr++;
	itmprintf("HAL_ADC_ConvHalfCpltCallback %u %lu\n",hctr,t);
	for(int i=0;i<DMABUFSIZ;i++)
	{
		itmprintf("%04X ",(unsigned)_stm_adc_dmabuf[i]);
	}
	itmprintf("\n");
#endif



	//itmprintf("HAL_ADC_ConvHalfCpltCallback %lu\n",t);
	//stm_adc_data_storenext(_stm_adc_dmabuf+0,t);		// Copy data from 1st half of buffer.


	// Point to first half of DMA buffer
	unsigned short *dmabufptr = _stm_adc_dmabuf;
	// Copy _stm_adc_grouping samples
	for(unsigned i=0;i<_stm_adc_grouping;i++)
	{
		// Copy group
		_stm_adc_data_storenext(dmabufptr,t);
		// Go to next group
		dmabufptr+=hadc1.Init.NbrOfConversion;

	}

	// Clear DMA buffer for debugging purposes
	//memset(_stm_adc_dmabuf,0x55,DMABUFSIZ*sizeof(unsigned short));
}

void HAL_ADCEx_EndOfSamplingCallback(ADC_HandleTypeDef *hadc)
{
	fprintf(file_pri,"HAL_ADCEx_EndOfSamplingCallback\n");
	itmprintf("HAL_ADCEx_EndOfSamplingCallback\n");

	// if( __HAL_ADC_GET_FLAG(&hadc, ADC_FLAG_EOS))
}

/******************************************************************************
	function: _stm_adc_init_timer
*******************************************************************************
	Init timer for timer-triggered conversion

	Clock is at 20MHz

	Use timer 15

*******************************************************************************/
void _stm_adc_init_timer(unsigned prescaler,unsigned reload)
{
	LL_TIM_InitTypeDef TIM_InitStruct = {0};

	// Peripheral clock enable
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM15);

	// TIM15 interrupt Init
	NVIC_SetPriority(TIM1_BRK_TIM15_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(TIM1_BRK_TIM15_IRQn);

	TIM_InitStruct.Prescaler = prescaler;
	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;
	TIM_InitStruct.Autoreload = reload;
	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
	TIM_InitStruct.RepetitionCounter = 0;
	LL_TIM_Init(TIM15, &TIM_InitStruct);

	LL_TIM_SetClockSource(TIM15, LL_TIM_CLOCKSOURCE_INTERNAL);
	LL_TIM_SetTriggerOutput(TIM15, LL_TIM_TRGO_UPDATE);
	LL_TIM_DisableMasterSlaveMode(TIM15);

	//LL_TIM_EnableIT_UPDATE(TIM15);
	//LL_TIM_EnableUpdateEvent(TIM15);
	LL_TIM_EnableCounter(TIM15);
	LL_TIM_GenerateEvent_UPDATE(TIM15);

}
/******************************************************************************
	function: _stm_adc_deinit_timer
*******************************************************************************
	Stop timer for conversions

	Use timer 15

*******************************************************************************/
void _stm_adc_deinit_timer()
{
	LL_TIM_DisableCounter(TIM15);			// Stop counting
	//LL_TIM_DisableIT_UPDATE(TIM15);			// Stop interrupts		// Interrupts not generated anymore
}

// Timer 15 interrrupt handler
void TIM1_BRK_TIM15_IRQHandler(void)
{
	if(LL_TIM_IsActiveFlag_UPDATE(TIM15) == 1)
	{
		// Clear the update interrupt flag
		LL_TIM_ClearFlag_UPDATE(TIM15);
	}
	// TIM1 update interrupt processing
	//TimerUpdate_Callback();

	static unsigned long ctr=0;
	static unsigned long tlast=0;

	if(file_pri)
	{
		ctr++;
		unsigned long t = timer_ms_get();
		//fprintf(file_pri,"t %lu: %lu\n",ctr,t-tlast);
		itmprintf("t %lu: %lu\n",ctr,t-tlast);
		tlast=t;
	}
}


/******************************************************************************
	function: stm_adc_perfbench_withreadout
*******************************************************************************
	Benchmark all the audio acquisition overhead and indicates CPU overhead and
	sample loss.

******************************************************************************/
unsigned long stm_adc_perfbench_withreadout(unsigned long mintime)
{
	unsigned long int t_last,t_cur;
	//unsigned long int tms_last,tms_cur;
	unsigned long int ctr,cps,nsample;
	//const unsigned long int mintime=1000;
	unsigned short adc[STM_ADC_MAXCHANNEL];		// ADC buffer
	unsigned long adcnc,adcms,adcpkt;

	ctr=0;
	nsample=0;

	//mintime=mintime*1000;
	t_last=timer_s_wait();
	stm_adc_data_clear();			// Clear data buffer and statistics

	unsigned long tint1=timer_ms_get_intclk();
	while((t_cur=timer_s_get())-t_last<mintime)
	{
		ctr++;

		// Simulate reading out the data from the buffers
		// Read until no data available

		while(!stm_adc_data_getnext(adc,&adcnc,&adcms,&adcpkt))
			nsample++;

	}
	unsigned long tint2=timer_ms_get_intclk();

	unsigned long totf = stm_adc_stat_totframes();
	unsigned long lostf = stm_adc_stat_lostframes();

	cps = ctr/(t_cur-t_last);

	fprintf(file_pri,"perfbench_withreadout: %lu perf (%lu intclk ms) Frames: %lu. Frames/sec: %lu\n",cps,tint2-tint1,nsample,nsample/(t_cur-t_last));
	fprintf(file_pri,"\tTotal frames: %lu. Lost frames: %lu\n",totf,lostf);
	return cps;
}
