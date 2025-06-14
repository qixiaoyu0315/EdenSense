#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <OneButton.h>
#include <time.h>

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

// 温度记录相关定义
#define MAX_RECORDS 144  // 存储144个数据点
#define RECORD_INTERVAL 600  // 记录间隔（秒）
#define GRAPH_HEIGHT 120    // 图表高度
#define GRAPH_WIDTH 240     // 图表宽度
#define GRAPH_TOP 40        // 图表顶部位置
#define GRAPH_LEFT 0        // 图表左侧位置
#define INFO_HEIGHT 35      // 信息显示区域高度
#define GRAPH_BOTTOM (GRAPH_TOP + GRAPH_HEIGHT)  // 图表底部位置

// 温度记录结构
struct TempRecord {
  float temps[MAX_RECORDS];  // 温度数组
  unsigned long timestamps[MAX_RECORDS];  // 使用 unsigned long 存储时间戳（毫秒）
  int recordCount;  // 当前记录数量
  int currentIndex;  // 当前写入位置
};

// 为每个传感器创建温度记录
TempRecord sensorRecords[16];  // 假设最多16个传感器

// 显示模式枚举
enum DisplayMode {
  MODE_OVERVIEW,    // 总览模式
  MODE_DETAIL,      // 详情模式
  MODE_GRAPH        // 图表模式
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
  } else if (currentMode == MODE_DETAIL) {
    currentMode = MODE_GRAPH;
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

// 更新温度记录
void updateTempRecords() {
  static unsigned long lastRecordTime = 0;
  unsigned long currentTime = millis();
  
  // 检查是否需要记录新数据（每10分钟记录一次）
  if (currentTime - lastRecordTime >= (RECORD_INTERVAL * 1000)) {
    for (int i = 0; i < totalSensors; i++) {
      float tempC = sensors.getTempCByIndex(i);
      if (tempC != DEVICE_DISCONNECTED_C) {
        // 更新记录
        sensorRecords[i].temps[sensorRecords[i].currentIndex] = tempC;
        sensorRecords[i].timestamps[sensorRecords[i].currentIndex] = currentTime;
        
        // 更新索引和计数
        sensorRecords[i].currentIndex = (sensorRecords[i].currentIndex + 1) % MAX_RECORDS;
        if (sensorRecords[i].recordCount < MAX_RECORDS) {
          sensorRecords[i].recordCount++;
        }
      }
    }
    lastRecordTime = currentTime;
  }
}

// 用于存储上一次的图表数据
struct GraphState {
  float lastMinTemp;
  float lastMaxTemp;
  float lastAvgTemp;
  float lastCurrentTemp;
  int lastRecordCount;
  bool needsFullRedraw;
  char lastTimeLabels[5][8];  // 存储时间标签文本
};

GraphState graphState = {0};

// 绘制图表背景（静态部分）
void drawGraphBackground() {
  // 清除图表区域
  tft.fillRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT + INFO_HEIGHT, TFT_BLACK);
  
  // 绘制坐标轴
  tft.drawLine(GRAPH_LEFT, GRAPH_BOTTOM, 
               GRAPH_LEFT + GRAPH_WIDTH, GRAPH_BOTTOM, TFT_WHITE);
  tft.drawLine(GRAPH_LEFT, GRAPH_TOP, 
               GRAPH_LEFT, GRAPH_BOTTOM, TFT_WHITE);
  
  // 绘制网格线
  for (int i = 0; i <= 4; i++) {
    int y = GRAPH_TOP + (GRAPH_HEIGHT * i / 4);
    tft.drawLine(GRAPH_LEFT, y, GRAPH_LEFT + GRAPH_WIDTH, y, TFT_DARKGREY);
  }
  
  // 绘制时间轴标签
  for (int i = 0; i <= 4; i++) {
    int x = GRAPH_LEFT + (GRAPH_WIDTH * i / 4);
    int minutes = (i * 30);
    snprintf(graphState.lastTimeLabels[i], sizeof(graphState.lastTimeLabels[i]), "%dm", minutes);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(graphState.lastTimeLabels[i], x, GRAPH_BOTTOM + 5);
  }
}

// 更新温度信息显示
void updateTempInfo(float minTemp, float maxTemp, float avgTemp, float currentTemp) {
  static char lastInfo[4][16] = {""};  // 存储上一次的信息文本
  char currentInfo[4][16];
  
  // 准备新的信息文本
  snprintf(currentInfo[0], sizeof(currentInfo[0]), "Max: %.1fC", maxTemp);
  snprintf(currentInfo[1], sizeof(currentInfo[1]), "Min: %.1fC", minTemp);
  snprintf(currentInfo[2], sizeof(currentInfo[2]), "Avg: %.1fC", avgTemp);
  snprintf(currentInfo[3], sizeof(currentInfo[3]), "Now: %.1fC", currentTemp);
  
  // 只在文本变化时更新显示
  for (int i = 0; i < 4; i++) {
    if (strcmp(lastInfo[i], currentInfo[i]) != 0) {
      // 清除旧文本区域
      tft.fillRect(GRAPH_LEFT + 5, GRAPH_TOP + i * 10, 100, 10, TFT_BLACK);
      
      // 绘制新文本
      tft.setTextColor(i == 3 ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextDatum(TL_DATUM);
      tft.drawString(currentInfo[i], GRAPH_LEFT + 5, GRAPH_TOP + i * 10);
      
      // 更新存储的文本
      strcpy(lastInfo[i], currentInfo[i]);
    }
  }
}

// 绘制折线图
void drawGraph(int sensorIndex) {
  static int lastSensorIndex = -1;
  bool forceRedraw = firstDraw || (lastSensorIndex != sensorIndex);
  
  // 检查是否需要完全重绘
  if (forceRedraw || graphState.needsFullRedraw) {
    drawGraphBackground();
    graphState.needsFullRedraw = false;
    lastSensorIndex = sensorIndex;
  }
  
  // 计算温度范围
  float minTemp = 100.0;
  float maxTemp = -100.0;
  float sumTemp = 0.0;
  int validCount = 0;
  
  for (int i = 0; i < sensorRecords[sensorIndex].recordCount; i++) {
    float temp = sensorRecords[sensorIndex].temps[i];
    if (temp != DEVICE_DISCONNECTED_C) {
      if (temp < minTemp) minTemp = temp;
      if (temp > maxTemp) maxTemp = temp;
      sumTemp += temp;
      validCount++;
    }
  }
  
  // 添加边距并确保最小范围
  float tempRange = maxTemp - minTemp;
  if (tempRange < 1.0) tempRange = 1.0;
  minTemp -= tempRange * 0.1;
  maxTemp += tempRange * 0.1;
  
  // 计算平均温度
  float avgTemp = validCount > 0 ? sumTemp / validCount : 0;
  float currentTemp = sensors.getTempCByIndex(sensorIndex);
  
  // 检查是否需要更新温度信息
  if (forceRedraw || 
      abs(minTemp - graphState.lastMinTemp) > 0.1 ||
      abs(maxTemp - graphState.lastMaxTemp) > 0.1 ||
      abs(avgTemp - graphState.lastAvgTemp) > 0.1 ||
      abs(currentTemp - graphState.lastCurrentTemp) > 0.1) {
    
    updateTempInfo(minTemp, maxTemp, avgTemp, currentTemp);
    
    // 更新状态
    graphState.lastMinTemp = minTemp;
    graphState.lastMaxTemp = maxTemp;
    graphState.lastAvgTemp = avgTemp;
    graphState.lastCurrentTemp = currentTemp;
  }
  
  // 只在记录数量变化或强制重绘时更新曲线
  if (forceRedraw || sensorRecords[sensorIndex].recordCount != graphState.lastRecordCount) {
    // 清除旧的曲线
    tft.fillRect(GRAPH_LEFT + 1, GRAPH_TOP + 1, GRAPH_WIDTH - 2, GRAPH_HEIGHT - 2, TFT_BLACK);
    
    // 绘制新的温度曲线
    if (sensorRecords[sensorIndex].recordCount > 1) {
      for (int i = 1; i < sensorRecords[sensorIndex].recordCount; i++) {
        int x1 = GRAPH_LEFT + ((i-1) * GRAPH_WIDTH / (MAX_RECORDS-1));
        int x2 = GRAPH_LEFT + (i * GRAPH_WIDTH / (MAX_RECORDS-1));
        
        float temp1 = sensorRecords[sensorIndex].temps[(sensorRecords[sensorIndex].currentIndex - sensorRecords[sensorIndex].recordCount + i-1 + MAX_RECORDS) % MAX_RECORDS];
        float temp2 = sensorRecords[sensorIndex].temps[(sensorRecords[sensorIndex].currentIndex - sensorRecords[sensorIndex].recordCount + i + MAX_RECORDS) % MAX_RECORDS];
        
        if (temp1 != DEVICE_DISCONNECTED_C && temp2 != DEVICE_DISCONNECTED_C) {
          int y1 = GRAPH_TOP + GRAPH_HEIGHT - ((temp1 - minTemp) * GRAPH_HEIGHT / (maxTemp - minTemp));
          int y2 = GRAPH_TOP + GRAPH_HEIGHT - ((temp2 - minTemp) * GRAPH_HEIGHT / (maxTemp - minTemp));
          
          tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
        }
      }
    }
    
    graphState.lastRecordCount = sensorRecords[sensorIndex].recordCount;
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
  
  // 初始化温度记录
  for (int i = 0; i < 16; i++) {
    sensorRecords[i].recordCount = 0;
    sensorRecords[i].currentIndex = 0;
  }
  
  // 初始化图表状态
  graphState.lastMinTemp = 0;
  graphState.lastMaxTemp = 0;
  graphState.lastAvgTemp = 0;
  graphState.lastCurrentTemp = 0;
  graphState.lastRecordCount = 0;
  graphState.needsFullRedraw = true;
}

void loop() {
  // 更新按键状态
  button1.tick();
  button2.tick();
  button3.tick();
  
  // 更新温度记录
  updateTempRecords();
  
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
    case MODE_GRAPH:
      drawGraph(selectedSensor);
      break;
  }
  
  delay(5);  // 减少主循环延时
}