// 模式 1：位置模式
// 用法: {"cmd":"mode_position","servos":[{"id":1,"pos":2048,"speed":1000,"acc":50},...]}
void handleModePosition(JsonDocument& doc, IPAddress ip, int port) {
  if (emergencyMode) {
    respondError(ip, port, "emergency_mode");
    return;
  }
  
  if (!doc.containsKey("servos")) {
    respondError(ip, port, "missing_servos");
    return;
  }
  
  JsonArray servos = doc["servos"];
  uint8_t syncIds[10];
  int16_t syncPos[10];
  uint16_t syncSpd[10];
  uint8_t syncAcc[10];
  int count = 0;
  
  // 先检查所有舵机
  for (JsonObject s : servos) {
    int id = s["id"];
    if (!isValidId(id)) {
      respondError(ip, port, "invalid_id");
      return;
    }
    if (getServoMode(id) != 0) {
      respondError(ip, port, "wrong_mode");
      return;
    }
  }
  
  // 填充参数
  for (JsonObject s : servos) {
    if (count >= NUM_SERVOS) break;
    syncIds[count] = s["id"];
    syncPos[count] = s["pos"];
    syncSpd[count] = s["speed"] | 1000;
    syncAcc[count] = s["acc"] | 50;
    count++;
  }
  
  sts.SyncWritePosEx(syncIds, count, syncPos, syncSpd, syncAcc);
  respondOK(ip, port);
}

// 模式 2：速度模式
// 用法: {"cmd":"mode_speed","servos":[{"id":1,"speed":500},...]}
// 速度范围: -4095 到 4095
void handleModeSpeed(JsonDocument& doc, IPAddress ip, int port) {
  if (emergencyMode) {
    respondError(ip, port, "emergency_mode");
    return;
  }
  
  if (!doc.containsKey("servos")) {
    respondError(ip, port, "missing_servos");
    return;
  }
  
  JsonArray servos = doc["servos"];
  
  // 检查
  for (JsonObject s : servos) {
    int id = s["id"];
    if (!isValidId(id)) {
      respondError(ip, port, "invalid_id");
      return;
    }
    if (getServoMode(id) != 1) {
      respondError(ip, port, "wrong_mode");
      return;
    }
  }
  
  // 执行
  for (JsonObject s : servos) {
    sts.WriteSpe(s["id"], s["speed"]);
  }
  respondOK(ip, port);
}

// 模式 3：步进模式
// 用法: {"cmd":"mode_step","servos":[{"id":1,"steps":1000,"speed":500,"acc":50},...]}
// steps 正负代表方向
void handleModeStep(JsonDocument& doc, IPAddress ip, int port) {
  if (emergencyMode) {
    respondError(ip, port, "emergency_mode");
    return;
  }
  
  if (!doc.containsKey("servos")) {
    respondError(ip, port, "missing_servos");
    return;
  }
  
  JsonArray servos = doc["servos"];
  
  for (JsonObject s : servos) {
    int id = s["id"];
    if (!isValidId(id)) {
      respondError(ip, port, "invalid_id");
      return;
    }
    if (getServoMode(id) != 3) {
      respondError(ip, port, "wrong_mode");
      return;
    }
  }
  
  for (JsonObject s : servos) {
    int id = s["id"];
    int steps = s["steps"];
    int speed = s["speed"] | 1000;
    int acc = s["acc"] | 50;
    sts.WritePosEx(id, steps, speed, acc);
  }
  respondOK(ip, port);
}


// 模式 4：PWM 模式
// 用法: {"cmd":"mode_pwm","servos":[{"id":1,"pwm":500},...]}
// PWM 范围: 0 到 1000; +1024 转向
void handleModePWM(JsonDocument& doc, IPAddress ip, int port) {
  if (emergencyMode) {
    respondError(ip, port, "emergency_mode");
    return;
  }
  
  if (!doc.containsKey("servos")) {
    respondError(ip, port, "missing_servos");
    return;
  }
  
  JsonArray servos = doc["servos"];
  
  for (JsonObject s : servos) {
    int id = s["id"];
    if (!isValidId(id)) {
      respondError(ip, port, "invalid_id");
      return;
    }
    if (getServoMode(id) != 2) {
      respondError(ip, port, "wrong_mode");
      return;
    }
  }
  
  for (JsonObject s : servos) {
    sts.writeWord(s["id"], 44, s["pwm"]);  // 寄存器44 = PWM值
  }
  respondOK(ip, port);
}


//   {"cmd":"stop"}                所有舵机
//   {"cmd":"stop","ids":[1,2]}    指定舵机
void handleStop(JsonDocument& doc, IPAddress ip, int port) {
  // 根据每个舵机的当前模式决定停止方式
  
  auto stopOne = [](int id) {
    int idx = getIdIndex(id);
    if (idx < 0) return;
    
    uint8_t mode = currentMode[idx];
    
    if (mode == 2) {
      // PWM 模式：写 PWM=0
      sts.writeWord(id, 44, 0);
    } else {
      // 其他模式：写速度=0
      sts.WriteSpe(id, 0);
    }
  };
  
  if (doc.containsKey("ids")) {
    JsonArray idsArray = doc["ids"];
    for (int id : idsArray) {
      stopOne(id);
    }
    Serial.println("[Stop] Selected");
  } else {
    for (int i = 0; i < NUM_SERVOS; i++) {
      stopOne(ids[i]);
    }
    Serial.println("[Stop] All");
  }
  respondOK(ip, port);
}
