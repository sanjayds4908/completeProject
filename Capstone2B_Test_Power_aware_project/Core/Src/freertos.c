/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : FreeRTOS Tasks — Power-Aware Motor Safety Controller
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "system_config.h"
#include "temperature_lut.h"
#include "iir_filter.h"
#include "motor_control.h"
#include "safety_monitor.h"
#include "power_manager.h"
#include "power_monitor.h"
#include "ssd1306.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

SystemData_t    g_sys;
osMutexId_t     g_sys_mutex;
osMutexId_t     g_i2c_mutex;

osThreadId_t motorTaskHandle;
osThreadId_t powerMonTaskHandle;
osThreadId_t tempTaskHandle;
osThreadId_t safetyTaskHandle;
osThreadId_t displayTaskHandle;
osThreadId_t loggingTaskHandle;
osThreadId_t powerMgrTaskHandle;

static char log_buf[256];
static bool safety_tripped = false;
static uint8_t motor_speed_mode = 0;

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

extern void i2c_bus_reset(void);

void TemperatureTask(void *argument);
void PowerMonitorTask(void *argument);
void MotorControlTask(void *argument);
void SafetySupervisorTask(void *argument);
void DisplayTask(void *argument);
void LoggingTask(void *argument);
void PowerManagerTask(void *argument);

/* USER CODE END FunctionPrototypes */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void TemperatureTask(void *argument)
{
    uint16_t adc = temp_adc_read_averaged();
    float temp = temp_lut_interpolate(adc);
    iir_filter_init(temp);

    for (;;)
    {
        adc = temp_adc_read_averaged();
        float lut_temp = temp_lut_interpolate(adc);
        float filtered = iir_filter_update(lut_temp);

        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.raw_adc = adc;
        g_sys.lut_temperature = lut_temp;
        g_sys.filtered_temperature = filtered;
        osMutexRelease(g_sys_mutex);

        osDelay(500);
    }
}

void PowerMonitorTask(void *argument)
{
    osMutexAcquire(g_i2c_mutex, osWaitForever);
    ina219_init();
    osMutexRelease(g_i2c_mutex);

    for (;;)
    {
        osMutexAcquire(g_i2c_mutex, osWaitForever);
        float v = ina219_read_voltage();
        float i = ina219_read_current();
        float p = ina219_read_power();
        osMutexRelease(g_i2c_mutex);

        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.voltage_V  = v;
        g_sys.current_mA = i;
        g_sys.power_mW   = p;
        osMutexRelease(g_sys_mutex);

        if (i > 10.0f && i < 5000.0f) reset_activity();

        osDelay(500);
    }
}

void MotorControlTask(void *argument)
{
    motor_init();

    for (;;)
    {
        if (g_sys.motor_state != MOTOR_STATE_STOPPED)
            reset_activity();

        osDelay(100);
    }
}

void SafetySupervisorTask(void *argument)
{
    safety_init();
    uint32_t notification;
    static uint32_t last_btn_tick = 0;

    for (;;)
    {
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notification, 0) == pdTRUE)
        {
            if (g_sys.system_mode == SYSTEM_MODE_POWER_SAVE)
            {
                power_manager_exit_sleep();
                osThreadResume(powerMgrTaskHandle);
            }

//            if (notification & USER_BTN_PIN_DEF)
//            {
//                uint32_t now = HAL_GetTick();
//                if (now - last_btn_tick > 500)
//                {
//                    last_btn_tick = now;
//                    if (g_sys.motor_state != MOTOR_STATE_STOPPED)
//                    {
//                        motor_stop();
//                    }
//                    else
//                    {
//                        safety_tripped = false;
//                        motor_forward();
//                        motor_set_speed(70);
//                    }
//                }
//            }
            if (notification & USER_BTN_PIN_DEF)
                        {
                            uint32_t now = HAL_GetTick();
                            if (now - last_btn_tick > 500)
                            {
                                last_btn_tick = now;
                                motor_speed_mode++;
                                if (motor_speed_mode > 3) motor_speed_mode = 0;

                                safety_tripped = false;

                                switch (motor_speed_mode)
                                {
                                    case 0:
                                        motor_stop();
                                        break;
                                    case 1:
                                        motor_forward();
                                        motor_set_speed(30);   /* LOW */
                                        break;
                                    case 2:
                                        motor_forward();
                                        motor_set_speed(60);   /* MEDIUM */
                                        break;
                                    case 3:
                                        motor_forward();
                                        motor_set_speed(100);  /* HIGH */
                                        break;
                                }
                            }
                        }
        }

        bool ir = safety_ir_detected();
        bool flame = safety_flame_detected();

        osMutexAcquire(g_sys_mutex, osWaitForever);
        g_sys.ir_detected    = ir;
        g_sys.flame_detected = flame;
        osMutexRelease(g_sys_mutex);

        /* Safety: use emergency stop then recover I2C */
        if (flame && !safety_tripped)
        {
            motor_emergency_stop();
            safety_buzzer_alarm();
            safety_tripped = true;

            osMutexAcquire(g_i2c_mutex, osWaitForever);
            i2c_bus_reset();
            osMutexRelease(g_i2c_mutex);
        }
        else if (g_sys.current_mA >= CURRENT_CRITICAL_MA && g_sys.current_mA < 10000.0f && !safety_tripped)
        {
            motor_emergency_stop();
            safety_tripped = true;
            safety_buzzer_alarm();
            osMutexAcquire(g_i2c_mutex, osWaitForever);
            i2c_bus_reset();
            osMutexRelease(g_i2c_mutex);
        }

        if (!flame && safety_tripped && g_sys.current_mA < CURRENT_CRITICAL_MA)
        {
            safety_buzzer_off();
        }

        osDelay(100);
    }
}

void DisplayTask(void *argument)
{
    osMutexAcquire(g_i2c_mutex, osWaitForever);
    ssd1306_init(&hi2c1);
    osMutexRelease(g_i2c_mutex);

    char line[22];

    for (;;)
    {
        int temp_i = (int)(g_sys.filtered_temperature * 10);
        int curr_i = (int)(g_sys.current_mA);

        const char *mode_str = (g_sys.system_mode == SYSTEM_MODE_ACTIVE)
                               ? "ACTIVE" : "POWER SAVE";
        const char *motor_str = (g_sys.motor_state == MOTOR_STATE_RUNNING) ? "RUNNING" :
                                (g_sys.motor_state == MOTOR_STATE_REDUCED) ? "REDUCED" : "STOPPED";

        const char *safety_str;
        if (g_sys.flame_detected)
            safety_str = "CRITICAL";
        else if (g_sys.current_mA >= CURRENT_CRITICAL_MA && g_sys.current_mA < 10000.0f)
            safety_str = "CRITICAL";
        else
            safety_str = "NORMAL";

        ssd1306_clear();

        ssd1306_set_cursor(0, 0);
        sprintf(line, "Mode: %s", mode_str);
        ssd1306_write_string(line);

        ssd1306_set_cursor(0, 10);
        sprintf(line, "Motor: %s", motor_str);
        ssd1306_write_string(line);

        ssd1306_set_cursor(0, 20);
        sprintf(line, "Temp: %d.%d C", temp_i/10, (temp_i<0?-temp_i:temp_i)%10);
        ssd1306_write_string(line);

        ssd1306_set_cursor(0, 30);
        sprintf(line, "I: %d mA", curr_i);
        ssd1306_write_string(line);

        ssd1306_set_cursor(0, 40);
        sprintf(line, "IR:%s Flm:%s",
                g_sys.ir_detected ? "YES" : "no",
                g_sys.flame_detected ? "YES!" : "no");
        ssd1306_write_string(line);

        ssd1306_set_cursor(0, 52);
        sprintf(line, "Safety: %s", safety_str);
        ssd1306_write_string(line);

        osMutexAcquire(g_i2c_mutex, osWaitForever);
        ssd1306_update();
        osMutexRelease(g_i2c_mutex);

        osDelay(500);
    }
}

void LoggingTask(void *argument)
{
    for (;;)
    {
        int lut_i  = (int)(g_sys.lut_temperature * 10);
        int filt_i = (int)(g_sys.filtered_temperature * 10);

        sprintf(log_buf, "[LOG] ADC:%d LUT:%d.%dC Filt:%d.%dC V:%dV I:%dmA IR:%s FL:%s M:%s P:%s\r\n",
            (int)g_sys.raw_adc,
            lut_i/10, (lut_i<0?-lut_i:lut_i)%10,
            filt_i/10, (filt_i<0?-filt_i:filt_i)%10,
            (int)g_sys.voltage_V,
            (int)g_sys.current_mA,
            g_sys.ir_detected ? "YES" : "NO",
            g_sys.flame_detected ? "YES" : "NO",
            (g_sys.motor_state == MOTOR_STATE_RUNNING) ? "RUN" :
            (g_sys.motor_state == MOTOR_STATE_REDUCED) ? "RED" : "STP",
            (g_sys.system_mode == SYSTEM_MODE_ACTIVE) ? "ACTIVE" : "PWRSAVE");

        HAL_UART_Transmit(&huart2, (uint8_t *)log_buf, strlen(log_buf), 500);
        osDelay(1000);
    }
}

void PowerManagerTask(void *argument)
{
    power_manager_init();

    for (;;)
    {
        if (power_manager_should_sleep())
        {
            power_manager_enter_sleep();
            osThreadSuspend(powerMgrTaskHandle);
        }

        osDelay(1000);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == IR_PIN || GPIO_Pin == FLAME_PIN || GPIO_Pin == USER_BTN_PIN_DEF)
    {
        g_sys.last_activity_tick = HAL_GetTick();
        BaseType_t xWoken = pdFALSE;
        xTaskNotifyFromISR((TaskHandle_t)safetyTaskHandle,
                           (uint32_t)GPIO_Pin,
                           eSetBits, &xWoken);
        portYIELD_FROM_ISR(xWoken);
    }
}

void MX_FREERTOS_Init(void)
{
    memset(&g_sys, 0, sizeof(g_sys));
    g_sys.system_mode = SYSTEM_MODE_ACTIVE;
    g_sys.motor_state = MOTOR_STATE_STOPPED;
    g_sys.safety_status = SAFETY_NORMAL;
    g_sys.last_activity_tick = HAL_GetTick();

    const osMutexAttr_t sys_m = { .name = "SysMutex" };
    g_sys_mutex = osMutexNew(&sys_m);

    const osMutexAttr_t i2c_m = { .name = "I2CMutex" };
    g_i2c_mutex = osMutexNew(&i2c_m);

    const osThreadAttr_t safety_a = { .name = "Safety", .stack_size = 512, .priority = osPriorityHigh };
    safetyTaskHandle = osThreadNew(SafetySupervisorTask, NULL, &safety_a);

    const osThreadAttr_t temp_a = { .name = "Temp", .stack_size = 512, .priority = osPriorityAboveNormal };
    tempTaskHandle = osThreadNew(TemperatureTask, NULL, &temp_a);

    const osThreadAttr_t pmon_a = { .name = "PwrMon", .stack_size = 512, .priority = osPriorityNormal };
    powerMonTaskHandle = osThreadNew(PowerMonitorTask, NULL, &pmon_a);

    const osThreadAttr_t motor_a = { .name = "Motor", .stack_size = 512, .priority = osPriorityNormal };
    motorTaskHandle = osThreadNew(MotorControlTask, NULL, &motor_a);

    const osThreadAttr_t pmgr_a = { .name = "PwrMgr", .stack_size = 512, .priority = osPriorityBelowNormal };
    powerMgrTaskHandle = osThreadNew(PowerManagerTask, NULL, &pmgr_a);

    const osThreadAttr_t disp_a = { .name = "Display", .stack_size = 512, .priority = osPriorityLow };
    displayTaskHandle = osThreadNew(DisplayTask, NULL, &disp_a);

    const osThreadAttr_t log_a = { .name = "Logging", .stack_size = 2048, .priority = osPriorityLow };
    loggingTaskHandle = osThreadNew(LoggingTask, NULL, &log_a);
}

/* USER CODE END Application */

