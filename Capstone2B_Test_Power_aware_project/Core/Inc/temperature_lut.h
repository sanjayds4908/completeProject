/**
  ******************************************************************************
  * @file    temperature_lut.h
  * @brief   Temperature: LUT + Interpolation + ADC Averaging
  ******************************************************************************
  */

#ifndef TEMPERATURE_LUT_H
#define TEMPERATURE_LUT_H

#include "system_config.h"

typedef struct {
    uint16_t adc_value;
    float    temperature_c;
} TempLUT_Entry_t;

uint16_t temp_adc_read_averaged(void);
float    temp_lut_lookup(uint16_t adc_value);
float    temp_lut_interpolate(uint16_t adc_value);

#endif
