void handleRegRead(JsonDocument& doc, IPAddress ip, int port) {
  if (!doc.containsKey("servos")) {
    respondError(ip, port, "missing_servos");
    return;
  }
  
  JsonDocument response;
  response["ok"] = true;
  JsonArray results = response["results"].to<JsonArray>();
  
  JsonArray requests = doc["servos"];
  
  for (JsonObject req : requests) {
    int id = req["id"];
    
    JsonObject result = results.add<JsonObject>();
    result["id"] = id;
    
    if (!isValidId(id)) {
      result["err"] = "invalid_id";
      continue;
    }
    
    JsonArray values = result["values"].to<JsonArray>();
    
    // regs 是数组的数组: [[addr, len], [addr, len], ...]
    for (JsonArray reg : req["regs"].as<JsonArray>()) {
      int addr = reg[0];
      int len = reg.size() > 1 ? (int)reg[1] : 1;
      
      int value;
      if (len == 1) {
        value = sts.readByte(id, addr);
      } else {
        value = sts.readWord(id, addr);
      }
      values.add(value);
    }
  }
  
  sendJson(response, ip, port);
}

// {
//   "cmd":"reg_write",
//   "servos":[
//     {
//       "id":1,
//       "regs":[[42,2000,2],[41,50,1]]   // [addr, value, len]
//     },
//     {
//       "id":2,
//       "area":"eeprom",                  // 可选，默认 ram
//       "regs":[[33,0,1]]
//     }
//   ]
// }
// 
// 响应: {"ok":true}
void handleRegWrite(JsonDocument& doc, IPAddress ip, int port) {
  if (!doc.containsKey("servos")) {
    respondError(ip, port, "missing_servos");
    return;
  }
  
  JsonArray requests = doc["servos"];
  
  for (JsonObject req : requests) {
    int id = req["id"];
    if (!isValidId(id)) continue;
    
    String area = req["area"] | "ram";
    
    if (area == "eeprom") {
      sts.unLockEprom(id);
    }
    
    // regs 格式: [[addr, value, len], ...]
    for (JsonArray reg : req["regs"].as<JsonArray>()) {
      int addr = reg[0];
      int value = reg[1];
      int len = reg.size() > 2 ? (int)reg[2] : 1;
      
      if (len == 1) {
        sts.writeByte(id, addr, value);
      } else {
        sts.writeWord(id, addr, value);
      }
      
      // 写入模式寄存器后同步缓存
      if (addr == 33) {
        int idx = getIdIndex(id);
        if (idx >= 0) {
          currentMode[idx] = value;
          Serial.printf("Cache updated: ID%d mode=%d\n", id, value);
        }
      }
    }
    
    if (area == "eeprom") {
      sts.LockEprom(id);
    }
  }
  
  respondOK(ip, port);
}

// 读取基础状态（紧凑数组格式）
// 
// 请求:
//   {"cmd":"read"}              所有舵机
//   {"cmd":"read","ids":[1,2]}  指定舵机
// 
// 响应:
// {
//   "ok":true,
//   "ids":[1,2],
//   "servos":[
//     [2048,0,0,40,74],   // [pos, spd, load, temp, voltage*10]
//     [1500,100,50,41,74]
//   ]
// }
void handleRead(JsonDocument& doc, IPAddress ip, int port) {
  JsonDocument response;
  response["ok"] = true;
  
  JsonArray idsArr = response["ids"].to<JsonArray>();
  JsonArray servosArr = response["servos"].to<JsonArray>();
  
  if (doc.containsKey("ids")) {
    JsonArray reqIds = doc["ids"];
    for (int id : reqIds) {
      if (!isValidId(id)) continue;
      idsArr.add(id);
      JsonArray s = servosArr.add<JsonArray>();
      fillServoState(s, id);
    }
  } else {
    for (int i = 0; i < NUM_SERVOS; i++) {
      idsArr.add(ids[i]);
      JsonArray s = servosArr.add<JsonArray>();
      fillServoState(s, ids[i]);
    }
  }
  
  sendJson(response, ip, port);
}

// 填充舵机状态（紧凑数组）
void fillServoState(JsonArray& arr, int id) {
  arr.add(sts.readWord(id, 56));          // pos
  arr.add(sts.ReadSpeed(id));              // spd
  arr.add(sts.readWord(id, 60));           // load
  arr.add(sts.readByte(id, 63));           // temp
  arr.add(sts.readByte(id, 62));           // voltage * 10
}
