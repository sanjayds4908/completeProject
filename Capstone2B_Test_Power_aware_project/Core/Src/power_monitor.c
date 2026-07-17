/**
  ******************************************************************************
  * @file    power_monitor.c
  * @brief   INA219 I2C Driver — Voltage, Current, Power
  *          Fixed: hardware I2C bus reset for STM32F4 BUSY flag bug
  ******************************************************************************
  */

#include "power_monitor.h"

#define REG_CONFIG   0x00
#define REG_BUS_V    0x02
#define REG_CURRENT  0x04
#define REG_POWER    0x03
#define REG_CALIB    0x05

#define I2C_TIMEOUT  50

/* Hardware I2C bus reset — toggles SCL via GPIO to unstick SDA
 * This is the ONLY reliable fix for STM32F4 I2C BUSY flag bug */
void i2c_bus_reset(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. Disable I2C peripheral */
    __HAL_I2C_DISABLE(&hi2c1);
    HAL_I2C_DeInit(&hi2c1);

    /* 2. Configure SCL (PB8) and SDA (PB9) as open-drain GPIO */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 3. Release SDA high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);

    /* 4. Toggle SCL 16 times to free stuck slave */
    for (int i = 0; i < 16; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_Delay(1);
    }

    /* 5. Generate STOP: SDA low then high while SCL high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(1);

    /* 6. Reconfigure pins back to I2C alternate function */
    GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 7. Re-init I2C */
    HAL_I2C_Init(&hi2c1);
}

static void i2c_recover(void)
{
    if (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY)
    {
        i2c_bus_reset();
    }
}

static HAL_StatusTypeDef write_reg(uint8_t reg, uint16_t val)
{
    uint8_t buf[2] = {(val >> 8) & 0xFF, val & 0xFF};
    HAL_StatusTypeDef s = HAL_I2C_Mem_Write(&hi2c1, INA219_ADDR, reg,
                                             I2C_MEMADD_SIZE_8BIT, buf, 2, I2C_TIMEOUT);
    if (s != HAL_OK) i2c_recover();
    return s;
}

static int16_t read_reg(uint8_t reg)
{
    uint8_t buf[2] = {0};
    HAL_StatusTypeDef s = HAL_I2C_Mem_Read(&hi2c1, INA219_ADDR, reg,
                                            I2C_MEMADD_SIZE_8BIT, buf, 2, I2C_TIMEOUT);
    if (s != HAL_OK)
    {
        i2c_recover();
        return 0;
    }
    return (int16_t)((buf[0] << 8) | buf[1]);
}

HAL_StatusTypeDef ina219_init(void)
{
    HAL_StatusTypeDef s = write_reg(REG_CONFIG, 0x399F);
    if (s != HAL_OK) return s;
    return write_reg(REG_CALIB, 4096);
}

float ina219_read_voltage(void)
{
    return (float)((read_reg(REG_BUS_V) >> 3) * 4) / 1000.0f;
}

//float ina219_read_current(void)
//{
//    int16_t raw = read_reg(REG_CURRENT);
//    float current_A = (float)raw * 0.00001f;
//    if (current_A < 0) current_A = -current_A;
//    return current_A * 1000.0f;
	float ina219_read_current(void)
	{
	    int16_t raw = read_reg(REG_CURRENT);
	    float current_mA = (float)raw * 0.1f;
	    if (current_mA < 0) current_mA = -current_mA;
	    return current_mA;
	}


float ina219_read_power(void)
{
    return (float)read_reg(REG_POWER) * 20.0f;
}
