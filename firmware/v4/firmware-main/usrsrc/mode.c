/*
	file: mode
	
	Main 'application mode' files, handling the main loop waiting for user commands.
	
		
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
//#include "adc.h"
#include "serial.h"
#include "i2c.h"
#if BUILD_BLUETOOTH==1
#include "rn41.h"
#include "mode_bt.h"
#endif

#include "commandset.h"
#include "mode_adc.h"
#include "mode_sample_adc.h"
///#include "mode_benchio.h"
//#include "mode_idle.h"
#include "mode_sd.h"
#if ENABLEMODECOULOMB==1
#include "mode_coulomb.h"
#endif
#include "mode_sample_motion.h"
//#include "mode_motionrecog.h"
//#include "mode_teststream.h"
//#include "mode_testsync.h"
#include "mode_mputest.h"
//#include "mode_demo.h"
//#include "mode_demo_clock.h"
#include "mode.h"
#include "mode_main.h"
#include "mode_eeprom.h"
/*#include "mode_bench.h"
#include "mode_bt.h"*/
#include "mode_rtc.h"
#include "mode_rtcext.h"
#include <mode_i2ctst.h>
//#include "mode_mputest.h"
#include "mode_mputest_n.h"
#include "mode_interface.h"
#include "mode_audio.h"
#include "mode_sample_sound.h"
#include "mode_sample_multimodal.h"
#include <mode_benchmarks.h>
#include "mode_dac.h"
#include "mode_serial.h"

void mode_dispatch(void)
{
	
	while(1)
	{
		//fprintf_P(file_pri,PSTR("Current mode: %d\n"),system_mode);
		//_delay_ms(500);
		//system_status_ok(system_mode);
		//_delay_ms(500);
		switch(system_mode)
		{
			
			case APP_MODE_MAIN:
				mode_main();
				break;			
			/*case 1:
				//stream();
				//mpu_testsleep();
				system_mode=0;
				break;
			case 2:
				//main_log();
				system_mode=0;
				break;
#if ENABLEGFXDEMO==1
			case APP_MODE_CLOCK:			
				mode_clock();
				break;
			case APP_MODE_DEMO:
				mode_demo();
				break;
#endif
*/
			/*case APP_MODE_BENCHIO:
				mode_benchio();
				system_mode=APP_MODE_MAIN;
				break;*/

#if BUILD_BLUETOOTH==1
#if DBG_RN41TERMINAL==1
			case APP_MODE_BT:
				mode_bt();
				system_mode=0;
				break;
#endif
#endif
			case APP_MODE_MOTIONSTREAM:
				mode_sample_motion();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_MPUTEST:
				mode_mputest();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_MPUTESTN:
				mode_mputest_n();
				system_mode=APP_MODE_MAIN;
				break;
			/*case APP_MODE_MOTIONRECOG:
				//mode_motionrecog();
				system_mode=0;
				break;*/
#if ENABLEMODECOULOMB==1
			case APP_MODE_COULOMB:
				mode_coulomb();
				system_mode=APP_MODE_MAIN;
				break;
#endif

			case APP_MODE_SD:
				mode_sd();
				system_mode=APP_MODE_MAIN;
				break;
			/*case APP_MODE_TS:
				mode_teststream();
				system_mode=0;
				break;
			case APP_MODE_TESTSYNC:
				printf("mode testsync\n");
				mode_testsync();
				system_mode=0;
				break;*/

			case APP_MODE_ADC:			
				mode_sample_adc();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_ADCTEST:
				mode_adc();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_EEPROM:
				mode_eeprom();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_RTC:
				mode_rtc();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_I2CTST:
				mode_i2ctst();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_INTERFACE:
				mode_interface();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_AUDIO:
				mode_audio();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_BENCHMARK:
				mode_benchmarkcpu();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_SAMPLESOUND:
				mode_sample_sound();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_SAMPLEMULTIMODAL:
				mode_sample_multimodal();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_DACTEST:
				mode_dactest();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_RTC_EXT:
				mode_rtcext();
				system_mode=APP_MODE_MAIN;
				break;
			case APP_MODE_SERIALTEST:
				mode_serialtest();
				system_mode=APP_MODE_MAIN;
				break;
			default:
				system_mode=APP_MODE_MAIN;
			
		}
	}
	
}
