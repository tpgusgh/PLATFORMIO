// ###########################################################################
// Section 1. Driver selection
// ###########################################################################

#define ILI9488_DRIVER          // ILI9488 드라이버 사용
#define TFT_RGB_ORDER TFT_RGB   // 색상 순서 (색 이상 시 TFT_BGR로 변경 가능)

// ###########################################################################
// Section 2. Pin configuration for ESP32
// ###########################################################################

#define TFT_MOSI 23             // SPI 데이터 출력 (TFT, SD 공용)
#define TFT_MISO 19             // SPI 데이터 입력 (SD 전용)
#define TFT_SCLK 18             // SPI 클럭 (TFT, SD 공용)
#define TFT_CS   5        // TFT Chip Select
#define TFT_DC   2              // TFT 데이터/명령 선택
#define TFT_RST  4              // TFT 리셋
#define TFT_BL   13             // 백라이트 핀 (모듈에 따라 VCC 직결 가능)

// SD 카드 핀 (SPI 공유)
#define SD_CS   15               // SD 카드 Chip Select

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

#define SPI_FREQUENCY       27000000  // TFT용 SPI 최대 속도
#define SPI_READ_FREQUENCY  20000000  // 읽기용 SPI 속도
