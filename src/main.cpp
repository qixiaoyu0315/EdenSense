#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <OneButton.h>

// 按键定义
#define KEY1_PIN   35    // K1: 切换显示模式
#define KEY2_PIN   36    // K2: 上翻
#define KEY3_PIN   37    // K3: 下翻
#define KEY4_PIN   38    // K4: 未使用

// DS18B20引脚定义
const int oneWireBus = 4;     // 使用GPIO4作为数据线

// 全局变量
int selectedSensor = -1;      // 当前选择的传感器，-1表示显示所有
int totalSensors = 0;         // 总传感器数量
unsigned long lastButtonCheck = 0; // 上次按键检查时间
const int debounceTime = 20;  // 按键防抖时间（毫秒）
unsigned long lastTempUpdate = 0;  // 上次温度更新时间
const int tempUpdateInterval = 1000;  // 温度更新间隔（毫秒）

// 用于存储上一次的温度值
float lastTemps[16] = {0};  // 假设最多16个传感器
bool firstDraw = true;      // 首次绘制标志

// 显示模式枚举
enum DisplayMode {
  MODE_OVERVIEW,    // 总览模式
  MODE_DETAIL      // 详情模式
};

DisplayMode currentMode = MODE_OVERVIEW;

// 初始化显示屏
TFT_eSPI tft = TFT_eSPI();

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

// 创建 OneButton 对象
OneButton button1(KEY1_PIN, true, true);  // true 表示使用内部上拉电阻，true 表示按键按下时为低电平
OneButton button2(KEY2_PIN, true, true);
OneButton button3(KEY3_PIN, true, true);

// 按键回调函数
void onButton1Click() {
  if (currentMode == MODE_OVERVIEW) {
    currentMode = MODE_DETAIL;
    selectedSensor = 0;
  } else {
    currentMode = MODE_OVERVIEW;
  }
  firstDraw = true;
}

void onButton2Click() {
  if (currentMode == MODE_DETAIL && selectedSensor > 0) {
    selectedSensor--;
    firstDraw = true;
  }
}

void onButton3Click() {
  if (currentMode == MODE_DETAIL && selectedSensor < totalSensors-1) {
    selectedSensor++;
    firstDraw = true;
  }
}

// 绘制标题栏（优化版本）
void drawHeader(const char* title, bool forceRedraw = false) {
  static char lastTitle[32] = "";
  if (forceRedraw || strcmp(lastTitle, title) != 0) {
    tft.fillRect(0, 0, tft.width(), 20, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(title, tft.width()/2, 10);
    strcpy(lastTitle, title);
  }
}

// 绘制底部状态栏（优化版本）
void drawFooter(const char* hint, bool forceRedraw = false) {
  static char lastHint[32] = "";
  if (forceRedraw || strcmp(lastHint, hint) != 0) {
    tft.fillRect(0, tft.height()-20, tft.width(), 20, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(hint, tft.width()/2, tft.height()-10);
    strcpy(lastHint, hint);
  }
}

// 显示单个传感器数据（优化版本）
void displayDetailView(int index) {
  static int lastIndex = -1;
  static float lastTemp = -999;
  bool forceRedraw = firstDraw || (lastIndex != index);
  
  if (forceRedraw) {
    tft.fillScreen(TFT_BLACK);
    lastIndex = index;
  }
  
  float tempC = sensors.getTempCByIndex(index);
  
  if (forceRedraw) {
    // 显示传感器编号
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("T" + String(index + 1), tft.width()/2, 30);
  }
  
  if (tempC != DEVICE_DISCONNECTED_C && (forceRedraw || abs(tempC - lastTemp) >= 0.1)) {
    // 清除旧温度显示区域
    tft.fillRect(0, 50, tft.width(), 60, TFT_BLACK);
    
    // 显示新温度
    tft.setTextSize(4);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(tempC, 1), tft.width()/2, 80);
    
    // 显示摄氏度符号
    tft.setTextSize(2);
    tft.drawString("C", tft.width()/2 + 60, 80);
    
    lastTemp = tempC;
  } else if (tempC == DEVICE_DISCONNECTED_C && forceRedraw) {
    tft.fillRect(0, 50, tft.width(), 60, TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("未连接", tft.width()/2, 80);
  }
  
  firstDraw = false;
}

// 显示所有传感器数据（优化版本）
void displayOverview() {
  static bool firstOverview = true;
  bool forceRedraw = firstDraw || firstOverview;
  
  if (forceRedraw) {
    tft.fillScreen(TFT_BLACK);
    firstOverview = false;
  }
  
  // 显示所有传感器数据
  for(int i = 0; i < totalSensors; i++) {
    float tempC = sensors.getTempCByIndex(i);
    int yPos = 20 + i * 25;
    
    // 只在温度变化或首次显示时更新
    if (forceRedraw || abs(tempC - lastTemps[i]) >= 0.1) {
      // 清除该行显示区域
      tft.fillRect(5, yPos-15, tft.width()-10, 20, TFT_BLACK);
      
      if(tempC != DEVICE_DISCONNECTED_C) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        String tempStr = "T" + String(i + 1) + ": " + String(tempC, 1) + "C";
        tft.drawString(tempStr, 10, yPos);
        lastTemps[i] = tempC;
      } else if (forceRedraw) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("T" + String(i + 1) + ": 未连接", 10, yPos);
      }
    }
  }
  
  firstDraw = false;
}

void setup() {
  // 初始化串口
  Serial.begin(115200);
  
  // 配置按键回调函数
  button1.attachClick(onButton1Click);
  button2.attachClick(onButton2Click);
  button3.attachClick(onButton3Click);
  
  // 初始化显示屏
  tft.init();
  tft.initDMA();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // 初始化传感器
  sensors.begin();
  delay(1000);
  
  // 获取传感器数量
  sensors.requestTemperatures();
  totalSensors = sensors.getDeviceCount();
  
  firstDraw = true;
}

void loop() {
  // 更新按键状态
  button1.tick();
  button2.tick();
  button3.tick();
  
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
  }
  
  delay(5);  // 减少主循环延时
}