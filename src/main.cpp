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

// 显示布局参数
#define SCREEN_WIDTH 128   // 屏幕宽度
#define SCREEN_HEIGHT 128  // 屏幕高度
#define TITLE_HEIGHT 12    // 标题区域高度
#define TEMP_INFO_HEIGHT 20  // 温度信息显示区域高度

// 图表显示参数
#define GRAPH_HEIGHT 80    // 图表高度
#define GRAPH_WIDTH 120    // 图表宽度
#define GRAPH_LEFT 9       // 图表左边距
#define TEMP_SCALE_WIDTH 8  // 温度刻度值显示区域宽度
#define GRAPH_TOP (TITLE_HEIGHT + TEMP_INFO_HEIGHT)  // 图表顶部位置
#define INFO_TOP (TITLE_HEIGHT + 2)  // 温度信息显示区域顶部位置

// 概览模式布局参数
#define OVERVIEW_ROW_HEIGHT 30  // 每行高度
#define OVERVIEW_TEMP_SIZE 2    // 温度字体大小
#define OVERVIEW_SENSOR_SIZE 1  // 传感器编号字体大小

// 温度记录相关定义
#define MAX_RECORDS 120  // 修改为120个数据点
#define RECORD_INTERVAL 10  // 每10秒记录一次
#define TEMP_MIN 10.0  // 最小温度刻度
#define TEMP_MAX 45.0  // 最大温度刻度
#define TEMP_STEP 5.0  // 温度刻度间隔
#define GRID_X_SPACING 12  // 垂直网格线间距（像素）

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

// 添加显示状态结构
struct DisplayState {
  bool needsRedraw;  // 是否需要重绘
  int lastSelectedSensor;  // 上次选择的传感器
  DisplayMode lastMode;  // 上次的显示模式
  float lastTemps[16];  // 上次显示的温度值
  bool lastAlarmStates[16][2];  // 上次的报警状态 [传感器][高温/低温]
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
bool screenOn = true;  // 屏幕开关状态
DisplayState displayState = {false, -1, MODE_OVERVIEW, {0}, {{false}}};

// 全局对象定义
TFT_eSPI tft;
OneWire oneWire(ONEWIRE_BUS);
DallasTemperature sensors(&oneWire);
OneButton button1(KEY1_PIN, true, true);
OneButton button2(KEY2_PIN, true, true);
OneButton button3(KEY3_PIN, true, true);
OneButton button4(KEY4_PIN, true, true);


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
void onButton4Click();
void checkTemperatureAlarms(int sensorIndex, float temp);

// 函数实现
void drawGraphBackground(int sensorIndex) {
  // 绘制图表边框
  tft.drawRect(GRAPH_LEFT, GRAPH_TOP, GRAPH_WIDTH, GRAPH_HEIGHT, TFT_WHITE);
  
  // 设置温度刻度值文本属性
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TR_DATUM);  // 右对齐文本
  
  // 绘制水平温度刻度线和刻度值
  for (float temp = TEMP_MIN; temp <= TEMP_MAX; temp += TEMP_STEP) {
    int y = GRAPH_TOP + GRAPH_HEIGHT - ((temp - TEMP_MIN) * GRAPH_HEIGHT / (TEMP_MAX - TEMP_MIN));
    // 绘制刻度线
    tft.drawLine(GRAPH_LEFT, y, GRAPH_LEFT + GRAPH_WIDTH, y, TFT_DARKGREY);
    // 绘制刻度值
    tft.drawString(String(int(temp)), GRAPH_LEFT - 2, y);
  }
  
  // 绘制垂直网格线（每12像素一条，共10条）
  for (int x = GRAPH_LEFT; x <= GRAPH_LEFT + GRAPH_WIDTH; x += GRID_X_SPACING) {
    tft.drawLine(x, GRAPH_TOP, x, GRAPH_TOP + GRAPH_HEIGHT, TFT_DARKGREY);
  }
}

void updateTempInfo(float minTemp, float maxTemp, float avgTemp, float currentTemp) {
  // 清除温度信息显示区域
  tft.fillRect(0, INFO_TOP, SCREEN_WIDTH, TEMP_INFO_HEIGHT - 2, TFT_BLACK);
  
  // 设置文本属性
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  
  // 计算每个温度值的显示位置
  int x_spacing = SCREEN_WIDTH / 4;  // 将屏幕宽度平均分为4份
  
  // 显示当前温度（绿色）
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(currentTemp, 1), 2, INFO_TOP);
  
  // 显示平均温度（黄色）
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString(String(avgTemp, 1), 2 + x_spacing, INFO_TOP);
  
  // 显示最大温度（红色）
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.drawString(String(maxTemp, 1), 2 + x_spacing * 2, INFO_TOP);
  
  // 显示最小温度（蓝色）
  tft.setTextColor(TFT_BLUE, TFT_BLACK);
  tft.drawString(String(minTemp, 1), 2 + x_spacing * 3, INFO_TOP);
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
  
  // 添加调试信息
  if (lastSensorIndex != sensorIndex) {
    Serial.print("切换到传感器 ");
    Serial.print(sensorIndex);
    Serial.print(", 当前记录数: ");
    Serial.println(sensorRecords[sensorIndex].recordCount);
  }
  
  if (needsFullRedraw) {
    // 清除整个屏幕
    tft.fillScreen(TFT_BLACK);
    
    // 显示当前传感器编号（T1, T2格式）
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("T" + String(sensorIndex + 1), SCREEN_WIDTH/2, TITLE_HEIGHT/2);
    
    drawGraphBackground(sensorIndex);
    lastSensorIndex = sensorIndex;
    graphState.needsFullRedraw = false;
    
    // 重置其他状态变量
    lastMinTemp = -999;
    lastMaxTemp = -999;
    lastAvgTemp = -999;
    lastCurrentTemp = -999;
    lastRecordCount = 0;
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
  
  // 打印图表数据信息
  if (sensorRecords[sensorIndex].recordCount > 0) {
    Serial.print("图表数据 - 传感器 ");
    Serial.print(sensorIndex);
    Serial.print(": 记录数=");
    Serial.print(sensorRecords[sensorIndex].recordCount);
    Serial.print(", 最小值=");
    Serial.print(minTemp);
    Serial.print(", 最大值=");
    Serial.print(maxTemp);
    Serial.print(", 平均值=");
    Serial.println(avgTemp);
  }
  
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
    // 清除旧图表（包括温度刻度值区域）
    int clearLeft = GRAPH_LEFT - TEMP_SCALE_WIDTH;
    int clearWidth = GRAPH_WIDTH + TEMP_SCALE_WIDTH - 1;
    tft.fillRect(clearLeft, GRAPH_TOP + 1, clearWidth, GRAPH_HEIGHT - 2, TFT_BLACK);
    
    // 重新绘制网格线和刻度值
    drawGraphBackground(sensorIndex);
    
    // 绘制数据点
    for (int i = 1; i < sensorRecords[sensorIndex].recordCount; i++) {
      float temp1 = sensorRecords[sensorIndex].temps[i-1];
      float temp2 = sensorRecords[sensorIndex].temps[i];
      
      if (temp1 != DEVICE_DISCONNECTED_C && temp2 != DEVICE_DISCONNECTED_C) {
        // 计算坐标 - 现在每个数据点对应一个像素位置
        int x1 = GRAPH_LEFT + i - 1;
        int x2 = GRAPH_LEFT + i;
        int y1 = GRAPH_TOP + GRAPH_HEIGHT - ((temp1 - TEMP_MIN) * GRAPH_HEIGHT / (TEMP_MAX - TEMP_MIN));
        int y2 = GRAPH_TOP + GRAPH_HEIGHT - ((temp2 - TEMP_MIN) * GRAPH_HEIGHT / (TEMP_MAX - TEMP_MIN));
        
        // 绘制线段
        tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
        
        // 绘制数据点（只在网格线位置绘制）
        if (i % GRID_X_SPACING == 0) {
          tft.fillCircle(x1, y1, 1, TFT_GREEN);
        }
        if (i == sensorRecords[sensorIndex].recordCount - 1) {
          tft.fillCircle(x2, y2, 1, TFT_GREEN);
        }
      }
    }
    
    lastRecordCount = sensorRecords[sensorIndex].recordCount;
  }
  
  firstDraw = false;
}

void displayOverview() {
  // 检查是否需要重绘
  bool needsRedraw = displayState.needsRedraw || 
                    displayState.lastMode != currentMode;
  
  // 检查每个传感器的状态是否发生变化
  for (int i = 0; i < totalSensors; i++) {
    float tempC = sensors.getTempCByIndex(i);
    // 添加温度变化检测的容差，避免小数点波动导致频繁刷新
    if (abs(tempC - displayState.lastTemps[i]) >= 0.1 ||
        alarmStates[i].highAlarm != displayState.lastAlarmStates[i][0] ||
        alarmStates[i].lowAlarm != displayState.lastAlarmStates[i][1]) {
      needsRedraw = true;
      break;
    }
  }
  
  if (!needsRedraw) {
    return;  // 如果没有变化，不进行重绘
  }
  
  // 清除屏幕
  tft.fillScreen(TFT_BLACK);
  
  // 显示标题
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("温度概览", SCREEN_WIDTH/2, TITLE_HEIGHT/2);
  
  // 计算每行的高度和起始位置
  int rowHeight = OVERVIEW_ROW_HEIGHT;
  int startY = TITLE_HEIGHT + 5;  // 标题下方留出5像素间距
  
  // 显示所有传感器温度
  for (int i = 0; i < totalSensors; i++) {
    float tempC = sensors.getTempCByIndex(i);
    int rowY = startY + (i * rowHeight);
    
    // 更新状态记录
    displayState.lastTemps[i] = tempC;
    displayState.lastAlarmStates[i][0] = alarmStates[i].highAlarm;
    displayState.lastAlarmStates[i][1] = alarmStates[i].lowAlarm;
    
    // 显示传感器编号（T1, T2格式）
    tft.setTextSize(OVERVIEW_SENSOR_SIZE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("T" + String(i + 1), 5, rowY);
    
    // 显示温度值
    if (tempC != DEVICE_DISCONNECTED_C) {
      tft.setTextSize(OVERVIEW_TEMP_SIZE);
      // 根据报警状态设置颜色
      if (alarmStates[i].highAlarm) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
      } else if (alarmStates[i].lowAlarm) {
        tft.setTextColor(TFT_BLUE, TFT_BLACK);
      } else {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
      }
      tft.setTextDatum(TR_DATUM);
      tft.drawString(String(tempC, 1) + "C", SCREEN_WIDTH - 5, rowY);
    } else {
      tft.setTextSize(OVERVIEW_TEMP_SIZE);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextDatum(TR_DATUM);
      tft.drawString("未连接", SCREEN_WIDTH - 5, rowY);
    }
  }
  
  displayState.lastMode = currentMode;
  displayState.needsRedraw = false;
}

void displayDetailView(int sensorIndex) {
  // 检查是否需要重绘
  bool needsRedraw = displayState.needsRedraw || 
                    displayState.lastMode != currentMode ||
                    displayState.lastSelectedSensor != sensorIndex;
  
  float tempC = sensors.getTempCByIndex(sensorIndex);
  // 添加温度变化检测的容差
  if (abs(tempC - displayState.lastTemps[sensorIndex]) >= 0.1 ||
      alarmStates[sensorIndex].highAlarm != displayState.lastAlarmStates[sensorIndex][0] ||
      alarmStates[sensorIndex].lowAlarm != displayState.lastAlarmStates[sensorIndex][1]) {
    needsRedraw = true;
  }
  
  if (!needsRedraw) {
    return;  // 如果没有变化，不进行重绘
  }
  
  // 清除屏幕
  tft.fillScreen(TFT_BLACK);
  
  // 显示传感器编号（T1, T2格式）
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("T" + String(sensorIndex + 1), SCREEN_WIDTH/2, TITLE_HEIGHT/2);
  
  // 更新状态记录
  displayState.lastTemps[sensorIndex] = tempC;
  displayState.lastAlarmStates[sensorIndex][0] = alarmStates[sensorIndex].highAlarm;
  displayState.lastAlarmStates[sensorIndex][1] = alarmStates[sensorIndex].lowAlarm;
  displayState.lastSelectedSensor = sensorIndex;
  
  if (tempC != DEVICE_DISCONNECTED_C) {
    // 显示温度值
    tft.setTextSize(3);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(tempC, 1) + "C", SCREEN_WIDTH/2, 60);
    
    // 显示报警状态
    tft.setTextSize(1);
    if (alarmStates[sensorIndex].highAlarm) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("高温报警", SCREEN_WIDTH/2, 90);
    } else if (alarmStates[sensorIndex].lowAlarm) {
      tft.setTextColor(TFT_BLUE, TFT_BLACK);
      tft.drawString("低温报警", SCREEN_WIDTH/2, 90);
    } else {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawString("正常", SCREEN_WIDTH/2, 90);
    }
    
    // 显示报警阈值
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("高温阈值: " + String(TEMP_ALARM_HIGH, 1) + "C", SCREEN_WIDTH/2, 110);
    tft.drawString("低温阈值: " + String(TEMP_ALARM_LOW, 1) + "C", SCREEN_WIDTH/2, 120);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("传感器未连接", SCREEN_WIDTH/2, 60);
  }
  
  displayState.lastMode = currentMode;
  displayState.needsRedraw = false;
}

void updateTempRecords() {
  static unsigned long lastRecordTime = 0;
  unsigned long currentTime = millis();
  
  // 检查是否需要记录新数据
  if (currentTime - lastRecordTime >= (RECORD_INTERVAL * 1000)) {
    Serial.println("开始记录温度数据...");
    for (int i = 0; i < totalSensors; i++) {
      float tempC = sensors.getTempCByIndex(i);
      if (tempC != DEVICE_DISCONNECTED_C) {
        // 更新记录
        sensorRecords[i].temps[sensorRecords[i].currentIndex] = tempC;
        sensorRecords[i].timestamps[sensorRecords[i].currentIndex] = currentTime;
        
        // 打印记录信息
        Serial.print("传感器 ");
        Serial.print(i);
        Serial.print(" 记录温度: ");
        Serial.print(tempC);
        Serial.print("C, 记录位置: ");
        Serial.print(sensorRecords[i].currentIndex);
        Serial.print(", 总记录数: ");
        Serial.println(sensorRecords[i].recordCount);
        
        // 更新索引和计数
        sensorRecords[i].currentIndex = (sensorRecords[i].currentIndex + 1) % MAX_RECORDS;
        if (sensorRecords[i].recordCount < MAX_RECORDS) {
          sensorRecords[i].recordCount++;
        }
      }
    }
    lastRecordTime = currentTime;
    Serial.println("温度数据记录完成");
  }
}

// 修改按钮回调函数
void onButton1Click() {
  currentMode = (DisplayMode)((currentMode + 1) % 3);
  if (currentMode == MODE_OVERVIEW) {
    selectedSensor = -1;  // 概览模式不选择具体传感器
  } else if (selectedSensor == -1) {
    selectedSensor = 0;  // 如果从概览模式切换过来，默认选择第一个传感器
  }
  displayState.needsRedraw = true;
  Serial.print("切换到模式: ");
  Serial.println(currentMode);
}

void onButton2Click() {
  if (currentMode == MODE_DETAIL || currentMode == MODE_GRAPH) {
    if (selectedSensor == -1) {
      selectedSensor = 0;
    } else {
      selectedSensor = (selectedSensor + 1) % totalSensors;
    }
    displayState.needsRedraw = true;
    Serial.print("选择传感器: ");
    Serial.println(selectedSensor + 1);  // 显示从1开始的编号
  }
}

void onButton3Click() {
  if (currentMode == MODE_DETAIL || currentMode == MODE_GRAPH) {
    if (selectedSensor == -1) {
      selectedSensor = totalSensors - 1;
    } else {
      selectedSensor = (selectedSensor - 1 + totalSensors) % totalSensors;
    }
    displayState.needsRedraw = true;
    Serial.print("选择传感器: ");
    Serial.println(selectedSensor + 1);  // 显示从1开始的编号
  }
}

void onButton4Click() {
  screenOn = !screenOn;  // 切换屏幕状态
  if (screenOn) {
    tft.writecommand(0x11);  // 退出睡眠模式
    delay(120);              // 等待屏幕唤醒
    tft.writecommand(0x29);  // 开启显示
    digitalWrite(TFT_BL, HIGH);  // 开启背光
  } else {
    digitalWrite(TFT_BL, LOW);   // 关闭背光
    delay(50);                   // 等待背光完全关闭
    tft.writecommand(0x28);      // 关闭显示
    tft.writecommand(0x10);      // 进入睡眠模式
  }
  Serial.print("屏幕已");
  Serial.println(screenOn ? "开启" : "关闭");
}

void setup() {
  Serial.begin(115200);
  Serial.println("EdenSense 温度监控系统启动...");
  
  // 初始化显示屏
  tft.init();
  tft.setRotation(0);  // 旋转180度 (0-3: 0°, 90°, 180°, 270°)
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // 初始化背光控制
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);  // 默认开启背光
  
  // 初始化按键
  button1.attachClick(onButton1Click);
  button2.attachClick(onButton2Click);
  button3.attachClick(onButton3Click);
  button4.attachClick(onButton4Click);
  
  // 初始化温度传感器
  sensors.begin();
  totalSensors = sensors.getDeviceCount();
  Serial.print("找到 ");
  Serial.print(totalSensors);
  Serial.println(" 个温度传感器");
  
  // 初始化温度记录数组
  for (int i = 0; i < totalSensors; i++) {
    sensorRecords[i].recordCount = 0;
    sensorRecords[i].currentIndex = 0;
  }
  
  // 初始化显示状态
  displayState.lastMode = currentMode;
  displayState.lastSelectedSensor = -1;
  for (int i = 0; i < totalSensors; i++) {
    displayState.lastTemps[i] = 0;
    displayState.lastAlarmStates[i][0] = false;
    displayState.lastAlarmStates[i][1] = false;
  }
  
  // 首次显示
  displayState.needsRedraw = true;
}

void loop() {
  // 更新按键状态
  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();
  
  // 如果屏幕关闭，只处理按键，不更新显示
  if (!screenOn) {
    return;
  }
  
  // 定期更新温度
  unsigned long currentMillis = millis();
  if (currentMillis - lastTempUpdate >= tempUpdateInterval) {
    // 请求所有传感器更新温度
    sensors.requestTemperatures();
    
    // 更新温度记录
    updateTempRecords();
    
    // 检查所有传感器的温度报警状态
    for (int i = 0; i < totalSensors; i++) {
      float tempC = sensors.getTempCByIndex(i);
      if (tempC != DEVICE_DISCONNECTED_C) {
        checkTemperatureAlarms(i, tempC);
      }
    }
    
    // 根据当前模式更新显示
    switch (currentMode) {
      case MODE_OVERVIEW:
        displayOverview();
        break;
      case MODE_DETAIL:
        if (selectedSensor >= 0 && selectedSensor < totalSensors) {
          displayDetailView(selectedSensor);
        }
        break;
      case MODE_GRAPH:
        if (selectedSensor >= 0 && selectedSensor < totalSensors) {
          drawGraph(selectedSensor);
        }
        break;
    }
    
    lastTempUpdate = currentMillis;
  }
  
  // 处理报警闪烁
  for (int i = 0; i < totalSensors; i++) {
    if (alarmStates[i].highAlarm || alarmStates[i].lowAlarm) {
      if (currentMillis - alarmStates[i].lastBlinkTime >= ALARM_BLINK_INTERVAL) {
        alarmStates[i].blinkState = !alarmStates[i].blinkState;
        alarmStates[i].lastBlinkTime = currentMillis;
        displayState.needsRedraw = true;  // 触发重绘以更新报警状态显示
      }
    }
  }
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