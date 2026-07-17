/**
  ******************************************************************************
  * @file    system_config.h
  * @brief   Power-Aware Motor Safety Controller — System Configuration
  *          Capstone-2 Assignment | STM32F446RE + FreeRTOS
  ******************************************************************************
  */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include "main.h"
#include "cmsis_os.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Pin Definitions (matching CubeMX) ---- */

#define MOTOR_PWM_TIM           htim3
#define MOTOR_PWM_CHANNEL       TIM_CHANNEL_1

#define MOTOR_IN1_PORT          MOTOR_IN1_GPIO_Port
#define MOTOR_IN1_PIN           MOTOR_IN1_Pin
#define MOTOR_IN2_PORT          MOTOR_IN2_GPIO_Port
#define MOTOR_IN2_PIN           MOTOR_IN2_Pin

#define BUZZER_PORT             BUZZER_GPIO_Port
#define BUZZER_PIN_DEF          BUZZER_Pin

#define IR_PORT                 IR_SENSOR_GPIO_Port
#define IR_PIN                  IR_SENSOR_Pin

#define FLAME_PORT              FLAME_SENSOR_GPIO_Port
#define FLAME_PIN               FLAME_SENSOR_Pin

#define STATUS_LED_PORT         GPIOA
#define STATUS_LED_PIN          GPIO_PIN_5

#define USER_BTN_PORT           USER_BTN_GPIO_Port
#define USER_BTN_PIN_DEF        USER_BTN_Pin

/* ---- Thresholds ---- */

#define CURRENT_WARNING_MA      150.0f
#define CURRENT_CRITICAL_MA     200.0f
#define TEMP_WARNING_C          60.0f
#define TEMP_CRITICAL_C         80.0f
#define TEMP_SAFE_MAX_C         55.0f

#define MOTOR_PWM_MAX           99
#define MOTOR_PWM_NOMINAL       69      /* ~70% of 99 */
#define MOTOR_PWM_REDUCED       29      /* ~30% of 99 */
#define MOTOR_MAX_PERCENT       100

#define INACTIVITY_TIMEOUT_MS   10000   /* 10 seconds */
#define ADC_SAMPLE_COUNT        20
#define IIR_ALPHA               0.1f

/* ---- Enumerations ---- */

typedef enum {
    SYSTEM_MODE_ACTIVE,
    SYSTEM_MODE_POWER_SAVE
} SystemMode_t;

typedef enum {
    MOTOR_STATE_RUNNING,
    MOTOR_STATE_REDUCED,
    MOTOR_STATE_STOPPED
} MotorState_t;

typedef enum {
    SAFETY_NORMAL,
    SAFETY_WARNING,
    SAFETY_CRITICAL
} SafetyStatus_t;

/* ---- Shared System Data ---- */

typedef struct {
    /* Temperature */
    uint16_t        raw_adc;
    float           lut_temperature;
    float           filtered_temperature;

    /* Power (INA219) */
    float           current_mA;
    float           voltage_V;
    float           power_mW;

    /* Motor */
    MotorState_t    motor_state;
    uint16_t        motor_pwm;

    /* Safety */
    SafetyStatus_t  safety_status;
    bool            ir_detected;
    bool            flame_detected;

    /* System */
    SystemMode_t    system_mode;
    uint32_t        last_activity_tick;

} SystemData_t;

/* ---- Global Externs ---- */

extern SystemData_t         g_sys;
extern osMutexId_t          g_sys_mutex;
extern osMutexId_t          g_i2c_mutex;

extern ADC_HandleTypeDef    hadc1;
extern I2C_HandleTypeDef    hi2c1;
extern TIM_HandleTypeDef    htim3;
extern UART_HandleTypeDef   huart2;

extern osThreadId_t motorTaskHandle;
extern osThreadId_t powerMonTaskHandle;
extern osThreadId_t tempTaskHandle;
extern osThreadId_t safetyTaskHandle;
extern osThreadId_t displayTaskHandle;
extern osThreadId_t loggingTaskHandle;
extern osThreadId_t powerMgrTaskHandle;

/* ---- Utility ---- */

static inline void reset_activity(void)
{
    g_sys.last_activity_tick = HAL_GetTick();
}

static inline bool is_system_idle(void)
{
    return (g_sys.motor_state == MOTOR_STATE_STOPPED)
        && (!g_sys.ir_detected)
        && (!g_sys.flame_detected)
        && (g_sys.filtered_temperature < TEMP_SAFE_MAX_C);
}

#endif /* SYSTEM_CONFIG_H */
