#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// ST7735S 引脚定义
#define TFT_SCL    18    // SCL/SCK引脚
#define TFT_SDA    3     // SDA/MOSI引脚
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

// 按键定义
#define KEY1_PIN   35    // K1按键
#define KEY2_PIN   36    // K2按键
#define KEY3_PIN   37    // K3按键
#define KEY4_PIN   38    // K4按键

// DS18B20引脚定义
const int oneWireBus = 4;     // 使用GPIO4作为数据线

// 全局变量
int selectedSensor = -1;      // 当前选择的传感器，-1表示显示所有
int totalSensors = 0;         // 总传感器数量
bool showAll = true;          // 是否显示所有传感器
unsigned long lastButtonCheck = 0; // 上次按键检查时间
const int debounceTime = 200;  // 按键防抖时间（毫秒）

// 初始化显示屏
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// 按键检测函数
void checkButtons() {
  if (millis() - lastButtonCheck < debounceTime) return;
  
  if (digitalRead(KEY1_PIN) == LOW) {  // K1: 切换传感器
    showAll = !showAll;
    if (!showAll) {
      selectedSensor = (selectedSensor + 1) % totalSensors;
    }
    lastButtonCheck = millis();
  }
  
  if (digitalRead(KEY2_PIN) == LOW) {  // K2: 保留用于其他功能
    lastButtonCheck = millis();
  }
  
  if (digitalRead(KEY3_PIN) == LOW) {  // K3: 保留用于其他功能
    lastButtonCheck = millis();
  }
  
  if (digitalRead(KEY4_PIN) == LOW) {  // K4: 保留用于其他功能
    lastButtonCheck = millis();
  }
}

// 显示单个传感器数据
void displaySingleSensor(int index) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 5);
  tft.printf("传感器 %d 详细信息", index + 1);
  
  float tempC = sensors.getTempCByIndex(index);
  if (tempC != DEVICE_DISCONNECTED_C) {
    // 大号显示温度
    tft.setTextSize(3);
    tft.setCursor(15, 40);
    tft.setTextColor(ST77XX_GREEN);
    tft.printf("%.1f", tempC);
    
    // 显示摄氏度符号
    tft.setTextSize(2);
    tft.setCursor(90, 40);
    tft.print("C");
    
    // 显示额外信息
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(2, 80);
    tft.println("按K1切换传感器");
    tft.setCursor(2, 95);
    tft.println("按K1长按返回总览");
  } else {
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_RED);
    tft.setCursor(15, 40);
    tft.println("错误");
  }
}

// 显示所有传感器数据
void displayAllSensors() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 5);
  tft.printf("温度监测 (%d个传感器)", totalSensors);
  
  for(int i = 0; i < totalSensors; i++) {
    float tempC = sensors.getTempCByIndex(i);
    if(tempC != DEVICE_DISCONNECTED_C) {
      tft.setTextSize(2);
      tft.setCursor(2, 20 + i * 25);
      tft.setTextColor(ST77XX_GREEN);
      tft.printf("T%d:%.1fC", i + 1, tempC);
    } else {
      tft.setTextSize(2);
      tft.setCursor(2, 20 + i * 25);
      tft.setTextColor(ST77XX_RED);
      tft.printf("T%d:错误", i + 1);
    }
  }
  
  // 显示操作提示
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(2, 110);
  tft.println("按K1选择传感器");
}

void setup() {
  // 初始化串口
  Serial.begin(115200);
  
  // 初始化按键
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(KEY4_PIN, INPUT_PULLUP);
  
  // 初始化SPI
  SPI.begin(TFT_SCL, -1, TFT_SDA, TFT_CS);
  
  // 初始化显示屏
  tft.initR(INITR_GREENTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  
  // 初始化传感器
  sensors.begin();
  delay(1000);
  
  // 获取传感器数量
  sensors.requestTemperatures();
  totalSensors = sensors.getDeviceCount();
  Serial.printf("找到 %d 个传感器\n", totalSensors);
}

void loop() {
  checkButtons();  // 检查按键状态
  sensors.requestTemperatures();
  
  if (showAll) {
    displayAllSensors();
  } else {
    displaySingleSensor(selectedSensor);
  }
  
  delay(100);  // 短暂延时，避免刷新太快
}