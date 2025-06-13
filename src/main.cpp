#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT_eSPI.h>
#include <SPI.h>

// 按键定义
#define KEY1_PIN   35    // K1: 切换显示模式/确认
#define KEY2_PIN   36    // K2: 上翻
#define KEY3_PIN   37    // K3: 下翻
#define KEY4_PIN   38    // K4: 返回

// DS18B20引脚定义
const int oneWireBus = 4;     // 使用GPIO4作为数据线

// 全局变量
int selectedSensor = -1;      // 当前选择的传感器，-1表示显示所有
int totalSensors = 0;         // 总传感器数量
bool showAll = true;          // 是否显示所有传感器
unsigned long lastButtonCheck = 0; // 上次按键检查时间
const int debounceTime = 200;  // 按键防抖时间（毫秒）
unsigned long lastTempUpdate = 0;  // 上次温度更新时间
const int tempUpdateInterval = 1000;  // 温度更新间隔（毫秒）

// 显示模式枚举
enum DisplayMode {
  MODE_OVERVIEW,    // 总览模式
  MODE_DETAIL,      // 详情模式
  MODE_SETTINGS     // 设置模式
};

DisplayMode currentMode = MODE_OVERVIEW;

// 初始化显示屏
TFT_eSPI tft = TFT_eSPI();

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// 绘制标题栏
void drawHeader(const char* title) {
  tft.fillRect(0, 0, tft.width(), 20, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title, tft.width()/2, 10);
}

// 绘制底部状态栏
void drawFooter(const char* hint) {
  tft.fillRect(0, tft.height()-20, tft.width(), 20, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(hint, tft.width()/2, tft.height()-10);
}

// 按键检测函数
void checkButtons() {
  if (millis() - lastButtonCheck < debounceTime) return;
  
  if (digitalRead(KEY1_PIN) == LOW) {  // K1: 切换显示模式/确认
    switch(currentMode) {
      case MODE_OVERVIEW:
        currentMode = MODE_DETAIL;
        selectedSensor = 0;
        break;
      case MODE_DETAIL:
        currentMode = MODE_SETTINGS;
        break;
      case MODE_SETTINGS:
        currentMode = MODE_OVERVIEW;
        break;
    }
    lastButtonCheck = millis();
  }
  
  if (digitalRead(KEY2_PIN) == LOW) {  // K2: 上翻
    if (currentMode == MODE_DETAIL && selectedSensor > 0) {
      selectedSensor--;
    }
    lastButtonCheck = millis();
  }
  
  if (digitalRead(KEY3_PIN) == LOW) {  // K3: 下翻
    if (currentMode == MODE_DETAIL && selectedSensor < totalSensors-1) {
      selectedSensor++;
    }
    lastButtonCheck = millis();
  }
  
  if (digitalRead(KEY4_PIN) == LOW) {  // K4: 返回
    if (currentMode != MODE_OVERVIEW) {
      currentMode = MODE_OVERVIEW;
    }
    lastButtonCheck = millis();
  }
}

// 显示单个传感器数据
void displayDetailView(int index) {
  tft.fillScreen(TFT_BLACK);
  drawHeader("传感器详情");
  
  float tempC = sensors.getTempCByIndex(index);
  
  // 显示传感器编号
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("传感器 " + String(index + 1), tft.width()/2, 40);
  
  if (tempC != DEVICE_DISCONNECTED_C) {
    // 大号显示温度
    tft.setTextSize(4);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(tempC, 1), tft.width()/2, 80);
    
    // 显示摄氏度符号
    tft.setTextSize(2);
    tft.drawString("C", tft.width()/2 + 60, 80);
  } else {
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("传感器未连接", tft.width()/2, 80);
  }
  
  // 显示操作提示
  drawFooter("K2:上翻 K3:下翻 K4:返回");
}

// 显示所有传感器数据
void displayOverview() {
  tft.fillScreen(TFT_BLACK);
  drawHeader("温度监测系统");
  
  // 显示传感器数量
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("已连接传感器: " + String(totalSensors), 5, 25);
  
  // 显示所有传感器数据
  for(int i = 0; i < totalSensors; i++) {
    float tempC = sensors.getTempCByIndex(i);
    int yPos = 40 + i * 25;
    
    // 绘制传感器背景
    tft.fillRect(5, yPos-15, tft.width()-10, 20, TFT_DARKGREY);
    
    if(tempC != DEVICE_DISCONNECTED_C) {
      tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
      String tempStr = "T" + String(i + 1) + ": " + String(tempC, 1) + "C";
      tft.drawString(tempStr, 10, yPos);
    } else {
      tft.setTextColor(TFT_RED, TFT_DARKGREY);
      tft.drawString("T" + String(i + 1) + ": 未连接", 10, yPos);
    }
  }
  
  // 显示操作提示
  drawFooter("K1:查看详情 K4:设置");
}

// 显示设置界面
void displaySettings() {
  tft.fillScreen(TFT_BLACK);
  drawHeader("系统设置");
  
  // 显示系统信息
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  tft.drawString("系统版本: 1.0.0", 10, 30);
  tft.drawString("ESP32-S3", 10, 50);
  tft.drawString("显示屏: ST7735", 10, 70);
  tft.drawString("传感器: DS18B20", 10, 90);
  
  // 显示操作提示
  drawFooter("K1:返回 K4:确认");
}

void setup() {
  // 初始化串口
  Serial.begin(115200);
  delay(100);
  Serial.println("系统启动...");
  
  // 初始化按键
  pinMode(KEY1_PIN, INPUT_PULLUP);
  pinMode(KEY2_PIN, INPUT_PULLUP);
  pinMode(KEY3_PIN, INPUT_PULLUP);
  pinMode(KEY4_PIN, INPUT_PULLUP);
  Serial.println("按键初始化完成");
  
  // 初始化显示屏
  Serial.println("开始初始化显示屏...");
  tft.init();
  delay(100);
  tft.setRotation(1);
  Serial.println("显示屏初始化完成");
  
  // 初始化传感器
  Serial.println("开始初始化传感器...");
  sensors.begin();
  delay(1000);
  
  // 获取传感器数量
  sensors.requestTemperatures();
  totalSensors = sensors.getDeviceCount();
  Serial.printf("找到 %d 个传感器\n", totalSensors);
  
  // 显示启动画面
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("温度监测系统", tft.width()/2, tft.height()/2 - 20);
  tft.setTextSize(1);
  tft.drawString("正在启动...", tft.width()/2, tft.height()/2 + 20);
  delay(1000);
}

void loop() {
  // 检查按键状态
  checkButtons();
  
  // 定时更新温度
  if (millis() - lastTempUpdate >= tempUpdateInterval) {
    sensors.requestTemperatures();
    lastTempUpdate = millis();
  }
  
  // 根据当前模式更新显示
  switch(currentMode) {
    case MODE_OVERVIEW:
      displayOverview();
      break;
    case MODE_DETAIL:
      displayDetailView(selectedSensor);
      break;
    case MODE_SETTINGS:
      displaySettings();
      break;
  }
  
  delay(10);
}