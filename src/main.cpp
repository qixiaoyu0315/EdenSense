#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// GPIO where the DS18B20 is connected to
const int oneWireBus = 4;     // 使用GPIO4作为数据线

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(oneWireBus);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);

void setup() {
  // Start the Serial Monitor
  Serial.begin(115200);
  // Start the DS18B20 sensor
  sensors.begin();
}

void loop() {
  Serial.println("正在读取温度...");
  sensors.requestTemperatures(); 
  
  // 获取连接的传感器数量
  int deviceCount = sensors.getDeviceCount();
  Serial.printf("找到 %d 个温度传感器\n", deviceCount);
  
  // 读取每个传感器的温度
  for(int i = 0; i < deviceCount; i++) {
    float tempC = sensors.getTempCByIndex(i);
    if(tempC != DEVICE_DISCONNECTED_C) {
      Serial.printf("传感器 %d 温度: %.2f ℃\n", i, tempC);
    } else {
      Serial.printf("传感器 %d 读取错误\n", i);
    }
  }
  
  delay(5000); // 等待5秒后再次读取
}