void emergencyStop() {
  emergencyMode = true;
  
  for (int i = 0; i < NUM_SERVOS; i++) {
    sts.WriteSpe(ids[i], 0);
  }
  
  Serial.println("!!! EMERGENCY STOP !!!");
}

// 主动状态上报（紧凑格式）
// 格式: {"node":1,"t":12345,"ids":[1,2],"servos":[[pos,spd,load,temp,v10],...]}
void sendStateReport() {
  if (emergencyMode) return;
  
  JsonDocument doc;
  doc["node"] = NODE_ID;
  doc["t"] = millis();
  
  JsonArray idsArr = doc["ids"].to<JsonArray>();
  JsonArray servosArr = doc["servos"].to<JsonArray>();
  
  for (int i = 0; i < NUM_SERVOS; i++) {
    idsArr.add(ids[i]);
    JsonArray s = servosArr.add<JsonArray>();
    fillServoState(s, ids[i]);
  }
  
  IPAddress raspIP(RASP_IP_0, RASP_IP_1, RASP_IP_2, RASP_IP_3);
  
  char buf[512];
  size_t len = serializeJson(doc, buf);
  
  reportUdp.beginPacket(raspIP, REPORT_PORT);
  reportUdp.write((uint8_t*)buf, len);
  reportUdp.endPacket();
}
