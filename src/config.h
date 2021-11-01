#pragma once
#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN 0x10
#endif

#define TFT_BL 4 // Display backlight control pi

#define ADC_EN 14
#define ADC_PIN 34
#define BUZZER_PIN 15

#define MAIN_BTN_PIN GPIO_NUM_12
#define BTN1_PIN GPIO_NUM_0
#define BTN2_PIN GPIO_NUM_35
#define SECOND_BTN_PIN GPIO_NUM_2
#define SECOND_BTN_R_PIN GPIO_NUM_17
#define SECOND_BTN_G_PIN GPIO_NUM_32
#define SECOND_BTN_B_PIN GPIO_NUM_33

#define DEBUG_PORT Serial // Serial debug port

#ifdef DEBUG_PORT
#define DEBUG_MSG(...) DEBUG_PORT.printf(__VA_ARGS__)
#else
#define DEBUG_MSG(...)
#endif

#define MIN_BAT_MILLIVOLTS 3250 // millivolts. 10% per https://blog.ampow.com/lipo-voltage-chart/

#define BAT_MILLIVOLTS_FULL 4100
#define BAT_MILLIVOLTS_EMPTY 3500

#define USE_I2S 1

#define AS_IDLE 0
#define AS_MEDIC 1
#define AS_MEDIC_DOWN 2
#define AS_SETUP 3

#define IDLE_TIME_OFFSET 0
#define MEDIC_TIME_OFFSET 4
#define REVIVALS_COUNT_OFFSET 8
#define FLAGS_OFFSET 12
#define STARTUP_PIN_OFFSET  16
#define SETUP_PIN_OFFSET  20
#define SFX_DELAY_OFFSET  24
#define MEDIC_DOWN_DELAY_OFFSET 28
#define MEDIC_DOWN_TIME_OFFSET  32
#define CURRENT_REVIVALS_OFFSET 60


#define SOFTWARE_VERSION_MAJOR 1
#define SOFTWARE_VERSION_MINOR 0
#define SOFTWARE_VERSION_PATCH 1
#define HARDWARE_VERSION_MAJOR 0
#define HARDWARE_VERSION_MINOR 0

#define USE_I2C 1
#define I2C_SDA 21
#define I2C_SCL 22

#define I2S_BCLK 27
#define I2S_WCLK 26
#define I2S_DOUT 15

#define USE_ST25DV  1
#define ST25DV_GPIO GPIO_NUM_13
