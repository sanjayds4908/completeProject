/**
  ******************************************************************************
  * @file    safety_monitor.h
  * @brief   Fault Detection & Protective Actions
  ******************************************************************************
  */

#ifndef SAFETY_MONITOR_H
#define SAFETY_MONITOR_H

#include "system_config.h"

void safety_init(void);
bool safety_ir_detected(void);
bool safety_flame_detected(void);
void safety_evaluate(void);
void safety_buzzer_alarm(void);
void safety_buzzer_warning(void);
void safety_buzzer_off(void);

#endif
