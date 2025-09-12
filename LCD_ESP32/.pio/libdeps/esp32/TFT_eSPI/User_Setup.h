//                            USER DEFINED SETTINGS
//   Set driver type, fonts to be loaded, pins used and SPI control method etc.

// User defined information reported by "Read_User_Setup" test & diagnostics example
#define USER_SETUP_INFO "ESP32 + ST7735R (Adafruit 1.44 TFT)"

// ###########################################################################
// Section 1. Driver selection
// ###########################################################################

#define ST7735_DRIVER      // Adafruit 1.44" TFT uses ST7735R
#define ST7735_GREENTAB    // Most common tab type for 1.44" 128x128
#define TFT_RGB_ORDER TFT_BGR  // Adafruit ST7735R uses BGR order

// ###########################################################################
// Section 2. Pin configuration for ESP32
// ###########################################################################

// SPI pins (VSPI on ESP32 DevKit v1)
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS    5   // Chip select control pin
#define TFT_DC    2   // Data/Command control pin
#define TFT_RST   4   // Reset pin (or connect to RST)

// Backlight (Adafruit board has onboard transistor, so usually tied to VCC)
// #define TFT_BL   22
// #define TFT_BACKLIGHT_ON HIGH

// ###########################################################################
// Section 3. Fonts to be loaded
// ###########################################################################

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ###########################################################################
// Section 4. SPI frequency settings
// ###########################################################################

// ST7735R is stable up to 27MHz. Use 20–27MHz for best performance.
#define SPI_FREQUENCY  27000000

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY  20000000

// Touch not used in Adafruit 1.44", so skip TOUCH_CS
// #define TOUCH_CS 21

