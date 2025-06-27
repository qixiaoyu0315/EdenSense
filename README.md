# EdenSense 温度监控系统

## 项目简介

EdenSense 是基于 ESP32-C3 的多通道温度监控系统，集成了 WiFi、MQTT 云通信、OLED/TFT 显示、低功耗管理等功能，适用于环境监测、远程数据采集等场景。

---

## 主要功能

- **多通道温度采集**：支持多路 DS18B20 传感器，实时采集温度数据。
- **本地显示**：TFT 屏幕显示温度概览、详情、历史曲线等多种页面。
- **WiFi 连接**：自动连接指定 WiFi，支持断线重连，信号强度图标显示。
- **MQTT 云通信**：支持 SSL 安全连接，远程命令触发数据上报，JSON 格式数据推送。
- **低功耗优化**：CPU 降频、WiFi 功率可调、屏幕亮度可调、禁用 WiFi 睡眠。
- **电源管理**：适配多种 USB 电源，串口输出电源与系统状态，便于故障排查。

---

## 快速上手

### 1. 硬件准备

- ESP32-C3 开发板
- DS18B20 温度传感器（支持多路）
- 1.3寸/1.54寸 TFT 屏幕
- 5V/2A 及以上 USB 电源适配器
- 优质 USB 线

### 2. 软件环境

- [PlatformIO](https://platformio.org/) 开发环境
- 推荐使用 VSCode + PlatformIO 插件

---

## 配置说明

### 1. WiFi 配置

在 `src/main.cpp` 文件中，找到如下代码并修改为你的实际 WiFi 名称和密码：

```cpp
#define WIFI_SSID "你的WiFi名称"
#define WIFI_PASSWORD "你的WiFi密码"
```

**可选：WiFi 功率设置**

```cpp
#define WIFI_POWER_DBM 8 // 建议8-12dBm，信号弱可适当提高
```

### 2. MQTT 配置

在 `src/main.cpp` 文件中，找到如下代码并根据你的 MQTT 服务器信息修改：

```cpp
#define MQTT_SERVER "你的MQTT服务器地址"
#define MQTT_PORT 8883 // SSL端口，普通端口一般为1883
#define MQTT_CLIENT_ID "esp32c3print"
#define MQTT_USERNAME "你的用户名"
#define MQTT_PASSWORD "你的密码"
#define MQTT_SUBSCRIBE_TOPIC "testtopic"
#define MQTT_PUBLISH_TOPIC "testtopic"
#define MQTT_USE_SSL true // 使用SSL连接
```

---

## 功能特性

### WiFi 连接

- 启动自动连接 WiFi，断线自动重连，最多重试5次
- 屏幕显示连接进度、IP、信号强度
- 信号强度图标：绿色（强）、黄色（中）、红色（弱）

### MQTT 云通信

- 自动连接 MQTT 服务器，断线自动重连
- 支持 SSL 安全连接（EMQX 云等）
- 订阅命令主题，收到 `refresh` 命令后推送温度数据
- 数据采用标准 JSON 格式，包含所有通道当前温度与历史温度数组

#### 远程命令与数据格式

- **获取温度数据**：向订阅主题发送 `refresh` 消息
- **数据格式示例**：

```json
{
  "T1-28FF12345678": {
    "c_t": "28.5",
    "l_t": [22.1, 21.9, ...] // 最多120个历史点
  },
  ...
}
```

- `c_t`：当前温度（字符串，保留1位小数）
- `l_t`：历史温度数组
- `last_time`：最后一次更新时间（字符串）

#### mosquitto 命令行示

---

## 电源管理与功耗优化

- **推荐电源**：5V/2A 及以上适配器，优质USB线
- **功耗优化**：
  - CPU频率降至80MHz
  - WiFi发射功率8dBm
  - 屏幕亮度可调（默认180/255）
  - 禁用WiFi睡眠
  - MQTT保活60秒
- **串口输出电源与系统状态**，便于排查供电异常

---

## 故障排查与建议

1. **频繁断电/重启**：检查电源适配器和USB线，避免使用电脑USB口
2. **WiFi连接失败**：确认SSID/密码正确，信号强度足够
3. **MQTT连接失败**：检查服务器地址、端口、用户名密码，确认网络畅通
4. **功耗过高/发热**：适当降低屏幕亮度、WiFi功率
5. **串口监控**：可通过串口查看系统运行日志和电源状态

---

## 目录结构说明

```
EdenSense/
├── include/           # 头文件目录
├── lib/               # 项目私有库目录
├── src/
│   └── main.cpp       # 主程序代码
├── test/              # 测试代码
├── platformio.ini     # PlatformIO 配置文件
├── README.md          # 项目说明文档（本文件）
├── WiFi配置说明.md
├── MQTT配置说明.md
├── 电源管理说明.md
```

---

## 依赖库

- Arduino
- OneWire
- DallasTemperature
- TFT_eSPI
- OneButton
- PubSubClient
- ArduinoJson
- WiFiClientSecure

PlatformIO 会自动管理依赖库，无需手动安装。

---

## 参考与致谢

- [PlatformIO 官方文档](https://docs.platformio.org/)
- [EMQX 云 MQTT 服务](https://www.emqx.com/zh/cloud)
- [ESP32 官方文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/)

---

如有问题欢迎提交 issue 或联系作者。

---

如需进一步定制或有其他需求，欢迎交流！ 