/**
  ******************************************************************************
  * @file    power_manager.c
  * @brief   Low-Power Mode Management — with I2C bus protection
  ******************************************************************************
  */

#include "power_manager.h"
#include "motor_control.h"
#include "iir_filter.h"
#include "ssd1306.h"

extern void i2c_bus_reset(void);

void power_manager_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    HAL_PWR_DisableSleepOnExit();

    g_sys.system_mode = SYSTEM_MODE_ACTIVE;
    g_sys.last_activity_tick = HAL_GetTick();

    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_SET);
}

void power_manager_enter_sleep(void)
{
    /* 1. Stop motor */
    motor_power_down();

    /* 2. Grab I2C mutex — ensures no task is mid-transfer */
    osMutexAcquire(g_i2c_mutex, osWaitForever);

    /* 3. Turn off OLED */
    ssd1306_display_off();

    /* 4. Suspend tasks that use I2C (safe — we hold the mutex) */
    osThreadSuspend(motorTaskHandle);
    osThreadSuspend(displayTaskHandle);
    osThreadSuspend(loggingTaskHandle);
    osThreadSuspend(powerMonTaskHandle);

    /* 5. Release I2C mutex */
    osMutexRelease(g_i2c_mutex);

    /* 6. Disable clocks */
    __HAL_RCC_USART2_CLK_DISABLE();
    __HAL_RCC_DMA1_CLK_DISABLE();

    /* 7. LED OFF */
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_RESET);

    /* 8. Update state */
    osMutexAcquire(g_sys_mutex, osWaitForever);
    g_sys.system_mode = SYSTEM_MODE_POWER_SAVE;
    osMutexRelease(g_sys_mutex);
}

void power_manager_exit_sleep(void)
{
    /* 1. Re-enable clocks */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* 2. Motor timer back on */
    motor_power_up();

    /* 3. Grab I2C mutex */
    osMutexAcquire(g_i2c_mutex, osWaitForever);

    /* 4. Full hardware I2C bus reset — guarantees clean state */
    i2c_bus_reset();

    /* 5. OLED back on */
    ssd1306_display_on();

    /* 6. Release I2C mutex before resuming tasks */
    osMutexRelease(g_i2c_mutex);

    /* 7. Resume tasks */
    osThreadResume(motorTaskHandle);
    osThreadResume(displayTaskHandle);
    osThreadResume(loggingTaskHandle);
    osThreadResume(powerMonTaskHandle);

    /* 8. LED ON */
    HAL_GPIO_WritePin(STATUS_LED_PORT, STATUS_LED_PIN, GPIO_PIN_SET);

    /* 9. Reset filter */
    iir_filter_reset(g_sys.lut_temperature);

    /* 10. Reset inactivity */
    reset_activity();

    /* 11. Update state */
    osMutexAcquire(g_sys_mutex, osWaitForever);
    g_sys.system_mode = SYSTEM_MODE_ACTIVE;
    osMutexRelease(g_sys_mutex);
}

bool power_manager_should_sleep(void)
{
    if (g_sys.system_mode != SYSTEM_MODE_ACTIVE)
        return false;

    if (!is_system_idle())
        return false;

    uint32_t elapsed = HAL_GetTick() - g_sys.last_activity_tick;
    return (elapsed >= INACTIVITY_TIMEOUT_MS);
}

void power_manager_wakeup_event(uint16_t gpio_pin)
{
    if (g_sys.system_mode == SYSTEM_MODE_POWER_SAVE)
    {
        BaseType_t xWoken = pdFALSE;
        xTaskNotifyFromISR((TaskHandle_t)safetyTaskHandle,
                           (uint32_t)gpio_pin,
                           eSetBits, &xWoken);
        portYIELD_FROM_ISR(xWoken);
    }
}
