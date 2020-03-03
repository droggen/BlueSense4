/*
 * system.h
 *
 *  Created on: 8 Aug 2019
 *      Author: droggen
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#define LED_RED 0
#define LED_YELLOW 1
#define LED_GREEN 2

void system_periphvcc_enable();
void system_periphvcc_disable();
void system_motionvcc_enable();
void system_motionvcc_disable();

void system_periph_setrn41resetpin(int set);

void system_delay_ms(unsigned short t);

void system_led_test(void);
void system_led_on(unsigned char ledn);
void system_led_off(unsigned char ledn);
void system_led_toggle(unsigned char led);
void system_led_set(unsigned char led);
unsigned char system_lifesign(unsigned char sec);
unsigned char system_lifesign2(unsigned char sec);
void system_status_ok(unsigned char status);
void system_blink_led(unsigned char n,unsigned char timeon,unsigned char timeoff,unsigned char led);
unsigned system_buttonpress();
unsigned char system_isbtconnected(void);
unsigned char system_isusbconnected(void);
void system_sleep();

#endif /* SYSTEM_H_ */
