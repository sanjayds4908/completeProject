/**
  ******************************************************************************
  * @file    safety_monitor.c
  * @brief   Priority-based Safety Evaluation
  *          1. Flame → STOP + ALARM
  *          2. Critical overcurrent → STOP + ALARM
  *          3. Overcurrent warning → REDUCE + BEEP
  *          4. Over-temperature → BEEP
  *          5. IR detected → BEEP
  *          6. All clear → NORMAL
  ******************************************************************************
  */

#include "safety_monitor.h"
#include "motor_control.h"

static uint32_t buzzer_toggle_tick = 0;
static bool buzzer_on = false;

void safety_init(void)
{
    safety_buzzer_off();
}

bool safety_ir_detected(void)
{
    return (HAL_GPIO_ReadPin(IR_PORT, IR_PIN) == GPIO_PIN_RESET);
}

bool safety_flame_detected(void)
{
    return (HAL_GPIO_ReadPin(FLAME_PORT, FLAME_PIN) == GPIO_PIN_RESET);
}

void safety_buzzer_alarm(void)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN_DEF, GPIO_PIN_SET);
    buzzer_on = true;
}

void safety_buzzer_warning(void)
{
    uint32_t now = HAL_GetTick();
    if (now - buzzer_toggle_tick >= 500)
    {
        buzzer_toggle_tick = now;
        buzzer_on = !buzzer_on;
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN_DEF,
                          buzzer_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }
}

void safety_buzzer_off(void)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN_DEF, GPIO_PIN_RESET);
    buzzer_on = false;
}

void safety_evaluate(void)
{
    bool flame = safety_flame_detected();
    bool ir    = safety_ir_detected();
    float current_mA, temperature;

    osMutexAcquire(g_sys_mutex, osWaitForever);
    current_mA  = g_sys.current_mA;
    temperature = g_sys.filtered_temperature;
    g_sys.ir_detected    = ir;
    g_sys.flame_detected = flame;
    osMutexRelease(g_sys_mutex);

    /* Priority 1: FLAME */
    if (flame)
    {
        motor_stop();
        safety_buzzer_alarm();
        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.safety_status = SAFETY_CRITICAL;
        osMutexRelease(g_sys_mutex);
        reset_activity();
        return;
    }

    /* Priority 2: CRITICAL OVERCURRENT */
    if (current_mA >= CURRENT_CRITICAL_MA && current_mA < 10000.0f)
    {
        motor_stop();
        safety_buzzer_alarm();
        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.safety_status = SAFETY_CRITICAL;
        osMutexRelease(g_sys_mutex);
        reset_activity();
        return;
    }

    /* Priority 3: OVERCURRENT WARNING */
    if (current_mA >= CURRENT_WARNING_MA && current_mA < 10000.0f)
    {
        motor_reduce_speed();
        safety_buzzer_warning();
        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.safety_status = SAFETY_WARNING;
        osMutexRelease(g_sys_mutex);
        reset_activity();
        return;
    }

    /* Priority 4: OVER-TEMPERATURE */
    if (temperature >= TEMP_WARNING_C)
    {
        safety_buzzer_warning();
        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.safety_status = SAFETY_WARNING;
        osMutexRelease(g_sys_mutex);
        reset_activity();
        return;
    }

    /* Priority 5: IR OBJECT */
    if (ir)
    {
        safety_buzzer_warning();
        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.safety_status = SAFETY_WARNING;
        osMutexRelease(g_sys_mutex);
        reset_activity();
        return;
    }

    /* All clear */
    safety_buzzer_off();
    osMutexAcquire(g_sys_mutex, osWaitForever);
    g_sys.safety_status = SAFETY_NORMAL;
    osMutexRelease(g_sys_mutex);
}
