/**
  ******************************************************************************
  * @file    power_manager.h
  * @brief   Low-Power Mode: Sleep Entry/Exit, WFI, Wake-Up
  ******************************************************************************
  */

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include "system_config.h"

void power_manager_init(void);
void power_manager_enter_sleep(void);
void power_manager_exit_sleep(void);
bool power_manager_should_sleep(void);
void power_manager_wakeup_event(uint16_t gpio_pin);

#endif
