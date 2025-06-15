// See SetupX_Template.h for all options available
#define USER_SETUP_ID 2

#define ST7735_DRIVER


#define TFT_WIDTH  128
#define TFT_HEIGHT 128


#define ST7735_GREENTAB
// #define ST7735_INITB
// #define ST7735_GREENTAB
// #define ST7735_GREENTAB2
// #define ST7735_GREENTAB3
// #define ST7735_GREENTAB128    // For 128 x 128 display
// #define ST7735_GREENTAB160x80 // For 160 x 80 display (BGR, inverted, 26 offset)
// #define ST7735_ROBOTLCD       // For some RobotLCD Arduino shields (128x160, BGR, https://docs.arduino.cc/retired/getting-started-guides/TFT)
// #define ST7735_REDTAB
// #define ST7735_BLACKTAB
// #define ST7735_REDTAB160x80  
// For ST7735, ST7789 and ILI9341 ONLY, define the colour order IF the blue and red are swapped on your display
// Try ONE option at a time to find the correct colour order for your display

//  #define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//  #define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red

// For NodeMCU - use pin numbers in the form PIN_Dx where Dx is the NodeMCU pin designation
// #define TFT_CS   PIN_D8  // Chip select control pin D8
// #define TFT_DC   PIN_D3  // Data Command control pin
// #define TFT_RST  PIN_D4  // Reset pin (could connect to NodeMCU RST, see next line)
//#define TFT_RST  -1    // Set TFT_RST to -1 if the display RESET is connected to NodeMCU RST or 3.3V
#define TFT_MISO -1
#define TFT_MOSI 3    // SDA/MOSI引脚
#define TFT_SCLK 18   // SCL/SCK引脚
#define TFT_CS   10   // CS片选引脚
#define TFT_DC   8    // DC数据/命令选择引脚
#define TFT_RST  9    // RES复位引脚
#define TFT_BL   17   // 背光控制引脚

#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:.
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
//#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
#define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

#define SMOOTH_FONT


// #define SPI_FREQUENCY  20000000
#define SPI_FREQUENCY  27000000
// #define SPI_FREQUENCY  40000000

#define SPI_TOUCH_FREQUENCY  2500000


// // #define SUPPORT_TRANSACTIONS
// #define TFT_ROTATION 1

// 定义是否使用硬件SPI
#define USE_HSPI_PORT

// // 启用双缓冲
// #define DMA_TO_DRIVER
// #define BUFFER_SIZE 128

// // 定义是否使用ESP32的SPI DMA
// #define DMA_TO_DRIVER

// // 定义是否使用双缓冲
// #define BUFFER_SIZE 128 