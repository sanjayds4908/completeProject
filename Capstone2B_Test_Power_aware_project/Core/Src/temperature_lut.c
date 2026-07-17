/**
  ******************************************************************************
  * @file    temperature_lut.c
  * @brief   Temperature: LUT + Interpolation + ADC Averaging
  *          LUT avoids expensive log()/division → less CPU → more WFI time
  ******************************************************************************
  */

#include "temperature_lut.h"

/* 30-entry LUT: 10kΩ NTC, Beta=3950, Rfixed=10kΩ, Vref=3.3V */
static const TempLUT_Entry_t TEMP_LUT[] = {
    {    0,   70.0f},
    {  214,   65.0f},
    {  428,   60.0f},
    {  642,   55.0f},
    {  857,   50.0f},
    { 1071,   45.0f},
    { 1285,   40.0f},
    { 1500,   35.0f},
    { 1714,   30.0f},
    { 1928,   25.0f},
    { 2142,   20.0f},
    { 2357,   15.0f},
    { 2571,   10.0f},
    { 2785,    5.0f},
    { 3000,    0.0f},
    { 3214,   -5.0f},
    { 3428,  -10.0f},
};


#define LUT_SIZE (sizeof(TEMP_LUT) / sizeof(TEMP_LUT[0]))

/* Technique 1: ADC Averaging — 20 samples to reduce noise */
uint16_t temp_adc_read_averaged(void)
{
    uint32_t acc = 0;
    for (int i = 0; i < ADC_SAMPLE_COUNT; i++)
    {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
        acc += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    return (uint16_t)(acc / ADC_SAMPLE_COUNT);
}

/* Technique 2: LUT Lookup — nearest entry, no interpolation */
float temp_lut_lookup(uint16_t adc_value)
{
    if (adc_value <= TEMP_LUT[0].adc_value) return TEMP_LUT[0].temperature_c;
    if (adc_value >= TEMP_LUT[LUT_SIZE-1].adc_value) return TEMP_LUT[LUT_SIZE-1].temperature_c;

    for (int i = 0; i < (int)LUT_SIZE - 1; i++)
    {
        if (adc_value >= TEMP_LUT[i].adc_value && adc_value < TEMP_LUT[i+1].adc_value)
            return TEMP_LUT[i].temperature_c;
    }
    return TEMP_LUT[LUT_SIZE-1].temperature_c;
}

/* Technique 3: LUT + Linear Interpolation — high accuracy */
float temp_lut_interpolate(uint16_t adc_value)
{
    if (adc_value <= TEMP_LUT[0].adc_value) return TEMP_LUT[0].temperature_c;
    if (adc_value >= TEMP_LUT[LUT_SIZE-1].adc_value) return TEMP_LUT[LUT_SIZE-1].temperature_c;

    for (int i = 0; i < (int)LUT_SIZE - 1; i++)
    {
        uint16_t a1 = TEMP_LUT[i].adc_value;
        uint16_t a2 = TEMP_LUT[i+1].adc_value;
        if (adc_value >= a1 && adc_value <= a2)
        {
            float t1 = TEMP_LUT[i].temperature_c;
            float t2 = TEMP_LUT[i+1].temperature_c;
            return t1 + (float)(adc_value - a1) * (t2 - t1) / (float)(a2 - a1);
        }
    }
    return TEMP_LUT[LUT_SIZE-1].temperature_c;
}
