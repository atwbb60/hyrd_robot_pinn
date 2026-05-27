#include <SCServo.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <esp_bt.h>
#include "config.h"

SMS_STS sts;
WiFiUDP udp;
WiFiUDP reportUdp;

uint8_t ids[10] = {1, 2};

// 0=position, 1=speed, 2=pwm, 3=step
uint8_t currentMode[10] = {0};

unsigned long lastReport = 0;
bool emergencyMode = false;

int getIdIndex(uint8_t id) {
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (ids[i] == id) return i;
  }
  return -1;
}

uint8_t getServoMode(uint8_t id) {
  int idx = getIdIndex(id);
  if (idx < 0) return 0xFF;
  return currentMode[idx];
}

bool isValidId(int id) {
  return getIdIndex(id) >= 0;
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // 关闭蓝牙，避免干扰 WiFi
  btStop();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
  
  Serial.println("\n========================");
  
  // 初始化舵机串口
  Serial1.begin(SERVO_BAUDRATE, SERIAL_8N1, SERVO_RX_PIN, SERVO_TX_PIN);
  sts.pSerial = &Serial1;
  delay(1000);
  
  // 读取所有舵机的当前模式，缓存到内存
  Serial.println("Reading servo modes...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    currentMode[i] = sts.readByte(ids[i], 33);
    Serial.printf("  ID%d mode=%d\n", ids[i], currentMode[i]);
  }
  
  // 防止重启后舵机突然跳到旧的目标位置
  Serial.println("Syncing target to current positions...");
  for (int i = 0; i < NUM_SERVOS; i++) {
    if (currentMode[i] == 0) {  // 仅位置模式需要同步
      int curPos = sts.readWord(ids[i], 56);
      sts.WritePosEx(ids[i], curPos, 1000, 50);
      Serial.printf("  ID%d synced to %d\n", ids[i], curPos);
    }
  }
  
  // 连接 WiFi
  setupWiFi();
  
  // 启动后主动注册
  delay(500);
  announceMyself();
  
  Serial.println("Ready");
}

void loop() {
  // 1. 处理指令
  handleUDP();
  
  // 2. 定期上报状态
  if (millis() - lastReport > REPORT_INTERVAL) {
    sendStateReport();
    lastReport = millis();
  }
  
}
