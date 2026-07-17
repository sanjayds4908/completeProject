/**
  ******************************************************************************
  * @file    motor_control.c
  * @brief   TB6612 Motor Driver — PWM speed, direction, power down/up
  ******************************************************************************
  */

#include "motor_control.h"

void motor_init(void)
{
    HAL_TIM_PWM_Start(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL);
    motor_stop();
}

void motor_set_speed(uint8_t percent)
{
    if (percent > MOTOR_MAX_PERCENT) percent = MOTOR_MAX_PERCENT;
    uint16_t pulse = (uint16_t)((percent * MOTOR_PWM_MAX) / 100);
    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL, pulse);

    osMutexAcquire(g_sys_mutex, osWaitForever);
    g_sys.motor_pwm = pulse;
    g_sys.motor_state = (pulse == 0) ? MOTOR_STATE_STOPPED :
                        (percent < 50) ? MOTOR_STATE_REDUCED : MOTOR_STATE_RUNNING;
    osMutexRelease(g_sys_mutex);
}

void motor_forward(void)
{
    HAL_GPIO_WritePin(MOTOR_IN1_PORT, MOTOR_IN1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MOTOR_IN2_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
}

void motor_reverse(void)
{
    HAL_GPIO_WritePin(MOTOR_IN1_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_IN2_PORT, MOTOR_IN2_PIN, GPIO_PIN_SET);
}

void motor_stop(void)
{
    /* Kill PWM and direction FIRST — no mutex needed for hardware */
    HAL_GPIO_WritePin(MOTOR_IN1_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_IN2_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL, 0);

    osMutexAcquire(g_sys_mutex, osWaitForever);
    g_sys.motor_pwm = 0;
    g_sys.motor_state = MOTOR_STATE_STOPPED;
    osMutexRelease(g_sys_mutex);
}

/* Emergency stop — no mutex, just kill the motor immediately
 * Use this from safety context where mutex might cause issues */
void motor_emergency_stop(void)
{
    HAL_GPIO_WritePin(MOTOR_IN1_PORT, MOTOR_IN1_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(MOTOR_IN2_PORT, MOTOR_IN2_PIN, GPIO_PIN_RESET);
    __HAL_TIM_SET_COMPARE(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL, 0);

    /* Direct write — no mutex, atomic enough for safety */
    g_sys.motor_pwm = 0;
    g_sys.motor_state = MOTOR_STATE_STOPPED;
}

void motor_reduce_speed(void)
{
    motor_set_speed(30);
}

void motor_resume_nominal(void)
{
    motor_forward();
    motor_set_speed(70);
}

void motor_power_down(void)
{
    motor_stop();
    HAL_TIM_PWM_Stop(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL);
    __HAL_RCC_TIM3_CLK_DISABLE();
}

void motor_power_up(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();
    HAL_TIM_PWM_Start(&MOTOR_PWM_TIM, MOTOR_PWM_CHANNEL);
}
