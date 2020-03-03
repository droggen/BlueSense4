#ifndef __INTERFACE_H
#define __INTERFACE_H


/**
 * @brief Some brief stuff
 *
 * Some long stuff
 */

extern signed char interface_bt_connected_past;
extern signed char interface_usb_connected_past;
extern unsigned char _interface_changedetectenabled;

void interface_btup(void);
void interface_btdown(void);
void interface_usbup(void);
void interface_usbdown(void);
void interface_hellopri(void);
void interface_hellodbg(void);
void interface_test(void);
void interface_swap(void);
void interface_init(void);


void interface_changedetectenable(unsigned char enable);
void interface_signalchange(signed char cur_bt_connected,signed char cur_usb_connected);

void _interface_update(signed char cur_bt_connected,signed char cur_usb_connected);

void init_interface_change_detect();
void interface_change_detect_periodic();

#endif
