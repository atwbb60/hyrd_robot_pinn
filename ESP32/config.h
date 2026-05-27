#ifndef CONFIG_H
#define CONFIG_H

#define NODE_ID 1               // 节点ID（每个ESP32唯一: 1, 2, 3...）
#define NUM_SERVOS 2            // 本节点的舵机数量

#define WIFI_SSID "smartpi5"
#define WIFI_PASSWORD "smartpi5"

#define CMD_PORT 8888           // 接收指令端口
#define REPORT_PORT 9999        // 树莓派接收状态的端口
#define REPORT_INTERVAL 100     // 状态上报间隔(ms)

#define RASP_IP_0 10
#define RASP_IP_1 42
#define RASP_IP_2 0
#define RASP_IP_3 1

#define SERVO_RX_PIN 18
#define SERVO_TX_PIN 17
#define SERVO_BAUDRATE 115200

#define HEARTBEAT_TIMEOUT 5000  // 心跳超时(ms)

#endif
