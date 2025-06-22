#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <OneButton.h>
#include <time.h>
#include <stdio.h>  // 为sprintf添加
#include <WiFi.h>   // 添加WiFi库
#include <esp_wifi.h>  // 添加ESP32 WiFi控制库
#include <PubSubClient.h>  // 添加MQTT客户端库
#include <ArduinoJson.h>   // 添加JSON库
#include <WiFiClientSecure.h>  // 添加SSL客户端库

// WiFi连接参数
#define WIFI_SSID "MTK_CHEETAH_AP_2.4G"      // 请修改为您的WiFi名称
#define WIFI_PASSWORD "5680578abc.."  // 请修改为您的WiFi密码
#define WIFI_TIMEOUT 10000           // WiFi连接超时时间（毫秒）

// WiFi功率设置
#define WIFI_POWER_DBM 8             // WiFi发射功率，单位dBm (范围: 0-20, 建议8-12)

// 电源管理和功耗优化配置
#define SCREEN_BRIGHTNESS 128        // 屏幕亮度 (0-255, 建议128-180)
#define MQTT_KEEPALIVE 60            // MQTT保活时间 (秒)
#define WIFI_SLEEP_DISABLE true      // 禁用WiFi睡眠模式
#define CPU_FREQ_MHZ 80              // CPU频率 (80MHz降低功耗)

// MQTT配置参数
#define MQTT_SERVER "ebd16e9d.ala.cn-hangzhou.emqxsl.cn"  // MQTT服务器地址
#define MQTT_PORT 8883               // MQTT服务器端口
#define MQTT_CLIENT_ID "esp32c3print"  // MQTT客户端ID
#define MQTT_USERNAME "esp32c3print"             // MQTT用户名（如果需要）
#define MQTT_PASSWORD "yjMMjxnbcPNifc8"             // MQTT密码（如果需要）
#define MQTT_SUBSCRIBE_TOPIC "testtopic"  // 订阅主题
#define MQTT_PUBLISH_TOPIC "testtopic"       // 发布主题

// WiFi连接状态
bool wifiConnected = false;
unsigned long wifiConnectStartTime = 0;
int wifiConnectAttempts = 0;
const int MAX_WIFI_ATTEMPTS = 5;

// MQTT连接状态
bool mqttConnected = false;
unsigned long mqttConnectStartTime = 0;
int mqttConnectAttempts = 0;
const int MAX_MQTT_ATTEMPTS = 5;
bool mqttDataRequested = false;  // 标记是否需要发送数据

// 类型定义
typedef uint8_t DeviceAddress[8];  // 添加DeviceAddress类型定义

// // 按键定义
// #define KEY1_PIN   35    // K1: 切换显示模式
// #define KEY2_PIN   36    // K2: 上翻
// #define KEY3_PIN   37    // K3: 下翻
// #define KEY4_PIN   38    // K4: 屏幕开关
// // DS18B20引脚定义
// #define ONEWIRE_BUS 4    // 使用GPIO4作为数据线




// 按键定义
#define KEY1_PIN   0    // K1: 切换显示模式
#define KEY2_PIN   1    // K2: 上翻
#define KEY3_PIN   2    // K3: 下翻
#define KEY4_PIN   3    // K4: 屏幕开关

// DS18B20引脚定义
#define ONEWIRE_BUS 20    // 使用GPIO4作为数据线



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
#define TEMP_UPDATE_INTERVAL 5000  // 温度显示更新间隔（1秒）
// #define TEMP_STORE_INTERVAL 720000  // 温度存储间隔（12分钟，单位：毫秒）
#define TEMP_STORE_INTERVAL 5000  // 温度存储间隔（12分钟，单位：毫秒）

#define TEMP_MIN 10.0  // 最小温度刻度
#define TEMP_MAX 45.0  // 最大温度刻度

#define TEMP_STEP 5.0  // 温度刻度间隔
#define GRID_X_SPACING 12  // 垂直网格线间距（像素）

// 温度报警相关定义
#define TEMP_ALARM_HIGH 30.0  // 高温报警阈值
#define TEMP_ALARM_LOW 10.0   // 低温报警阈值
#define ALARM_BLINK_INTERVAL 500  // 报警闪烁间隔（毫秒）

#define TFT_BL 7

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
  float minTemp;    // 最小温度
  float maxTemp;    // 最大温度
  float avgTemp;    // 平均温度
  unsigned long lastStatsUpdate;  // 上次统计数据更新时间
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

// 函数前向声明
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
void connectWiFi();           // 添加WiFi连接函数
void checkWiFiStatus();       // 添加WiFi状态检查函数
void displayWiFiStatus();     // 添加WiFi状态显示函数
void displayMQTTStatus();     // 添加MQTT状态显示函数
void connectMQTT();           // 添加MQTT连接函数
void checkMQTTStatus();       // 添加MQTT状态检查函数
void mqttCallback(char* topic, byte* payload, unsigned int length);  // MQTT消息回调函数
void publishTemperatureData();  // 发布温度数据函数
String createTemperatureJSON(); // 创建温度JSON数据函数
void setupPowerManagement();   // 电源管理设置函数
void monitorPowerStatus();     // 电源状态监控函数
void printSystemInfo();        // 打印系统信息函数

// 添加全局变量存储传感器序列号
DeviceAddress sensorAddresses[16];  // 存储传感器序列号

// 辅助函数实现
String getShortAddress(DeviceAddress deviceAddress) {
  String result;
  result.reserve(8);  // 预分配8个字符的空间
  
  // 只使用序列号的后4个字节，转换为大写16进制字符串
  for (int i = 4; i < 8; i++) {
    char hex[3];
    sprintf(hex, "%02X", deviceAddress[i]);
    result += hex;
  }
  return result;
}

static String getSensorTitle(int sensorIndex) {
  String shortAddr = getShortAddress(sensorAddresses[sensorIndex]);
  return "T" + String(sensorIndex + 1) + "/" + shortAddr;
}

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

WiFiClientSecure espClient;  // 使用SSL客户端
PubSubClient mqttClient(espClient);

// SSL配置（用于EMQX云服务）
#define MQTT_USE_SSL true  // 启用SSL连接

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
    
    // 显示传感器编号和序列号
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    
    // 生成序列号显示
    String shortAddr;
    shortAddr.reserve(8);
    for (int i = 4; i < 8; i++) {
      char hex[3];
      sprintf(hex, "%02X", sensorAddresses[sensorIndex][i]);
      shortAddr += hex;
    }
    String title = "T" + String(sensorIndex + 1) + "/" + shortAddr;
    tft.drawString(title, SCREEN_WIDTH/2, TITLE_HEIGHT/2);
    
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
  
  // 使用存储的统计数据
  float minTemp = sensorRecords[sensorIndex].minTemp;
  float maxTemp = sensorRecords[sensorIndex].maxTemp;
  float avgTemp = sensorRecords[sensorIndex].avgTemp;
  float currentTemp = currentTemps[sensorIndex];
  
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
    
    // 显示传感器编号和序列号
    tft.setTextSize(OVERVIEW_SENSOR_SIZE);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(TL_DATUM);
    
    // 生成序列号显示
    String shortAddr;
    shortAddr.reserve(8);
    for (int j = 4; j < 8; j++) {
      char hex[3];
      sprintf(hex, "%02X", sensorAddresses[i][j]);
      shortAddr += hex;
    }
    String title = "T" + String(i + 1) + ":"; 
    tft.setTextSize(OVERVIEW_TEMP_SIZE);
    tft.drawString(title, 5, rowY);
    
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
  
  // 显示WiFi状态图标
  if (wifiConnected) {
    displayWiFiStatus();
  }
  
  // 显示MQTT状态图标
  displayMQTTStatus();
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
    
    // 显示传感器编号和序列号
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    
    // 生成序列号显示
    String shortAddr;
    shortAddr.reserve(8);
    for (int i = 4; i < 8; i++) {
      char hex[3];
      sprintf(hex, "%02X", sensorAddresses[sensorIndex][i]);
      shortAddr += hex;
    }
    String title = "T" + String(sensorIndex + 1) + "/" + shortAddr;
    tft.drawString(title, SCREEN_WIDTH/2, TITLE_HEIGHT/2);
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
    tft.setTextSize(4);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(String(tempC, 1) + "C", SCREEN_WIDTH/2, 40);
    
    // 使用存储的统计数据
    float minTemp = sensorRecords[sensorIndex].minTemp;
    float maxTemp = sensorRecords[sensorIndex].maxTemp;
    float avgTemp = sensorRecords[sensorIndex].avgTemp;
    
    // 清除统计信息显示区域
    tft.fillRect(0, 80, SCREEN_WIDTH, 60, TFT_BLACK);
    
    // 设置统计信息文本属性
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    
    // 显示最大温度（红色）
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Max: " + String(maxTemp, 1) + "C", SCREEN_WIDTH/2, 75);
    
    // 显示平均温度（黄色）
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Avg: " + String(avgTemp, 1) + "C", SCREEN_WIDTH/2, 95);
    
    // 显示最小温度（蓝色）
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
    tft.drawString("Min: " + String(minTemp, 1) + "C", SCREEN_WIDTH/2, 115);
  } else {
    // 清除整个显示区域
    tft.fillRect(0, 40, SCREEN_WIDTH, 100, TFT_BLACK);
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

  // 初始化电源管理
  setupPowerManagement();

  // 初始化WiFi
  Serial.println("初始化WiFi...");
  WiFi.mode(WIFI_STA);  // 设置为站点模式
  WiFi.setAutoReconnect(true);  // 启用自动重连
  
  // 设置WiFi发射功率为较低值
  Serial.print("设置WiFi发射功率为: ");
  Serial.print(WIFI_POWER_DBM);
  Serial.println(" dBm");
  esp_wifi_set_max_tx_power(WIFI_POWER_DBM * 4);  // ESP32使用0.25dBm为单位
  
  // 开始WiFi连接
  connectWiFi();
  
  // 初始化MQTT（在WiFi连接成功后）
  Serial.println("初始化MQTT客户端...");
  mqttClient.setBufferSize(8192);  // 设置缓冲区大小为8KB

  // 初始化温度传感器
  sensors.begin();
  totalSensors = sensors.getDeviceCount();
  
  // 获取所有传感器的序列号
  for (int i = 0; i < totalSensors; i++) {
    if (sensors.getAddress(sensorAddresses[i], i)) {
      Serial.print("传感器 ");
      Serial.print(i);
      Serial.print(" 序列号: ");
      for (uint8_t j = 0; j < 8; j++) {
        Serial.print(sensorAddresses[i][j], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
  
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
    sensorRecords[i].minTemp = DEVICE_DISCONNECTED_C;
    sensorRecords[i].maxTemp = DEVICE_DISCONNECTED_C;
    sensorRecords[i].avgTemp = DEVICE_DISCONNECTED_C;
    sensorRecords[i].lastStatsUpdate = 0;
    
    // 获取初始温度值
    sensors.requestTemperatures();
    float initialTemp = sensors.getTempCByIndex(i);
    if (initialTemp != DEVICE_DISCONNECTED_C) {
      // 使用初始温度值初始化记录
      sensorRecords[i].temps[0] = initialTemp;
      sensorRecords[i].timestamps[0] = millis();
      sensorRecords[i].recordCount = 1;
      sensorRecords[i].currentIndex = 1;
      
      // 初始化统计数据
      sensorRecords[i].minTemp = initialTemp;
      sensorRecords[i].maxTemp = initialTemp;
      sensorRecords[i].avgTemp = initialTemp;
      sensorRecords[i].lastStatsUpdate = millis();
      
      // 打印初始化信息
      Serial.println("\n=== 传感器初始化 ===");
      Serial.print("传感器 ");
      Serial.print(i);
      Serial.print(" (T");
      Serial.print(i + 1);
      Serial.println(")");
      Serial.print("初始温度: ");
      Serial.print(initialTemp);
      Serial.println("C");
      Serial.print("时间戳: ");
      Serial.println(millis());
      Serial.println("====================\n");
      
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
  
  // WiFi连接和状态检查
  if (!wifiConnected) {
    connectWiFi();
  } else {
    checkWiFiStatus();
    
    // WiFi连接成功后，处理MQTT
    if (!mqttConnected) {
      connectMQTT();
    } else {
      checkMQTTStatus();
    }
  }
  
  // 处理MQTT数据请求
  if (mqttDataRequested && mqttConnected) {
    publishTemperatureData();
  }
  
  // 监控电源状态
  monitorPowerStatus();
  
  // 打印系统信息
  printSystemInfo();
  
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
  
  // 读取温度数据
  readTemperatures();
  
  // 立即处理显示更新（按键触发）
  updateDisplay();
  
  // 显示WiFi状态图标
  if (wifiConnected) {
    displayWiFiStatus();
  }
  
  // 显示MQTT状态图标
  displayMQTTStatus();
  
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
          
          // 检查是否需要存储到记录数组（每12分钟存储一次）
          if (currentMillis - lastTempStoreTime >= TEMP_STORE_INTERVAL) {
            // 存储温度数据
            sensorRecords[i].temps[sensorRecords[i].currentIndex] = tempC;
            sensorRecords[i].timestamps[sensorRecords[i].currentIndex] = currentMillis;
            
            // 更新索引和计数
            sensorRecords[i].currentIndex = (sensorRecords[i].currentIndex + 1) % MAX_RECORDS;
            if (sensorRecords[i].recordCount < MAX_RECORDS) {
              sensorRecords[i].recordCount++;
            }
            
            // 重新计算统计数据
            float minTemp = 999.0;  // 使用一个较大的初始值
            float maxTemp = -999.0; // 使用一个较小的初始值
            float sumTemp = 0.0;
            int validCount = 0;
            
            // 遍历所有记录计算统计数据
            for (int j = 0; j < sensorRecords[i].recordCount; j++) {
              float temp = sensorRecords[i].temps[j];
              if (temp != DEVICE_DISCONNECTED_C) {
                minTemp = min(minTemp, temp);
                maxTemp = max(maxTemp, temp);
                sumTemp += temp;
                validCount++;
              }
            }
            
            // 更新统计数据
            if (validCount > 0) {
              sensorRecords[i].minTemp = minTemp;
              sensorRecords[i].maxTemp = maxTemp;
              sensorRecords[i].avgTemp = sumTemp / validCount;
              sensorRecords[i].lastStatsUpdate = currentMillis;
              
              // 打印详细的统计信息用于调试
              Serial.println("\n=== 温度统计数据更新 ===");
              Serial.print("时间戳: ");
              Serial.println(currentMillis);
              Serial.print("传感器 ");
              Serial.print(i);
              Serial.print(" (T");
              Serial.print(i + 1);
              Serial.println(")");
              Serial.print("当前温度: ");
              Serial.print(tempC);
              Serial.println("C");
              Serial.print("记录数量: ");
              Serial.println(sensorRecords[i].recordCount);
              Serial.print("有效数据: ");
              Serial.println(validCount);
              Serial.print("最小温度: ");
              Serial.print(sensorRecords[i].minTemp);
              Serial.println("C");
              Serial.print("最大温度: ");
              Serial.print(sensorRecords[i].maxTemp);
              Serial.println("C");
              Serial.print("平均温度: ");
              Serial.print(sensorRecords[i].avgTemp);
              Serial.println("C");
              Serial.print("数据数组: [");
              for (int j = 0; j < sensorRecords[i].recordCount; j++) {
                Serial.print(sensorRecords[i].temps[j]);
                Serial.print(", ");
              }
              Serial.println("]");
              Serial.println("====================\n");
            } else {
              // 如果没有有效数据，使用当前温度作为所有统计值
              sensorRecords[i].minTemp = tempC;
              sensorRecords[i].maxTemp = tempC;
              sensorRecords[i].avgTemp = tempC;
              sensorRecords[i].lastStatsUpdate = currentMillis;
              
              Serial.println("\n=== 温度统计数据初始化 ===");
              Serial.print("传感器 ");
              Serial.print(i);
              Serial.print(" 使用当前温度初始化: ");
              Serial.print(tempC);
              Serial.println("C");
              Serial.println("====================\n");
            }
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

// WiFi连接函数
void connectWiFi() {
  if (wifiConnected) {
    return;  // 如果已经连接，直接返回
  }
  
  unsigned long currentMillis = millis();
  
  // 如果是第一次尝试连接或重连
  if (wifiConnectStartTime == 0) {
    Serial.println("开始连接WiFi...");
    Serial.print("SSID: ");
    Serial.println(WIFI_SSID);
    
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    wifiConnectStartTime = currentMillis;
    wifiConnectAttempts++;
    
    // 在屏幕上显示连接状态
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("正在连接WiFi...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10);
    tft.drawString(WIFI_SSID, SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10);
  }
  
  // 检查连接状态
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    wifiConnectStartTime = 0;
    wifiConnectAttempts = 0;
    
    Serial.println("WiFi连接成功！");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
    Serial.print("信号强度: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    // 在屏幕上显示连接成功信息
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi连接成功", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 20);
    tft.drawString("IP: " + WiFi.localIP().toString(), SCREEN_WIDTH/2, SCREEN_HEIGHT/2);
    tft.drawString("信号: " + String(WiFi.RSSI()) + " dBm", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 20);
    
    // 延迟2秒后恢复正常显示
    delay(2000);
    displayState.needsRedraw = true;
    
  } else if (currentMillis - wifiConnectStartTime > WIFI_TIMEOUT) {
    // 连接超时
    Serial.println("WiFi连接超时");
    
    if (wifiConnectAttempts < MAX_WIFI_ATTEMPTS) {
      Serial.print("重试连接 (");
      Serial.print(wifiConnectAttempts);
      Serial.print("/");
      Serial.print(MAX_WIFI_ATTEMPTS);
      Serial.println(")");
      
      // 在屏幕上显示重试信息
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_ORANGE, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("WiFi连接超时", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10);
      tft.drawString("重试中...", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10);
      
      // 重新开始连接
      WiFi.disconnect();
      delay(1000);
      wifiConnectStartTime = 0;
    } else {
      // 达到最大重试次数
      Serial.println("WiFi连接失败，达到最大重试次数");
      
      // 在屏幕上显示连接失败信息
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextSize(1);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("WiFi连接失败", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 - 10);
      tft.drawString("请检查网络设置", SCREEN_WIDTH/2, SCREEN_HEIGHT/2 + 10);
      
      // 延迟3秒后恢复正常显示
      delay(3000);
      displayState.needsRedraw = true;
      
      // 重置连接状态
      wifiConnectStartTime = 0;
      wifiConnectAttempts = 0;
    }
  }
}

// WiFi状态检查函数
void checkWiFiStatus() {
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    // WiFi连接丢失
    Serial.println("WiFi连接丢失");
    wifiConnected = false;
    displayState.needsRedraw = true;
  }
}

// WiFi状态显示函数
void displayWiFiStatus() {
  if (!wifiConnected) {
    return;  // 如果WiFi未连接，不显示状态
  }
  
  static int lastRSSI = -100;
  static unsigned long lastUpdateTime = 0;
  unsigned long currentMillis = millis();
  
  // 只在信号强度变化或每5秒更新一次
  int currentRSSI = WiFi.RSSI();
  if (currentRSSI == lastRSSI && currentMillis - lastUpdateTime < 5000) {
    return;
  }
  
  lastRSSI = currentRSSI;
  lastUpdateTime = currentMillis;
  
  // 在屏幕左上角显示WiFi状态图标
  int iconX = 15;  // 改为左侧
  int iconY = 5;
  
  // 根据信号强度选择颜色
  uint16_t color;
  
  if (currentRSSI >= -50) {
    color = TFT_GREEN;  // 信号强
  } else if (currentRSSI >= -70) {
    color = TFT_YELLOW; // 信号中等
  } else {
    color = TFT_RED;    // 信号弱
  }
  
  // 绘制WiFi图标（简化的信号强度条）
  int bars = 0;
  if (currentRSSI >= -50) bars = 4;
  else if (currentRSSI >= -60) bars = 3;
  else if (currentRSSI >= -70) bars = 2;
  else if (currentRSSI >= -80) bars = 1;
  
  // 清除图标区域
  tft.fillRect(iconX - 8, iconY, 10, 10, TFT_BLACK);
  
  for (int i = 0; i < bars; i++) {
    int barHeight = (i + 1) * 2;
    int barY = iconY + 8 - barHeight;
    tft.fillRect(iconX + i * 2, barY, 2, barHeight, color);  // 改为从左到右绘制
  }
}

// MQTT状态显示函数
void displayMQTTStatus() {
  static bool lastMQTTStatus = false;
  static unsigned long lastUpdateTime = 0;
  unsigned long currentMillis = millis();
  
  // 只在状态变化或每3秒更新一次
  if (mqttConnected == lastMQTTStatus && currentMillis - lastUpdateTime < 3000) {
    return;
  }
  
  lastMQTTStatus = mqttConnected;
  lastUpdateTime = currentMillis;
  
  // 在屏幕右上角显示MQTT状态
  int iconX = SCREEN_WIDTH - 15;
  int iconY = 5;
  
  // 清除图标区域
  tft.fillRect(iconX - 8, iconY, 10, 10, TFT_BLACK);
  
  // 设置文本属性
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  
  if (mqttConnected) {
    // 显示绿色"M"表示MQTT已连接
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("M", iconX, iconY + 5);
  } else {
    // 显示红色"M"表示MQTT未连接
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("M", iconX, iconY + 5);
  }
}

void connectMQTT() {
  if (mqttConnected || !wifiConnected) {
    return;  // 如果已经连接或WiFi未连接，直接返回
  }
  
  unsigned long currentMillis = millis();
  
  // 如果是第一次尝试连接或重连
  if (mqttConnectStartTime == 0) {
    Serial.println("开始连接MQTT服务器...");
    Serial.print("服务器: ");
    Serial.print(MQTT_SERVER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    
    // 配置SSL（用于EMQX云服务）
    if (MQTT_USE_SSL) {
      Serial.println("配置SSL连接...");
      espClient.setCACert(NULL);  // 不验证服务器证书
      espClient.setInsecure();    // 允许不安全的连接
    }
    
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE);  // 设置保活时间
    
    mqttConnectStartTime = currentMillis;
    mqttConnectAttempts++;
  }
  
  // 尝试连接MQTT服务器
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD)) {
    mqttConnected = true;
    mqttConnectStartTime = 0;
    mqttConnectAttempts = 0;
    
    Serial.println("MQTT连接成功！");
    
    // 订阅命令主题
    if (mqttClient.subscribe(MQTT_SUBSCRIBE_TOPIC)) {
      Serial.print("成功订阅主题: ");
      Serial.println(MQTT_SUBSCRIBE_TOPIC);
    } else {
      Serial.println("订阅主题失败");
    }
    
  } else {
    // 连接失败
    Serial.print("MQTT连接失败，错误代码: ");
    Serial.println(mqttClient.state());
    
    if (currentMillis - mqttConnectStartTime > 5000) {  // 5秒超时
      if (mqttConnectAttempts < MAX_MQTT_ATTEMPTS) {
        Serial.print("重试MQTT连接 (");
        Serial.print(mqttConnectAttempts);
        Serial.print("/");
        Serial.print(MAX_MQTT_ATTEMPTS);
        Serial.println(")");
        
        mqttConnectStartTime = 0;  // 重置时间，准备重试
      } else {
        Serial.println("MQTT连接失败，达到最大重试次数");
        mqttConnectStartTime = 0;
        mqttConnectAttempts = 0;
      }
    }
  }
}

void checkMQTTStatus() {
  if (mqttConnected) {
    // 检查MQTT连接状态
    if (!mqttClient.connected()) {
      Serial.println("MQTT连接丢失");
      mqttConnected = false;
      mqttConnectStartTime = 0;  // 重置连接时间，准备重连
    } else {
      // 处理MQTT消息
      mqttClient.loop();
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // 将接收到的消息转换为字符串
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("收到MQTT消息 [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  // 检查是否是refresh命令
  if (message == "refresh") {
    Serial.println("收到refresh命令，准备发送温度数据");
    mqttDataRequested = true;  // 标记需要发送数据
  }
}

void publishTemperatureData() {
  if (!mqttConnected) {
    Serial.println("MQTT未连接，无法发送数据");
    return;
  }
  
  if (!mqttDataRequested) {
    return;  // 如果没有请求数据，不发送
  }
  
  Serial.println("开始创建温度数据JSON...");
  
  // 创建JSON数据
  String jsonData = createTemperatureJSON();
  
  Serial.print("JSON数据长度: ");
  Serial.println(jsonData.length());
  Serial.println("JSON数据内容:");
  Serial.println(jsonData);
  
  // 发布数据到MQTT主题
  if (mqttClient.publish(MQTT_PUBLISH_TOPIC, jsonData.c_str())) {
    Serial.print("成功发布数据到主题: ");
    Serial.println(MQTT_PUBLISH_TOPIC);
  } else {
    Serial.println("发布数据失败");
  }
  
  // 重置请求标记
  mqttDataRequested = false;
}

String createTemperatureJSON() {
  DynamicJsonDocument doc(8192);  // 分配8KB内存用于JSON文档
  
  // 为每个传感器创建数据
  for (int i = 0; i < totalSensors; i++) {
    // 生成序列号显示
    String shortAddr;
    shortAddr.reserve(8);
    for (int j = 4; j < 8; j++) {
      char hex[3];
      sprintf(hex, "%02X", sensorAddresses[i][j]);
      shortAddr += hex;
    }
    
    // 创建传感器标识符：T1-序列号格式
    String sensorKey = "T" + String(i + 1) + "-" + shortAddr;
    
    // 创建传感器对象
    JsonObject sensorObj = doc.createNestedObject(sensorKey);
    
    // 添加当前温度
    float currentTemp = currentTemps[i];
    if (currentTemp != DEVICE_DISCONNECTED_C) {
      sensorObj["c_t"] = String(currentTemp, 1);  // 保留1位小数
    } else {
      sensorObj["c_t"] = "null";
    }
    
    // 添加历史温度数组
    JsonArray historyArray = sensorObj.createNestedArray("l_t");
    
    // 获取历史温度数据
    TempRecord& record = sensorRecords[i];
    for (int j = 0; j < record.recordCount; j++) {
      float temp = record.temps[j];
      if (temp != DEVICE_DISCONNECTED_C) {
        historyArray.add(round(temp * 10) / 10.0);  // 保留1位小数
      } else {
        historyArray.add(0.0);  // 对于无效数据使用0
      }
    }
  }
  
  // 序列化JSON为字符串
  String jsonString;
  serializeJson(doc, jsonString);
  
  return jsonString;
}

void setupPowerManagement() {
  Serial.println("配置电源管理...");
  
  // 设置CPU频率为80MHz以降低功耗
  setCpuFrequencyMhz(CPU_FREQ_MHZ);
  Serial.print("CPU频率设置为: ");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" MHz");
  
  // 配置WiFi电源管理
  if (WIFI_SLEEP_DISABLE) {
    WiFi.setSleep(false);  // 禁用WiFi睡眠模式
    Serial.println("WiFi睡眠模式已禁用");
  }
  
  // 设置屏幕亮度
  analogWrite(TFT_BL, SCREEN_BRIGHTNESS);
  Serial.print("屏幕亮度设置为: ");
  Serial.println(SCREEN_BRIGHTNESS);
  
  // 配置ADC衰减器（如果使用电池供电）
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // 0-3.3V范围
  
  Serial.println("电源管理配置完成");
}

void monitorPowerStatus() {
  static unsigned long lastPowerCheck = 0;
  static int powerCheckCount = 0;
  unsigned long currentMillis = millis();
  
  // 每30秒检查一次电源状态
  if (currentMillis - lastPowerCheck >= 30000) {
    lastPowerCheck = currentMillis;
    powerCheckCount++;
    
    // 获取当前电压（如果使用电池供电）
    // float voltage = analogRead(1) * 2 * 3.3 / 4095.0;  // 分压电路
    
    // 检查WiFi连接状态
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("警告: WiFi连接丢失");
    }
    
    // 检查MQTT连接状态
    if (!mqttConnected) {
      Serial.println("警告: MQTT连接丢失");
    }
    
    // 每5分钟输出一次状态信息
    if (powerCheckCount % 10 == 0) {
      Serial.println("=== 电源状态监控 ===");
      Serial.print("运行时间: ");
      Serial.print(currentMillis / 1000);
      Serial.println(" 秒");
      Serial.print("CPU频率: ");
      Serial.print(ESP.getCpuFreqMHz());
      Serial.println(" MHz");
      Serial.print("WiFi状态: ");
      Serial.println(WiFi.status() == WL_CONNECTED ? "已连接" : "未连接");
      Serial.print("MQTT状态: ");
      Serial.println(mqttConnected ? "已连接" : "未连接");
      Serial.print("可用内存: ");
      Serial.print(ESP.getFreeHeap());
      Serial.println(" 字节");
      Serial.println("==================");
    }
  }
}

void printSystemInfo() {
  static unsigned long lastPrintTime = 0;
  unsigned long currentMillis = millis();
  
  // 每秒打印一次
  if (currentMillis - lastPrintTime >= 1000) {
    lastPrintTime = currentMillis;
    
    Serial.println("\n=== 系统信息 ===");
    Serial.print("运行时间: ");
    Serial.print(currentMillis / 1000);
    Serial.println(" 秒");
    
    // 内存信息
    Serial.print("可用内存: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" 字节");
    Serial.print("最小可用内存: ");
    Serial.print(ESP.getMinFreeHeap());
    Serial.println(" 字节");
    Serial.print("最大分配块: ");
    Serial.print(ESP.getMaxAllocHeap());
    Serial.println(" 字节");
    
    // CPU信息
    Serial.print("CPU频率: ");
    Serial.print(ESP.getCpuFreqMHz());
    Serial.println(" MHz");
    
    // WiFi信息
    Serial.print("WiFi状态: ");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("已连接");
      Serial.print("IP地址: ");
      Serial.println(WiFi.localIP());
      Serial.print("信号强度: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("未连接");
    }
    
    // MQTT信息
    Serial.print("MQTT状态: ");
    Serial.println(mqttConnected ? "已连接" : "未连接");
    
    // 屏幕信息
    Serial.print("屏幕状态: ");
    Serial.println(screenOn ? "开启" : "关闭");
    
    // 传感器信息
    Serial.print("传感器数量: ");
    Serial.println(totalSensors);
    for (int i = 0; i < totalSensors; i++) {
      Serial.print("T");
      Serial.print(i + 1);
      Serial.print(": ");
      if (currentTemps[i] != DEVICE_DISCONNECTED_C) {
        Serial.print(currentTemps[i], 1);
        Serial.println("°C");
      } else {
        Serial.println("未连接");
      }
    }
    
    // 显示模式信息
    Serial.print("显示模式: ");
    switch (currentMode) {
      case MODE_OVERVIEW:
        Serial.println("概览模式");
        break;
      case MODE_DETAIL:
        Serial.println("详情模式");
        break;
      case MODE_GRAPH:
        Serial.println("图表模式");
        break;
    }
    
    // 按键状态
    Serial.print("按键状态: K1=");
    Serial.print(digitalRead(KEY1_PIN));
    Serial.print(" K2=");
    Serial.print(digitalRead(KEY2_PIN));
    Serial.print(" K3=");
    Serial.print(digitalRead(KEY3_PIN));
    Serial.print(" K4=");
    Serial.println(digitalRead(KEY4_PIN));
    
    // 背光状态
    Serial.print("背光引脚(GPIO");
    Serial.print(TFT_BL);
    Serial.print(")状态: ");
    Serial.println(digitalRead(TFT_BL) ? "HIGH" : "LOW");
    
    Serial.println("================\n");
  }
} 