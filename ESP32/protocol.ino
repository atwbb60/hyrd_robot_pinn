void handleUDP() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;
  
  char buf[1024];
  int len = udp.read(buf, 1023);
  buf[len] = 0;
  
  IPAddress senderIP = udp.remoteIP();
  int senderPort = udp.remotePort();
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf);
  
  if (error) {
    respondError(senderIP, senderPort, "invalid_json");
    return;
  }
  
  String cmd = doc["cmd"];
  
  // 4 种模式
  if (cmd == "mode_position") {
    handleModePosition(doc, senderIP, senderPort);
  }
  else if (cmd == "mode_speed") {
    handleModeSpeed(doc, senderIP, senderPort);
  }
  else if (cmd == "mode_step") {
    handleModeStep(doc, senderIP, senderPort);
  }
  else if (cmd == "mode_pwm") {
    handleModePWM(doc, senderIP, senderPort);
  }
  // 通用控制
  else if (cmd == "stop") {
    handleStop(doc, senderIP, senderPort);
  }
  // 状态读取
  else if (cmd == "read") {
    handleRead(doc, senderIP, senderPort);
  }
  // 寄存器
  else if (cmd == "reg_read") {
    handleRegRead(doc, senderIP, senderPort);
  }
  else if (cmd == "reg_write") {
    handleRegWrite(doc, senderIP, senderPort);
  }
  // 模式查询
  else if (cmd == "get_modes") {
    handleGetModes(senderIP, senderPort);
  }
  // 系统
  else if (cmd == "ping") {
    respondOK(senderIP, senderPort);
  }
  else if (cmd == "info") {
    handleInfo(senderIP, senderPort);
  }
  else if (cmd == "latency") {
    handleLatency(doc, senderIP, senderPort);
  }
  else if (cmd == "emergency") {
    emergencyStop();
    respondOK(senderIP, senderPort);
  }
  else if (cmd == "reset") {
    respondOK(senderIP, senderPort);
    delay(100);
    ESP.restart();
  }
  else {
    respondError(senderIP, senderPort, "unknown_cmd");
  }
}

// 节点信息
void handleInfo(IPAddress ip, int port) {
  JsonDocument doc;
  doc["ok"] = true;
  doc["node"] = NODE_ID;
  doc["num_servos"] = NUM_SERVOS;
  doc["uptime"] = millis() / 1000;
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  
  JsonArray servoIds = doc["servo_ids"].to<JsonArray>();
  for (int i = 0; i < NUM_SERVOS; i++) {
    servoIds.add(ids[i]);
  }
  
  sendJson(doc, ip, port);
}

// 查询当前模式
void handleGetModes(IPAddress ip, int port) {
  JsonDocument doc;
  doc["ok"] = true;
  
  JsonArray modes = doc["modes"].to<JsonArray>();
  for (int i = 0; i < NUM_SERVOS; i++) {
    modes.add(currentMode[i]);
  }
  
  JsonArray servoIds = doc["ids"].to<JsonArray>();
  for (int i = 0; i < NUM_SERVOS; i++) {
    servoIds.add(ids[i]);
  }
  
  sendJson(doc, ip, port);
}

// 延迟测试
void handleLatency(JsonDocument& doc, IPAddress ip, int port) {
  unsigned long t_recv = millis();
  delayMicroseconds(100);
  unsigned long t_send = millis();
  
  JsonDocument response;
  response["ok"] = true;
  response["esp_recv"] = t_recv;
  response["esp_send"] = t_send;
  sendJson(response, ip, port);
}