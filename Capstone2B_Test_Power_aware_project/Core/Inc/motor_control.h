/**
  ******************************************************************************
  * @file    motor_control.h
  * @brief   TB6612 Motor Driver Control
  ******************************************************************************
  */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "system_config.h"

void motor_init(void);
void motor_set_speed(uint8_t percent);
void motor_forward(void);
void motor_reverse(void);
void motor_stop(void);
void motor_emergency_stop(void);
void motor_reduce_speed(void);
void motor_resume_nominal(void);
void motor_power_down(void);
void motor_power_up(void);

#endif
