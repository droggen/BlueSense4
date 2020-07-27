#ifndef __MPU_USART0
#define __MPU_USART0

extern unsigned char _mpu_tmp[];
extern unsigned char *_mpu_tmp_reg;
extern volatile unsigned char _mpu_ongoing;				

void _mpu_waitavailandreserve(void);
unsigned char _mpu_tryavailandreserve(void);

void mpu_writereg_wait_poll_block(unsigned char reg,unsigned char v);
unsigned char mpu_readreg_wait_poll_block(unsigned char reg);
//unsigned short mpu_readreg16(unsigned char reg);
void mpu_readregs_wait_poll_block(unsigned char *d,unsigned char reg,unsigned char n);
void mpu_readregs_wait_poll_or_int_block(unsigned char *d,unsigned char reg,unsigned char n);
unsigned char mpu_readregs_try_poll_block_raw(unsigned char *d,unsigned char reg,unsigned char n);
unsigned char mpu_readregs_try_int_cb(unsigned char *d,unsigned char reg,unsigned char n,void (*cb)(void));
unsigned char mpu_readregs_try_int_cb_raw(unsigned char reg,unsigned char n,void (*cb)(void));
void __mpu_readregs_int_cb_cb(void);


#endif
