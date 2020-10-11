#ifndef __MODE_H
#define __MODE_H



#define APP_MODE_MAIN 0
#define APP_MODE_CLOCK 3
#define APP_MODE_DEMO 4
#define APP_MODE_ADC 5
#define APP_MODE_BENCHIO 6
#define APP_MODE_BT 7
#define APP_MODE_MOTIONSTREAM 8
#define APP_MODE_COULOMB 9
#define APP_MODE_SD 10
#define APP_MODE_TS 11
#define APP_MODE_TESTSYNC 12
#define APP_MODE_MOTIONRECOG 13
#define APP_MODE_MPUTEST 14
#define APP_MODE_EEPROM 15
#define APP_MODE_RTC 16
#define APP_MODE_MPUTESTN 17
#define APP_MODE_I2CTST 18
#define APP_MODE_INTERFACE 19
#define APP_MODE_AUDIO 20
#define APP_MODE_BENCHMARK 21
#define APP_MODE_SAMPLESOUND 22
#define APP_MODE_ADCTEST 23
#define APP_MODE_SAMPLEMULTIMODAL 24
#define APP_MODE_DACTEST 25
#define APP_MODE_RTC_EXT 26
#define APP_MODE_SERIALTEST 27
#define APP_MODE_MODULATION 28

void mode_dispatch(void);



#endif
