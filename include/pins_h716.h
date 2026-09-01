#ifndef PINS_H716_H
#define PINS_H716_H

// LilyGo EPD47 ESP32-S3 (H716) Pin Definitions

// E-Paper Interface
#define H716_EPD_CFG_DATA 13
#define H716_EPD_CFG_CLK  12
#define H716_EPD_CFG_STR  0

#define H716_EPD_CKV      38
#define H716_EPD_STH      40
#define H716_EPD_CKH      41

#define H716_EPD_D0       8
#define H716_EPD_D1       1
#define H716_EPD_D2       2
#define H716_EPD_D3       3
#define H716_EPD_D4       4
#define H716_EPD_D5       5
#define H716_EPD_D6       6
#define H716_EPD_D7       7

// Peripherals
#define H716_BUTTON_PIN   21
#define H716_BATTERY_ADC  14

// I2C (RTC / Touch)
#define H716_I2C_SDA      18
#define H716_I2C_SCL      17
#define H716_TP_INT       47

// SD Card
#define H716_SD_MISO      16
#define H716_SD_MOSI      15
#define H716_SD_SCK       11
#define H716_SD_CS        42

#endif // PINS_H716_H
