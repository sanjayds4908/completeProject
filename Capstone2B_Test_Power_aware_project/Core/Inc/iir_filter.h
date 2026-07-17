/**
  ******************************************************************************
  * @file    iir_filter.h
  * @brief   IIR Low-Pass Filter for temperature smoothing
  ******************************************************************************
  */

#ifndef IIR_FILTER_H
#define IIR_FILTER_H

#include "system_config.h"

void  iir_filter_init(float initial_value);
float iir_filter_update(float new_sample);
float iir_filter_get(void);
void  iir_filter_reset(float reset_value);

#endif
