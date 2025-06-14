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
#define ONEWIRE_BUS 4    // 使用GPIO4作为数据线

// 图表相关定义
#define GRAPH_HEIGHT 120    // 图表高度
#define GRAPH_WIDTH 240     // 图表宽度
#define GRAPH_TOP 40        // 图表顶部位置
#define GRAPH_LEFT 0        // 图表左侧位置
#define INFO_HEIGHT 35      // 信息显示区域高度
#define GRAPH_BOTTOM (GRAPH_TOP + GRAPH_HEIGHT)  // 图表底部位置

// 温度记录相关定义
#define MAX_RECORDS 144  // 存储144个数据点
#define RECORD_INTERVAL 600  // 记录间隔（秒）

// 温度报警相关定义
#define TEMP_ALARM_HIGH 30.0  // 高温报警阈值
#define TEMP_ALARM_LOW 10.0   // 低温报警阈值
#define ALARM_BLINK_INTERVAL 500  // 报警闪烁间隔（毫秒）

// 显示模式枚举
enum DisplayMode {
  MODE_OVERVIEW,    // 总览模式
  MODE_DETAIL,      // 详情模式
  MODE_GRAPH        // 图表模式
};

// 温度记录结构
struct TempRecord {
  float temps[MAX_RECORDS];  // 温度数组
  unsigned long timestamps[MAX_RECORDS];  // 使用 unsigned long 存储时间戳（毫秒）
  int recordCount;  // 当前记录数量
  int currentIndex;  // 当前写入位置
};

// 图表状态结构
struct GraphState {
  float lastMinTemp;
  float lastMaxTemp;
  float lastAvgTemp;
  float lastCurrentTemp;
  int lastRecordCount;
  bool needsFullRedraw;
  char lastTimeLabels[5][8];  // 存储时间标签文本
};

// 报警状态结构
struct AlarmState {
  bool highAlarm;
  bool lowAlarm;
  unsigned long lastBlinkTime;
  bool blinkState;
};

// 全局变量定义
int selectedSensor = -1;      // 当前选择的传感器，-1表示显示所有
int totalSensors = 0;         // 总传感器数量
unsigned long lastButtonCheck = 0; // 上次按键检查时间
const int debounceTime = 20;  // 按键防抖时间（毫秒）
unsigned long lastTempUpdate = 0;  // 上次温度更新时间
const int tempUpdateInterval = 1000;  // 温度更新间隔（毫秒）
bool firstDraw = true;      // 首次绘制标志
float lastTemps[16] = {0};  // 假设最多16个传感器
DisplayMode currentMode = MODE_OVERVIEW;
TempRecord sensorRecords[16];  // 假设最多16个传感器
GraphState graphState = {0};   // 图表状态
AlarmState alarmStates[16];  // 每个传感器的报警状态

// 全局对象定义
TFT_eSPI tft;
OneWire oneWire(ONEWIRE_BUS);
DallasTemperature sensors(&oneWire);
OneButton button1(KEY1_PIN, true, true);
OneButton button2(KEY2_PIN, true, true);
OneButton button3(KEY3_PIN, true, true);

// 函数声明
void drawGraphBackground(int sensorIndex);
void updateTempInfo(float minTemp, float maxTemp, float avgTemp, float currentTemp);
void drawGraph(int sensorIndex);
void displayOverview();
void displayDetailView(int index);
void updateTempRecords();
void onButton1Click();
void onButton2Click();
void onButton3Click();
void checkTemperatureAlarms(int sensorIndex, float temp);

// 函数实现
void drawGraphBackground(int sensorIndex) {
  // 清除图表区域
  tft.fillRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_BLACK);
  
  // 绘制边框
  tft.drawRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_WHITE);
  
  // 绘制水平网格线
  for (int i = 1; i < 4; i++) {
    int y = GRAPH_TOP + (GRAPH_HEIGHT * i / 4);
    tft.drawLine(GRAPH_LEFT, y, GRAPH_LEFT + GRAPH_WIDTH, y, TFT_DARKGREY);
  }
  
  // 绘制垂直网格线
  for (int i = 1; i < 4; i++) {
    int x = GRAPH_LEFT + (GRAPH_WIDTH * i / 4);
    tft.drawLine(x, GRAPH_TOP, x, GRAPH_TOP + GRAPH_HEIGHT, TFT_DARKGREY);
  }
  
  // 绘制时间标签
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM);
  
  // 获取当前时间
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  // 计算时间间隔（分钟）
  int interval = RECORD_INTERVAL / 60;  // 转换为分钟
  
  // 绘制5个时间标签
  for (int i = 0; i < 5; i++) {
    int x = GRAPH_LEFT + (GRAPH_WIDTH * i / 4);
    time_t labelTime = now - (RECORD_INTERVAL * (3 - i));
    struct tm* labelTimeInfo = localtime(&labelTime);
    
    char timeStr[8];
    strftime(timeStr, sizeof(timeStr), "%H:%M", labelTimeInfo);
    tft.drawString(timeStr, x, GRAPH_TOP + GRAPH_HEIGHT + 5);
    
    // 保存时间标签文本
    strncpy(graphState.lastTimeLabels[i], timeStr, sizeof(graphState.lastTimeLabels[i]) - 1);
    graphState.lastTimeLabels[i][sizeof(graphState.lastTimeLabels[i]) - 1] = '\0';
  }
}

void updateTempInfo(float minTemp, float maxTemp, float avgTemp, float currentTemp) {
  static char lastInfo[4][16] = {""};  // 存储上一次的信息文本
  char newInfo[4][16];  // 存储新的信息文本
  
  // 格式化新的信息文本
  snprintf(newInfo[0], sizeof(newInfo[0]), "最低: %.1fC", minTemp);
  snprintf(newInfo[1], sizeof(newInfo[1]), "最高: %.1fC", maxTemp);
  snprintf(newInfo[2], sizeof(newInfo[2]), "平均: %.1fC", avgTemp);
  snprintf(newInfo[3], sizeof(newInfo[3]), "当前: %.1fC", currentTemp);
  
  // 检查是否需要更新显示
  bool needsUpdate = false;
  for (int i = 0; i < 4; i++) {
    if (strcmp(lastInfo[i], newInfo[i]) != 0) {
      needsUpdate = true;
      break;
    }
  }
  
  if (needsUpdate) {
    // 清除信息显示区域
    tft.fillRect(0, GRAPH_TOP + GRAPH_HEIGHT + 25, tft.width(), INFO_HEIGHT, TFT_BLACK);
    
    // 显示新的信息
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(TL_DATUM);
    
    for (int i = 0; i < 4; i++) {
      int y = GRAPH_TOP + GRAPH_HEIGHT + 25 + (i * 8);
      tft.drawString(newInfo[i], 5, y);
      strcpy(lastInfo[i], newInfo[i]);  // 更新存储的文本
    }
  }
}

void drawGraph(int sensorIndex) {
  static int lastSensorIndex = -1;
  static float lastMinTemp = -999;
  static float lastMaxTemp = -999;
  static float lastAvgTemp = -999;
  static float lastCurrentTemp = -999;
  static int lastRecordCount = 0;
  
  // 检查是否需要完全重绘
  bool needsFullRedraw = firstDraw || 
                        (lastSensorIndex != sensorIndex) || 
                        graphState.needsFullRedraw;
  
  if (needsFullRedraw) {
    drawGraphBackground(sensorIndex);
    lastSensorIndex = sensorIndex;
    graphState.needsFullRedraw = false;
  }
  
  // 获取温度数据
  float minTemp = 999;
  float maxTemp = -999;
  float sumTemp = 0;
  float currentTemp = sensors.getTempCByIndex(sensorIndex);
  int validCount = 0;
  
  // 计算统计数据
  for (int i = 0; i < sensorRecords[sensorIndex].recordCount; i++) {
    float temp = sensorRecords[sensorIndex].temps[i];
    if (temp != DEVICE_DISCONNECTED_C) {
      minTemp = min(minTemp, temp);
      maxTemp = max(maxTemp, temp);
      sumTemp += temp;
      validCount++;
    }
  }
  
  float avgTemp = validCount > 0 ? sumTemp / validCount : 0;
  
  // 检查是否需要更新温度信息
  bool needsInfoUpdate = needsFullRedraw ||
                        (abs(minTemp - lastMinTemp) >= 0.1) ||
                        (abs(maxTemp - lastMaxTemp) >= 0.1) ||
                        (abs(avgTemp - lastAvgTemp) >= 0.1) ||
                        (abs(currentTemp - lastCurrentTemp) >= 0.1);
  
  if (needsInfoUpdate) {
    updateTempInfo(minTemp, maxTemp, avgTemp, currentTemp);
    lastMinTemp = minTemp;
    lastMaxTemp = maxTemp;
    lastAvgTemp = avgTemp;
    lastCurrentTemp = currentTemp;
  }
  
  // 检查是否需要重绘图表
  bool needsGraphUpdate = needsFullRedraw ||
                         (sensorRecords[sensorIndex].recordCount != lastRecordCount);
  
  if (needsGraphUpdate) {
    // 清除旧图表
    tft.fillRect(GRAPH_LEFT + 1, GRAPH_TOP + 1, GRAPH_WIDTH - 2, GRAPH_HEIGHT - 2, TFT_BLACK);
    
    // 计算温度范围
    float tempRange = maxTemp - minTemp;
    if (tempRange < 1.0) tempRange = 1.0;  // 确保有最小范围
    
    // 绘制数据点
    for (int i = 1; i < sensorRecords[sensorIndex].recordCount; i++) {
      float temp1 = sensorRecords[sensorIndex].temps[i-1];
      float temp2 = sensorRecords[sensorIndex].temps[i];
      
      if (temp1 != DEVICE_DISCONNECTED_C && temp2 != DEVICE_DISCONNECTED_C) {
        // 计算坐标
        int x1 = GRAPH_LEFT + ((i-1) * GRAPH_WIDTH / (MAX_RECORDS - 1));
        int x2 = GRAPH_LEFT + (i * GRAPH_WIDTH / (MAX_RECORDS - 1));
        int y1 = GRAPH_TOP + GRAPH_HEIGHT - ((temp1 - minTemp) * GRAPH_HEIGHT / tempRange);
        int y2 = GRAPH_TOP + GRAPH_HEIGHT - ((temp2 - minTemp) * GRAPH_HEIGHT / tempRange);
        
        // 绘制线段
        tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
        
        // 绘制数据点
        tft.fillCircle(x1, y1, 1, TFT_GREEN);
        tft.fillCircle(x2, y2, 1, TFT_GREEN);
      }
    }
    
    lastRecordCount = sensorRecords[sensorIndex].recordCount;
  }
  
  firstDraw = false;
}

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
    
    // 检查温度报警
    checkTemperatureAlarms(i, tempC);
    
    // 只在温度变化或首次显示时更新
    if (forceRedraw || abs(tempC - lastTemps[i]) >= 0.1) {
      // 清除该行显示区域
      tft.fillRect(5, yPos-15, tft.width()-10, 20, TFT_BLACK);
      
      if(tempC != DEVICE_DISCONNECTED_C) {
        // 根据报警状态选择颜色
        uint16_t textColor = TFT_GREEN;
        if (alarmStates[i].highAlarm && alarmStates[i].blinkState) {
          textColor = TFT_RED;
        } else if (alarmStates[i].lowAlarm && alarmStates[i].blinkState) {
          textColor = TFT_BLUE;
        }
        
        tft.setTextColor(textColor, TFT_BLACK);
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

void displayDetailView(int index) {
  static int lastIndex = -1;
  static float lastTemp = -999;
  bool forceRedraw = firstDraw || (lastIndex != index);
  
  if (forceRedraw) {
    tft.fillScreen(TFT_BLACK);
    lastIndex = index;
  }
  
  float tempC = sensors.getTempCByIndex(index);
  checkTemperatureAlarms(index, tempC);
  
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
    
    // 根据报警状态选择颜色
    uint16_t textColor = TFT_GREEN;
    if (alarmStates[index].highAlarm && alarmStates[index].blinkState) {
      textColor = TFT_RED;
    } else if (alarmStates[index].lowAlarm && alarmStates[index].blinkState) {
      textColor = TFT_BLUE;
    }
    
    // 显示新温度
    tft.setTextSize(4);
    tft.setTextColor(textColor, TFT_BLACK);
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

// 修改按键回调函数实现
void onButton1Click() {
  if (currentMode == MODE_OVERVIEW) {
    currentMode = MODE_DETAIL;
    selectedSensor = 0;  // 从第一个传感器开始
  } else if (currentMode == MODE_DETAIL) {
    currentMode = MODE_GRAPH;
    // 保持在当前选中的传感器
  } else {
    currentMode = MODE_OVERVIEW;
    selectedSensor = -1;  // 重置传感器选择
  }
  firstDraw = true;
  graphState.needsFullRedraw = true;
}

void onButton2Click() {
  if (currentMode == MODE_DETAIL || currentMode == MODE_GRAPH) {
    if (selectedSensor > 0) {
      selectedSensor--;
      firstDraw = true;
      graphState.needsFullRedraw = true;
      Serial.print("Button2: 切换到传感器 ");
      Serial.println(selectedSensor);
    }
  }
}

void onButton3Click() {
  if (currentMode == MODE_DETAIL || currentMode == MODE_GRAPH) {
    if (selectedSensor < totalSensors-1) {
      selectedSensor++;
      firstDraw = true;
      graphState.needsFullRedraw = true;
      Serial.print("Button3: 切换到传感器 ");
      Serial.println(selectedSensor);
    }
  }
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
  tft.initDMA();  // 启用DMA加速
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // 初始化传感器
  sensors.begin();
  delay(1000);
  
  // 获取传感器数量
  sensors.requestTemperatures();
  totalSensors = sensors.getDeviceCount();
  
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
  
  // 初始化报警状态
  for (int i = 0; i < 16; i++) {
    alarmStates[i].highAlarm = false;
    alarmStates[i].lowAlarm = false;
    alarmStates[i].lastBlinkTime = 0;
    alarmStates[i].blinkState = false;
  }
  
  firstDraw = true;
}

void loop() {
  // 更新按键状态
  button1.tick();
  button2.tick();
  button3.tick();
  
  // 添加调试信息
  static unsigned long lastDebugTime = 0;
  if (millis() - lastDebugTime >= 1000) {  // 每秒打印一次状态
    Serial.print("当前模式: ");
    Serial.print(currentMode);
    Serial.print(", 选中传感器: ");
    Serial.println(selectedSensor);
    lastDebugTime = millis();
  }
  
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
  
  delay(5);
}

// 添加报警检查函数
void checkTemperatureAlarms(int sensorIndex, float temp) {
  if (temp == DEVICE_DISCONNECTED_C) return;
  
  AlarmState &alarm = alarmStates[sensorIndex];
  alarm.highAlarm = (temp >= TEMP_ALARM_HIGH);
  alarm.lowAlarm = (temp <= TEMP_ALARM_LOW);
  
  // 更新闪烁状态
  if (alarm.highAlarm || alarm.lowAlarm) {
    if (millis() - alarm.lastBlinkTime >= ALARM_BLINK_INTERVAL) {
      alarm.blinkState = !alarm.blinkState;
      alarm.lastBlinkTime = millis();
    }
  } else {
    alarm.blinkState = false;
  }
} 