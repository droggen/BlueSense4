/*
 * mode_sample_multimodal.h
 *
 *  Created on: 5 Feb 2020
 *      Author: droggen
 */

#ifndef MODE_SAMPLE_MULTIMODAL_H_
#define MODE_SAMPLE_MULTIMODAL_H_

#define MULTIMODAL_MPU (0b100)
#define MULTIMODAL_SND (0b010)
#define MULTIMODAL_ADC (0b001)

extern unsigned mode_sample_multimodal_mode;

void mode_sample_multimodal(void);
unsigned char CommandParserSampleMultimodal(char *buffer,unsigned char size);

#endif /* MODE_SAMPLE_MULTIMODAL_H_ */
