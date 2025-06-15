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
#define KEY4_PIN   38    // K4: 屏幕开关

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
#define MAX_RECORDS 120  // 保持120个数据点
#define RECORD_INTERVAL 10  // 每10秒记录一次（保持不变）
#define TEMP_UPDATE_INTERVAL 10000  // 温度显示更新间隔（1秒）
#define TEMP_STORE_INTERVAL 10000  // 温度存储间隔（10秒）

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

unsigned long lastTempUpdate = 0;  // 上次温度更新时间
const int tempUpdateInterval = 10000;  // 温度更新间隔（毫秒）

bool firstDraw = true;      // 首次绘制标志
float lastTemps[16] = {0};  // 假设最多16个传感器
bool screenOn = true;  // 屏幕开关状态

DisplayMode currentMode = MODE_OVERVIEW;
TempRecord sensorRecords[16];  // 假设最多16个传感器
GraphState graphState = {0};   // 图表状态
AlarmState alarmStates[16];  // 每个传感器的报警状态
DisplayState displayState = {false, -1, MODE_OVERVIEW, {0}, {{false}}};

// 添加新的全局变量
unsigned long lastScreenCommandTime = 0;
bool screenCommandPending = false;
bool screenCommandType = false;  // false = 关闭命令, true = 开启命令
unsigned long lastTempRequestTime = 0;
bool tempRequestPending = false;
unsigned long lastTempStoreTime = 0;  // 上次温度存储时间
unsigned long lastTempUpdateTime = 0;  // 上次温度显示更新时间
bool displayNeedsUpdate = false;  // 标记是否需要更新显示

// 添加新的全局变量
unsigned long lastTempReadTime = 0;  // 上次读取温度的时间
float currentTemps[16] = {0};       // 当前温度值缓存
bool tempReadPending = false;       // 温度读取状态标志

// 全局对象定义
TFT_eSPI tft;

OneWire oneWire(ONEWIRE_BUS);
DallasTemperature sensors(&oneWire);

OneButton button1(KEY1_PIN, true);  // 使用内部上拉，启用消抖
OneButton button2(KEY2_PIN, true);
OneButton button3(KEY3_PIN, true);
OneButton button4(KEY4_PIN, true);

// 函数声明
void drawGraphBackground(int sensorIndex);
void updateTempInfo(float minTemp, float maxTemp, float avgTemp, float currentTemp);
void drawGraph(int sensorIndex);
void displayOverview();
void displayDetailView(int index);
void onButton1Click();
void onButton2Click();
void onButton3Click();
void onButton4Click();
void checkTemperatureAlarms(int sensorIndex, float temp);
void updateDisplay();
void readTemperatures();

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
    
    // 使用当前温度值初始化状态变量
    float currentTemp = currentTemps[sensorIndex];
    if (currentTemp != DEVICE_DISCONNECTED_C) {
      lastMinTemp = currentTemp;
      lastMaxTemp = currentTemp;
      lastAvgTemp = currentTemp;
      lastCurrentTemp = currentTemp;
    }
    lastRecordCount = sensorRecords[sensorIndex].recordCount;
  }
  
  // 获取温度数据
  float minTemp = 999;
  float maxTemp = -999;
  float sumTemp = 0;
  float currentTemp = currentTemps[sensorIndex];
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
  bool tempChanged[16] = {false};  // 记录每个传感器的温度是否变化
  for (int i = 0; i < totalSensors; i++) {
    float tempC = currentTemps[i];
    if (abs(tempC - displayState.lastTemps[i]) >= 0.1 ||
        alarmStates[i].highAlarm != displayState.lastAlarmStates[i][0] ||
        alarmStates[i].lowAlarm != displayState.lastAlarmStates[i][1]) {
      tempChanged[i] = true;
      needsRedraw = true;
    }
  }
  
  if (!needsRedraw) {
    return;  // 如果没有变化，不进行重绘
  }
  
  // 如果是模式切换，清除整个屏幕
  if (displayState.lastMode != currentMode) {
    tft.fillScreen(TFT_BLACK);
    
    // 显示标题
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("温度概览", SCREEN_WIDTH/2, TITLE_HEIGHT/2);
  }
  
  // 计算每行的高度和起始位置
  int rowHeight = OVERVIEW_ROW_HEIGHT;
  int startY = TITLE_HEIGHT + 5;  // 标题下方留出5像素间距
  
  // 显示所有传感器温度
  for (int i = 0; i < totalSensors; i++) {
    float tempC = currentTemps[i];
    int rowY = startY + (i * rowHeight);
    
    // 如果温度没有变化且不是首次显示，跳过更新
    if (!tempChanged[i] && !displayState.needsRedraw && displayState.lastMode == currentMode) {
      continue;
    }
    
    // 清除该行的显示区域
    tft.fillRect(0, rowY, SCREEN_WIDTH, rowHeight, TFT_BLACK);
    
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
  
  float tempC = currentTemps[sensorIndex];
  bool tempChanged = abs(tempC - displayState.lastTemps[sensorIndex]) >= 0.1;
  bool alarmChanged = alarmStates[sensorIndex].highAlarm != displayState.lastAlarmStates[sensorIndex][0] ||
                     alarmStates[sensorIndex].lowAlarm != displayState.lastAlarmStates[sensorIndex][1];
  
  if (!needsRedraw && !tempChanged && !alarmChanged) {
    return;  // 如果没有变化，不进行重绘
  }
  
  // 如果是模式切换或传感器切换，清除整个屏幕
  if (displayState.lastMode != currentMode || displayState.lastSelectedSensor != sensorIndex) {
    tft.fillScreen(TFT_BLACK);
    
    // 显示传感器编号（T1, T2格式）
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("T" + String(sensorIndex + 1), SCREEN_WIDTH/2, TITLE_HEIGHT/2);
    
    // 显示报警阈值（只在完全重绘时显示）
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  
  // 更新状态记录
  displayState.lastTemps[sensorIndex] = tempC;
  displayState.lastAlarmStates[sensorIndex][0] = alarmStates[sensorIndex].highAlarm;
  displayState.lastAlarmStates[sensorIndex][1] = alarmStates[sensorIndex].lowAlarm;
  displayState.lastSelectedSensor = sensorIndex;
  
  if (tempC != DEVICE_DISCONNECTED_C) {
    // 清除温度显示区域
    tft.fillRect(0, 40, SCREEN_WIDTH, 60, TFT_BLACK);
    
    // 显示温度值
    tft.setTextSize(3);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(String(tempC, 1) + "C", SCREEN_WIDTH/2, 60);
    
    // 清除报警状态显示区域
    tft.fillRect(0, 80, SCREEN_WIDTH, 20, TFT_BLACK);
    
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
  } else {
    // 清除整个显示区域
    tft.fillRect(0, 40, SCREEN_WIDTH, 80, TFT_BLACK);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("传感器未连接", SCREEN_WIDTH/2, 60);
  }
  
  displayState.lastMode = currentMode;
  displayState.needsRedraw = false;
}

// 修改按键回调函数
void onButton1Click() {
  unsigned long clickTime = millis();
  DisplayMode oldMode = currentMode;
  currentMode = (DisplayMode)((currentMode + 1) % 3);
  if (currentMode == MODE_OVERVIEW) {
    selectedSensor = -1;
  } else if (selectedSensor == -1) {
    selectedSensor = 0;
  }
  displayNeedsUpdate = true;  // 标记需要更新显示
  
  // 添加详细日志
  Serial.print("[按键1] 时间戳: ");
  Serial.print(clickTime);
  Serial.print("ms, 模式切换: ");
  Serial.print(oldMode);
  Serial.print(" -> ");
  Serial.print(currentMode);
  Serial.print(", 选中传感器: ");
  Serial.println(selectedSensor);
}

void onButton2Click() {
  unsigned long clickTime = millis();
  int oldSensor = selectedSensor;
  
  if (currentMode == MODE_DETAIL || currentMode == MODE_GRAPH) {
    if (selectedSensor == -1) {
      selectedSensor = 0;
    } else {
      selectedSensor = (selectedSensor + 1) % totalSensors;
    }
    displayNeedsUpdate = true;  // 标记需要更新显示
    
    // 添加详细日志
    Serial.print("[按键2] 时间戳: ");
    Serial.print(clickTime);
    Serial.print("ms, 当前模式: ");
    Serial.print(currentMode);
    Serial.print(", 传感器切换: ");
    Serial.print(oldSensor);
    Serial.print(" -> ");
    Serial.println(selectedSensor);
  } else {
    // 添加详细日志 - 按键在非相关模式下被按下
    Serial.print("[按键2] 时间戳: ");
    Serial.print(clickTime);
    Serial.print("ms, 当前模式: ");
    Serial.print(currentMode);
    Serial.println(", 按键在当前模式下无效");
  }
}

void onButton3Click() {
  unsigned long clickTime = millis();
  int oldSensor = selectedSensor;
  
  if (currentMode == MODE_DETAIL || currentMode == MODE_GRAPH) {
    if (selectedSensor == -1) {
      selectedSensor = totalSensors - 1;
    } else {
      selectedSensor = (selectedSensor - 1 + totalSensors) % totalSensors;
    }
    displayNeedsUpdate = true;  // 标记需要更新显示
    
    // 添加详细日志
    Serial.print("[按键3] 时间戳: ");
    Serial.print(clickTime);
    Serial.print("ms, 当前模式: ");
    Serial.print(currentMode);
    Serial.print(", 传感器切换: ");
    Serial.print(oldSensor);
    Serial.print(" -> ");
    Serial.println(selectedSensor);
  } else {
    // 添加详细日志 - 按键在非相关模式下被按下
    Serial.print("[按键3] 时间戳: ");
    Serial.print(clickTime);
    Serial.print("ms, 当前模式: ");
    Serial.print(currentMode);
    Serial.println(", 按键在当前模式下无效");
  }
}

void onButton4Click() {
  unsigned long clickTime = millis();
  bool oldScreenState = screenOn;
  screenOn = !screenOn;
  
  // 设置屏幕命令状态
  screenCommandPending = true;
  screenCommandType = screenOn;
  lastScreenCommandTime = clickTime;
  
  // 添加详细日志
  Serial.print("[按键4] 时间戳: ");
  Serial.print(clickTime);
  Serial.print("ms, 屏幕状态切换: ");
  Serial.print(oldScreenState ? "开启" : "关闭");
  Serial.print(" -> ");
  Serial.println(screenOn ? "开启" : "关闭");
}

void setup() {
  Serial.begin(115200);
  Serial.println("EdenSense 温度监控系统启动...");

  // 初始化温度传感器
  sensors.begin();
  totalSensors = sensors.getDeviceCount();
  
  // 初始化显示屏
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // 初始化背光控制
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  // 初始化按键
  button1.attachClick(onButton1Click);
  button2.attachClick(onButton2Click);
  button3.attachClick(onButton3Click);
  button4.attachClick(onButton4Click);
  
  // 初始化温度记录数组并获取初始温度值
  for (int i = 0; i < totalSensors; i++) {
    sensorRecords[i].recordCount = 0;
    sensorRecords[i].currentIndex = 0;
    
    // 获取初始温度值
    sensors.requestTemperatures();
    float initialTemp = sensors.getTempCByIndex(i);
    if (initialTemp != DEVICE_DISCONNECTED_C) {
      // 使用初始温度值初始化记录
      sensorRecords[i].temps[0] = initialTemp;
      sensorRecords[i].timestamps[0] = millis();
      sensorRecords[i].recordCount = 1;
      sensorRecords[i].currentIndex = 1;
      
      // 初始化显示状态
      displayState.lastTemps[i] = initialTemp;
      
      // 初始化图表状态
      graphState.lastMinTemp = initialTemp;
      graphState.lastMaxTemp = initialTemp;
      graphState.lastAvgTemp = initialTemp;
      graphState.lastCurrentTemp = initialTemp;
    }
  }
  
  // 初始化显示状态
  displayState.lastMode = currentMode;
  displayState.lastSelectedSensor = -1;
  displayState.needsRedraw = true;
  
  // 首次显示
  updateDisplay();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // 更新按键状态
  button1.tick();
  button2.tick();
  button3.tick();
  button4.tick();
  
  // 处理屏幕命令
  if (screenCommandPending) {
    if (screenCommandType) {  // 开启屏幕
      if (currentMillis - lastScreenCommandTime >= 120) {
        tft.writecommand(0x29);
        digitalWrite(TFT_BL, HIGH);
        screenCommandPending = false;
      } else if (currentMillis - lastScreenCommandTime == 0) {
        tft.writecommand(0x11);
      }
    } else {  // 关闭屏幕
      tft.writecommand(0x10);
      digitalWrite(TFT_BL, LOW);
      screenCommandPending = false;
    }
  }
  
  // 如果屏幕关闭，只处理按键和屏幕命令
  if (!screenOn) {
    return;
  }
  
  // 读取温度数据
  readTemperatures();
  
  // 立即处理显示更新（按键触发）
  updateDisplay();
  
  // 处理报警闪烁
  for (int i = 0; i < totalSensors; i++) {
    if (alarmStates[i].highAlarm || alarmStates[i].lowAlarm) {
      if (currentMillis - alarmStates[i].lastBlinkTime >= ALARM_BLINK_INTERVAL) {
        alarmStates[i].blinkState = !alarmStates[i].blinkState;
        alarmStates[i].lastBlinkTime = currentMillis;
        displayNeedsUpdate = true;
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

// 添加显示更新函数
void updateDisplay() {
  if (!displayNeedsUpdate && !displayState.needsRedraw) {
    return;  // 如果没有需要更新的内容，直接返回
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
  
  displayNeedsUpdate = false;  // 清除显示更新标记
  displayState.needsRedraw = false;  // 清除状态更新标记
}

// 添加温度读取函数
void readTemperatures() {
  unsigned long currentMillis = millis();
  
  // 检查是否需要读取温度
  if (!tempReadPending) {
    if (currentMillis - lastTempReadTime >= TEMP_UPDATE_INTERVAL) {
      sensors.requestTemperatures();
      tempReadPending = true;
      lastTempReadTime = currentMillis;
    }
  } else {
    // 检查温度是否已经准备好（给传感器100ms的响应时间）
    if (currentMillis - lastTempReadTime >= 100) {
      // 读取所有传感器的温度
      for (int i = 0; i < totalSensors; i++) {
        float tempC = sensors.getTempCByIndex(i);
        if (tempC != DEVICE_DISCONNECTED_C) {
          currentTemps[i] = tempC;  // 更新温度缓存
          
          // 检查是否需要存储到记录数组
          if (currentMillis - lastTempStoreTime >= TEMP_STORE_INTERVAL) {
            sensorRecords[i].temps[sensorRecords[i].currentIndex] = tempC;
            sensorRecords[i].timestamps[sensorRecords[i].currentIndex] = currentMillis;
            
            // 更新索引和计数
            sensorRecords[i].currentIndex = (sensorRecords[i].currentIndex + 1) % MAX_RECORDS;
            if (sensorRecords[i].recordCount < MAX_RECORDS) {
              sensorRecords[i].recordCount++;
            }
            
            // 打印存储信息
            Serial.print("传感器 ");
            Serial.print(i);
            Serial.print(" 存储温度: ");
            Serial.print(tempC);
            Serial.print("C, 存储位置: ");
            Serial.print(sensorRecords[i].currentIndex);
            Serial.print(", 总记录数: ");
            Serial.println(sensorRecords[i].recordCount);
          }
          
          // 检查报警状态
          checkTemperatureAlarms(i, tempC);
          if (alarmStates[i].highAlarm || alarmStates[i].lowAlarm) {
            displayNeedsUpdate = true;
          }
        }
      }
      
      // 如果是存储时间点，更新存储时间
      if (currentMillis - lastTempStoreTime >= TEMP_STORE_INTERVAL) {
        lastTempStoreTime = currentMillis;
      }
      
      tempReadPending = false;
    }
  }
} 