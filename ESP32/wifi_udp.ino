void setupWiFi() {
  WiFi.mode(WIFI_STA);
  //WiFi.setSleep(false);  // 关闭省电
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting WiFi");
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 60) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi failed, restarting...");
    ESP.restart();
  }
  
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway (Pi): ");
  Serial.println(WiFi.gatewayIP());
  
  udp.begin(CMD_PORT);
  Serial.printf("CMD port: %d, Report port: %d\n", CMD_PORT, REPORT_PORT);
}

// 主动向树莓派注册（自动发现）
void announceMyself() {
  IPAddress raspIP(RASP_IP_0, RASP_IP_1, RASP_IP_2, RASP_IP_3);
  
  JsonDocument doc;
  doc["type"] = "hello";
  doc["node"] = NODE_ID;
  doc["num_servos"] = NUM_SERVOS;
  
  JsonArray servoIds = doc["servo_ids"].to<JsonArray>();
  for (int i = 0; i < NUM_SERVOS; i++) {
    servoIds.add(ids[i]);
  }
  
  char buf[256];
  size_t len = serializeJson(doc, buf);
  
  reportUdp.beginPacket(raspIP, REPORT_PORT);
  reportUdp.write((uint8_t*)buf, len);
  reportUdp.endPacket();
  
  Serial.printf("Announced as node %d to %d.%d.%d.%d:%d\n",
                NODE_ID, RASP_IP_0, RASP_IP_1, RASP_IP_2, RASP_IP_3, REPORT_PORT);
}


// 通用 JSON 发送
void sendJson(JsonDocument& doc, IPAddress ip, int port) {
  char buf[1024];
  size_t len = serializeJson(doc, buf);
  
  udp.beginPacket(ip, port);
  udp.write((uint8_t*)buf, len);
  udp.endPacket();
}

// 精简响应：成功
void respondOK(IPAddress ip, int port) {
  JsonDocument doc;
  doc["ok"] = true;
  sendJson(doc, ip, port);
}

// 精简响应：失败
void respondError(IPAddress ip, int port, const char* err) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["err"] = err;
  sendJson(doc, ip, port);
}
