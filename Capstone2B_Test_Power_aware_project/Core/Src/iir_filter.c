/**
  ******************************************************************************
  * @file    iir_filter.c
  * @brief   Technique 4: IIR Low-Pass Filter
  *          Filtered = α × new + (1-α) × previous
  *          α = 0.1 → heavy smoothing, removes sensor noise
  ******************************************************************************
  */

#include "iir_filter.h"

static float filtered = 25.0f;
static bool  initialized = false;

void iir_filter_init(float initial_value)
{
    filtered = initial_value;
    initialized = true;
}

float iir_filter_update(float new_sample)
{
    if (!initialized) { filtered = new_sample; initialized = true; }
    else filtered = IIR_ALPHA * new_sample + (1.0f - IIR_ALPHA) * filtered;
    return filtered;
}

float iir_filter_get(void) { return filtered; }

void iir_filter_reset(float reset_value)
{
    filtered = reset_value;
    initialized = true;
}
