/*
 * i2c_poll.h
 *
 *  Created on: 3 oct. 2019
 *      Author: droggen
 */

#ifndef I2C_I2C_POLL_H_
#define I2C_I2C_POLL_H_

void i2c_poll_write(unsigned char addr, unsigned char n, unsigned char *data,unsigned char stop);
void i2c_poll_read(unsigned char addr, unsigned char n, unsigned char *data,unsigned char stop);

#endif /* I2C_I2C_POLL_H_ */
