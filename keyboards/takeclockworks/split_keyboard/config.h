#pragma once
#define DEBUG_ENABLE
// driver
#define I2C_DRIVER I2CD1

// pins (XIAO RP2040: D4=GP6, D5=GP7)
#define I2C1_SDA_PIN GP6
#define I2C1_SCL_PIN GP7

// Some QMK versions use these generic names
#define I2C_SDA_PIN  GP6
#define I2C_SCL_PIN  GP7

// speed (まずは100k)
#define I2C1_CLOCK_SPEED 100000
#define I2C_CLOCK_SPEED  100000
