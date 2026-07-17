/**
  ******************************************************************************
  * @file    power_monitor.h
  * @brief   INA219 I2C Current/Voltage/Power Sensor
  ******************************************************************************
  */

#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

#include "system_config.h"

#define INA219_ADDR     (0x40 << 1)

HAL_StatusTypeDef ina219_init(void);
float ina219_read_voltage(void);
float ina219_read_current(void);
float ina219_read_power(void);

#endif
