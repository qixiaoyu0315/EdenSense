#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ST7735S 引脚定义
#define TFT_SCL    18    // SCL/SCK引脚
#define TFT_SDA    3    // SDA/MOSI引脚
#define TFT_CS     10    // CS片选引脚
#define TFT_RST    9     // RES复位引脚
#define TFT_DC     8     // DC数据/命令选择引脚
// BLK -> 3.3V   (背光控制)
// VCC -> 3.3V   (电源)
// GND -> GND    (地线)

// 屏幕尺寸和偏移量设置
#define ST7735_XSTART 2   // X轴偏移量
#define ST7735_YSTART 3   // Y轴偏移量
#define ST7735_WIDTH  128 // 屏幕宽度
#define ST7735_HEIGHT 128 // 屏幕高度

// DS18B20引脚定义
const int oneWireBus = 4;     // 使用GPIO4作为数据线

// 初始化显示屏
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

void setup() {
  // Start the Serial Monitor
  Serial.begin(115200);
  
  // 初始化SPI
  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);
  
  // 初始化ST7735S显示屏
  tft.initR(INITR_GREENTAB); // 尝试使用GREENTAB初始化
  
  // 设置显示区域和偏移量
  tft.setAddrWindow(ST7735_XSTART, ST7735_YSTART, 
                    ST7735_WIDTH - 1, ST7735_HEIGHT - 1);
                    
  tft.setRotation(1);        // 旋转屏幕方向（0-3）
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextWrap(true);
  
  // 显示启动信息
  tft.setCursor(2, 3); // 使用偏移量设置起始位置
  
  // Start the DS18B20 sensor
  sensors.begin();
  
  // 等待传感器初始化
  delay(1000);
}

void loop() {
  // 清除显示区域
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(2, 3); // 使用偏移量设置起始位置
  tft.setTextSize(1);
  sensors.requestTemperatures(); 
  
  // 获取连接的传感器数量
  int deviceCount = sensors.getDeviceCount();
  Serial.printf("找到 %d 个传感器\n", deviceCount);
  
  // 读取每个传感器的温度
  for(int i = 0; i < deviceCount; i++) {
    float tempC = sensors.getTempCByIndex(i);
    if(tempC != DEVICE_DISCONNECTED_C) {
      // 在串口显示
      Serial.printf("传感器 %d 温度: %.2f ℃\n", i, tempC);
      
      // 在屏幕上显示
      tft.setTextSize(2);
      tft.setCursor(2, 25 + i * 30); // 调整了行间距和起始位置
      tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);
      tft.printf("T%d:%.1fC", i + 1, tempC);
    } else {
      Serial.printf("传感器 %d 读取错误\n", i);
      tft.setTextSize(2);
      tft.setCursor(2, 25 + i * 30);
      tft.setTextColor(ST77XX_RED, ST77XX_BLACK);
    }
  }
  
  // 显示更新时间
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(2, ST7735_HEIGHT - 20);
  
  delay(5000); // 等待5秒后再次读取
}