#include "bmp280.h"

#define BMP280_REG_CALIBRATION 0x88U
#define BMP280_REG_CHIP_ID     0xD0U
#define BMP280_REG_CONFIG      0xF5U
#define BMP280_REG_CTRL_MEAS   0xF4U
#define BMP280_REG_PRESSURE    0xF7U
#define BMP280_CHIP_ID         0x58U
#define BMP280_IO_TIMEOUT_MS   100U

static uint16_t read_u16_le(const uint8_t *data)
{
  return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static int16_t read_s16_le(const uint8_t *data)
{
  return (int16_t)read_u16_le(data);
}

static void spi_select(const bmp280_t *sensor)
{
  HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin, GPIO_PIN_RESET);
}

static void spi_deselect(const bmp280_t *sensor)
{
  HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin, GPIO_PIN_SET);
}

static bmp280_status_t read_registers(bmp280_t *sensor,
                                      uint8_t reg,
                                      uint8_t *data,
                                      uint16_t length)
{
  if ((sensor == NULL) || (data == NULL) || (length == 0U)) {
    return BMP280_ERROR_ARGUMENT;
  }

  if (sensor->bus == BMP280_BUS_I2C) {
    if (HAL_I2C_Mem_Read(sensor->i2c, sensor->i2c_address, reg,
                         I2C_MEMADD_SIZE_8BIT, data, length,
                         BMP280_IO_TIMEOUT_MS) != HAL_OK) {
      return BMP280_ERROR_COMMUNICATION;
    }
    return BMP280_OK;
  }

  uint8_t command = reg | 0x80U;
  uint8_t dummy = 0xFFU;
  spi_select(sensor);
  HAL_StatusTypeDef status = HAL_SPI_Transmit(sensor->spi, &command, 1U,
                                               BMP280_IO_TIMEOUT_MS);
  if (status == HAL_OK) {
    for (uint16_t i = 0U; i < length; ++i) {
      status = HAL_SPI_TransmitReceive(sensor->spi, &dummy, &data[i], 1U,
                                      BMP280_IO_TIMEOUT_MS);
      if (status != HAL_OK) {
        break;
      }
    }
  }
  spi_deselect(sensor);

  return (status == HAL_OK) ? BMP280_OK : BMP280_ERROR_COMMUNICATION;
}

static bmp280_status_t write_register(bmp280_t *sensor,
                                      uint8_t reg,
                                      uint8_t value)
{
  if (sensor->bus == BMP280_BUS_I2C) {
    return (HAL_I2C_Mem_Write(sensor->i2c, sensor->i2c_address, reg,
                              I2C_MEMADD_SIZE_8BIT, &value, 1U,
                              BMP280_IO_TIMEOUT_MS) == HAL_OK)
               ? BMP280_OK
               : BMP280_ERROR_COMMUNICATION;
  }

  uint8_t data[2] = {(uint8_t)(reg & 0x7FU), value};
  spi_select(sensor);
  HAL_StatusTypeDef status = HAL_SPI_Transmit(sensor->spi, data, 2U,
                                               BMP280_IO_TIMEOUT_MS);
  spi_deselect(sensor);
  return (status == HAL_OK) ? BMP280_OK : BMP280_ERROR_COMMUNICATION;
}

static bool calibration_is_empty(const uint8_t *data, uint16_t length)
{
  for (uint16_t i = 0U; i < length; ++i) {
    if (data[i] != 0U) {
      return false;
    }
  }
  return true;
}

static void decode_calibration(bmp280_t *sensor, const uint8_t *data)
{
  sensor->calibration.dig_t1 = read_u16_le(&data[0]);
  sensor->calibration.dig_t2 = read_s16_le(&data[2]);
  sensor->calibration.dig_t3 = read_s16_le(&data[4]);
  sensor->calibration.dig_p1 = read_u16_le(&data[6]);
  sensor->calibration.dig_p2 = read_s16_le(&data[8]);
  sensor->calibration.dig_p3 = read_s16_le(&data[10]);
  sensor->calibration.dig_p4 = read_s16_le(&data[12]);
  sensor->calibration.dig_p5 = read_s16_le(&data[14]);
  sensor->calibration.dig_p6 = read_s16_le(&data[16]);
  sensor->calibration.dig_p7 = read_s16_le(&data[18]);
  sensor->calibration.dig_p8 = read_s16_le(&data[20]);
  sensor->calibration.dig_p9 = read_s16_le(&data[22]);
}

static bmp280_status_t init_common(bmp280_t *sensor)
{
  uint8_t chip_id = 0U;
  bmp280_status_t status = read_registers(sensor, BMP280_REG_CHIP_ID,
                                          &chip_id, 1U);
  if (status != BMP280_OK) {
    return status;
  }
  if (chip_id != BMP280_CHIP_ID) {
    return BMP280_ERROR_CHIP_ID;
  }

  uint8_t calibration[24];
  status = read_registers(sensor, BMP280_REG_CALIBRATION,
                          calibration, sizeof(calibration));
  if (status != BMP280_OK) {
    return status;
  }

  /* The Wokwi custom chip exposes direct Pa data and no Bosch trim values. */
  sensor->simulator_encoding = calibration_is_empty(calibration,
                                                     sizeof(calibration));
  if (!sensor->simulator_encoding) {
    decode_calibration(sensor, calibration);
    if (sensor->calibration.dig_p1 == 0U) {
      return BMP280_ERROR_CALIBRATION;
    }
  }

  status = write_register(sensor, BMP280_REG_CONFIG, 0x10U);
  if (status == BMP280_OK) {
    /* Temperature x1, pressure x1, normal mode. */
    status = write_register(sensor, BMP280_REG_CTRL_MEAS, 0x27U);
  }
  if (status == BMP280_OK) {
    sensor->initialized = true;
  }
  return status;
}

bmp280_status_t bmp280_init_i2c(bmp280_t *sensor,
                                I2C_HandleTypeDef *i2c,
                                uint8_t address_7bit)
{
  if ((sensor == NULL) || (i2c == NULL)) {
    return BMP280_ERROR_ARGUMENT;
  }
  *sensor = (bmp280_t){0};
  sensor->bus = BMP280_BUS_I2C;
  sensor->i2c = i2c;
  sensor->i2c_address = (uint16_t)address_7bit << 1;
  return init_common(sensor);
}

bmp280_status_t bmp280_init_spi(bmp280_t *sensor,
                                SPI_HandleTypeDef *spi,
                                GPIO_TypeDef *cs_port,
                                uint16_t cs_pin)
{
  if ((sensor == NULL) || (spi == NULL) || (cs_port == NULL)) {
    return BMP280_ERROR_ARGUMENT;
  }
  *sensor = (bmp280_t){0};
  sensor->bus = BMP280_BUS_SPI;
  sensor->spi = spi;
  sensor->cs_port = cs_port;
  sensor->cs_pin = cs_pin;
  spi_deselect(sensor);
  HAL_Delay(2U);
  return init_common(sensor);
}

static bmp280_status_t compensate_pressure(const bmp280_t *sensor,
                                           int32_t adc_temperature,
                                           int32_t adc_pressure,
                                           uint32_t *pressure_pa)
{
  const bmp280_calibration_t *c = &sensor->calibration;
  int32_t var1_t = ((((adc_temperature >> 3) -
                      ((int32_t)c->dig_t1 << 1))) *
                    (int32_t)c->dig_t2) >> 11;
  int32_t var2_t = (((((adc_temperature >> 4) - (int32_t)c->dig_t1) *
                       ((adc_temperature >> 4) - (int32_t)c->dig_t1)) >> 12) *
                     (int32_t)c->dig_t3) >> 14;
  int32_t t_fine = var1_t + var2_t;

  int64_t var1 = (int64_t)t_fine - 128000;
  int64_t var2 = var1 * var1 * (int64_t)c->dig_p6;
  var2 += (var1 * (int64_t)c->dig_p5) << 17;
  var2 += ((int64_t)c->dig_p4) << 35;
  var1 = ((var1 * var1 * (int64_t)c->dig_p3) >> 8) +
         ((var1 * (int64_t)c->dig_p2) << 12);
  var1 = (((((int64_t)1) << 47) + var1) * (int64_t)c->dig_p1) >> 33;
  if (var1 == 0) {
    return BMP280_ERROR_CALIBRATION;
  }

  int64_t pressure = 1048576 - adc_pressure;
  pressure = (((pressure << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)c->dig_p9 * (pressure >> 13) * (pressure >> 13)) >> 25;
  var2 = ((int64_t)c->dig_p8 * pressure) >> 19;
  pressure = ((pressure + var1 + var2) >> 8) +
             ((int64_t)c->dig_p7 << 4);
  if (pressure <= 0) {
    return BMP280_ERROR_DATA;
  }
  *pressure_pa = (uint32_t)(pressure >> 8);
  return BMP280_OK;
}

bmp280_status_t bmp280_read_pressure_pa(bmp280_t *sensor,
                                        uint32_t *pressure_pa)
{
  if ((sensor == NULL) || (pressure_pa == NULL) || !sensor->initialized) {
    return BMP280_ERROR_ARGUMENT;
  }

  uint8_t raw[6];
  bmp280_status_t status = read_registers(sensor, BMP280_REG_PRESSURE,
                                          raw, sizeof(raw));
  if (status != BMP280_OK) {
    return status;
  }

  int32_t adc_pressure = ((int32_t)raw[0] << 12) |
                         ((int32_t)raw[1] << 4) |
                         ((int32_t)raw[2] >> 4);
  int32_t adc_temperature = ((int32_t)raw[3] << 12) |
                            ((int32_t)raw[4] << 4) |
                            ((int32_t)raw[5] >> 4);

  if (sensor->simulator_encoding) {
    *pressure_pa = (uint32_t)adc_pressure >> 3;
    return BMP280_OK;
  }
  return compensate_pressure(sensor, adc_temperature, adc_pressure,
                             pressure_pa);
}
