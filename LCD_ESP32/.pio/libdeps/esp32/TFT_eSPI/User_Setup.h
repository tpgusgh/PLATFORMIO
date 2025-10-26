#define ILI9488_DRIVER
#define TFT_RGB_ORDER TFT_RGB

#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   27
#define TFT_RST  26
#define TFT_BL   -1  // 백라이트는 3.3V 직결

#define SPI_FREQUENCY  27000000  // 27MHz 안정적
#define SPI_READ_FREQUENCY  20000000
