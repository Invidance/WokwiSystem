#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define BMP280_CHIP_ID_REG     0xD0
#define BMP280_RESET_REG       0xE0
#define BMP280_CTRL_MEAS_REG   0xF4
#define BMP280_CONFIG_REG      0xF5

#define BMP280_PRESS_MSB       0xF7
#define BMP280_PRESS_LSB       0xF8
#define BMP280_PRESS_XLSB      0xF9

#define BMP280_TEMP_MSB        0xFA
#define BMP280_TEMP_LSB        0xFB
#define BMP280_TEMP_XLSB       0xFC

#define BMP280_CHIP_ID         0x58

typedef struct {
  // Register state
  uint8_t regs[256];
  uint8_t reg_ptr;

  // I2C
  i2c_dev_t i2c;
  bool i2c_expect_register;

  // SPI
  spi_dev_t spi;
  pin_t cs;
  uint8_t spi_buffer;
  bool spi_first_byte;
  bool spi_read;
  uint8_t spi_reg;

  // Simulation
  timer_t update_timer;

  uint32_t pressure_attr;
  uint32_t noise_attr;
  uint32_t spike_attr;

  uint32_t tick;
} chip_state_t;


// ---------------------------------------------------------
// Synthetic sensor model
// ---------------------------------------------------------

static void update_sensor_data(chip_state_t *chip) {
  uint32_t base_pressure = attr_read(chip->pressure_attr);
  uint32_t noise = attr_read(chip->noise_attr);
  uint32_t spike = attr_read(chip->spike_attr);

  chip->tick++;

  // deterministic triangle-wave "noise"
  int32_t phase = chip->tick % 20;
  int32_t delta;

  if (phase < 10) {
    delta = phase;
  } else {
    delta = 20 - phase;
  }

  delta -= 5;

  int32_t pressure = (int32_t)base_pressure;

  if (noise > 0) {
    pressure += (delta * (int32_t)noise) / 5;
  }

  // Every ~10 seconds simulate one spike if enabled
  if (spike > 0 && (chip->tick % 100) == 0) {
    pressure += 5000; // +50 hPa spike
  }

  if (pressure < 30000) {
    pressure = 30000;
  }

  if (pressure > 120000) {
    pressure = 120000;
  }

  /*
   * Simplified raw pressure encoding.
   *
   * This is NOT Bosch calibration math yet.
   * We simply map pressure in Pa into a 20-bit raw register value,
   * so firmware can read changing sensor registers.
   */
  uint32_t raw_pressure = (uint32_t)pressure << 3;

  chip->regs[BMP280_PRESS_MSB] =
      (raw_pressure >> 16) & 0xFF;

  chip->regs[BMP280_PRESS_LSB] =
      (raw_pressure >> 8) & 0xFF;

  chip->regs[BMP280_PRESS_XLSB] =
      raw_pressure & 0xF0;

  // fixed synthetic temperature raw value
  uint32_t raw_temp = 25U << 12;

  chip->regs[BMP280_TEMP_MSB] =
      (raw_temp >> 16) & 0xFF;

  chip->regs[BMP280_TEMP_LSB] =
      (raw_temp >> 8) & 0xFF;

  chip->regs[BMP280_TEMP_XLSB] =
      raw_temp & 0xF0;
}


static void sensor_timer_callback(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  update_sensor_data(chip);
}


// ---------------------------------------------------------
// Register access
// ---------------------------------------------------------

static uint8_t read_register(chip_state_t *chip, uint8_t reg) {
  return chip->regs[reg];
}


static void write_register(
    chip_state_t *chip,
    uint8_t reg,
    uint8_t value
) {
  switch (reg) {
    case BMP280_RESET_REG:
      if (value == 0xB6) {
        chip->regs[BMP280_CTRL_MEAS_REG] = 0;
        chip->regs[BMP280_CONFIG_REG] = 0;
      }
      break;

    case BMP280_CTRL_MEAS_REG:
    case BMP280_CONFIG_REG:
      chip->regs[reg] = value;
      break;

    default:
      // Ignore writes to read-only / unimplemented registers
      break;
  }
}


// ---------------------------------------------------------
// I2C
// ---------------------------------------------------------

static bool i2c_connect(
    void *user_data,
    uint32_t address,
    bool read
) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (!read) {
    chip->i2c_expect_register = true;
  }

  return true;
}


static uint8_t i2c_read_byte(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  uint8_t value =
      read_register(chip, chip->reg_ptr);

  chip->reg_ptr++;

  return value;
}


static bool i2c_write_byte(
    void *user_data,
    uint8_t data
) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->i2c_expect_register) {
    chip->reg_ptr = data;
    chip->i2c_expect_register = false;

    return true;
  }

  write_register(chip, chip->reg_ptr, data);

  chip->reg_ptr++;

  return true;
}


static void i2c_disconnect(void *user_data) {
  // Register pointer intentionally preserved:
  // HAL_I2C_Mem_Read often performs write-register-address
  // followed by repeated-start read.
}


// ---------------------------------------------------------
// SPI
// ---------------------------------------------------------

static void spi_done(
    void *user_data,
    uint8_t *buffer,
    uint32_t count
) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (count == 0) {
    return;
  }

  uint8_t incoming = buffer[0];

  if (chip->spi_first_byte) {

    /*
     * BMP280 SPI convention:
     * bit 7 = 1 -> read
     * bit 7 = 0 -> write
     */

    chip->spi_read =
        (incoming & 0x80) != 0;

    chip->spi_reg =
        incoming & 0x7F;

    chip->spi_first_byte = false;

    if (chip->spi_read) {
      chip->spi_buffer =
          read_register(chip, chip->spi_reg);
    } else {
      chip->spi_buffer = 0;
    }

  } else {

    if (chip->spi_read) {
      chip->spi_reg++;

      chip->spi_buffer =
          read_register(chip, chip->spi_reg);

    } else {
      write_register(
          chip,
          chip->spi_reg,
          incoming
      );

      chip->spi_reg++;
      chip->spi_buffer = 0;
    }
  }

  if (pin_read(chip->cs) == LOW) {
    spi_start(
        chip->spi,
        &chip->spi_buffer,
        1
    );
  }
}


static void cs_changed(
    void *user_data,
    pin_t pin,
    uint32_t value
) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (value == LOW) {
    chip->spi_first_byte = true;
    chip->spi_buffer = 0;

    spi_start(
        chip->spi,
        &chip->spi_buffer,
        1
    );

  } else {
    spi_stop(chip->spi);
  }
}


// ---------------------------------------------------------
// Chip initialization
// ---------------------------------------------------------

void chip_init(void) {
  chip_state_t *chip =
      calloc(1, sizeof(chip_state_t));

  // -------------------------------------------------------
  // Attributes
  // -------------------------------------------------------

  chip->pressure_attr =
      attr_init("pressure", 101325);

  chip->noise_attr =
      attr_init("noise", 30);

  chip->spike_attr =
      attr_init("spike", 0);


  // -------------------------------------------------------
  // Registers
  // -------------------------------------------------------

  chip->regs[BMP280_CHIP_ID_REG] =
      BMP280_CHIP_ID;

  chip->regs[BMP280_CTRL_MEAS_REG] =
      0;

  chip->regs[BMP280_CONFIG_REG] =
      0;

  update_sensor_data(chip);


  // -------------------------------------------------------
  // I2C initialization
  // -------------------------------------------------------

  i2c_config_t i2c_config = {
      .address = 0x76,

      .scl =
          pin_init("SCL", INPUT_PULLUP),

      .sda =
          pin_init("SDA", INPUT_PULLUP),

      .connect =
          i2c_connect,

      .read =
          i2c_read_byte,

      .write =
          i2c_write_byte,

      .disconnect =
          i2c_disconnect,

      .user_data =
          chip,
  };

  chip->i2c =
      i2c_init(&i2c_config);


  // -------------------------------------------------------
  // SPI initialization
  // -------------------------------------------------------

  chip->cs =
      pin_init("CS", INPUT_PULLUP);

  spi_config_t spi_config = {
      .sck =
          pin_init("SCK", INPUT),

      .mosi =
          pin_init("SDI", INPUT),

      .miso =
          pin_init("SDO", INPUT),

      .mode = 0,

      .done =
          spi_done,

      .user_data =
          chip,
  };

  chip->spi =
      spi_init(&spi_config);


  pin_watch_config_t cs_watch = {
      .edge = BOTH,

      .pin_change =
          cs_changed,

      .user_data =
          chip,
  };

  pin_watch(
      chip->cs,
      &cs_watch
  );


  // -------------------------------------------------------
  // Sensor update timer: 100ms
  // -------------------------------------------------------

  timer_config_t timer_config = {
      .callback =
          sensor_timer_callback,

      .user_data =
          chip,
  };

  chip->update_timer =
      timer_init(&timer_config);

  timer_start(
      chip->update_timer,
      100000,
      true
  );


  printf(
      "BMP280 simulator started\n"
  );
}