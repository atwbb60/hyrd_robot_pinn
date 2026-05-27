import socket
import json
import threading
import time
from config import CMD_PORT, TIMEOUT, RETRIES, NODES_CONFIG, MODE_MAP
import robot_monitor as monitor

_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
_sock.settimeout(TIMEOUT)
_sock_lock = threading.Lock()


def send_cmd(node_id, cmd_dict):

    ip = monitor.get_node_ip(node_id)
    if ip is None:
        print(f"[API] Node {node_id} not discovered yet")
        return None
    
    data = json.dumps(cmd_dict).encode()
    
    for _ in range(RETRIES):
        with _sock_lock:
            _sock.sendto(data, (ip, CMD_PORT))
            try:
                response, _ = _sock.recvfrom(2048)
                return json.loads(response.decode())
            except socket.timeout:
                continue
    return None

def mode_position(node_id, servos):
    """位置模式
    servos: list of dict, 每个含 id, pos, speed(可选), acc(可选)
    """
    return send_cmd(node_id, {
        "cmd": "mode_position",
        "servos": servos
    })


def mode_speed(node_id, servos):
    """速度模式
    servos: [{"id": 1, "speed": 500}, ...]
    speed 范围: -4095 到 4095
    """
    return send_cmd(node_id, {
        "cmd": "mode_speed",
        "servos": servos
    })


def mode_step(node_id, servos):
    """步进模式
    servos: [{"id": 1, "steps": 1000, "speed": 500, "acc": 50}, ...]
    steps 正负代表方向
    """
    return send_cmd(node_id, {
        "cmd": "mode_step",
        "servos": servos
    })


def mode_pwm(node_id, servos):
    """PWM 模式
    servos: [{"id": 1, "pwm": 500}, ...]
    pwm 范围: 0 到 1000; +1024
    """
    return send_cmd(node_id, {
        "cmd": "mode_pwm",
        "servos": servos
    })


def stop(node_id, ids=None):
    """停止
    stop(1)                 所有舵机
    stop(1, [1, 2])         指定舵机
    """
    cmd = {"cmd": "stop"}
    if ids is not None:
        cmd["ids"] = list(ids) if not isinstance(ids, list) else ids
    return send_cmd(node_id, cmd)


def read(node_id, ids=None):
    """读取舵机基础状态
    read(1)               所有舵机
    read(1, [1])          指定舵机
    """
    cmd = {"cmd": "read"}
    if ids is not None:
        cmd["ids"] = list(ids) if not isinstance(ids, list) else ids
    return send_cmd(node_id, cmd)


def read_dict(node_id, ids=None):
    """读取舵机状态（自动解析为字典格式，方便使用）
    """
    result = read(node_id, ids)
    if not result or not result.get("ok"):
        return None
    
    ids_list = result.get("ids", [])
    servos = result.get("servos", [])
    
    return [
        {
            "id": ids_list[i],
            "pos": s[0],
            "spd": s[1],
            "load": s[2],
            "temp": s[3],
            "voltage": s[4] / 10.0
        }
        for i, s in enumerate(servos)
    ]


def reg_read(node_id, servos):
    # 按舵机分组读取寄存器
    serialized = []
    for req in servos:
        serialized.append({
            "id": req["id"],
            "regs": [list(r) for r in req["regs"]]
        })
    
    return send_cmd(node_id, {
        "cmd": "reg_read",
        "servos": serialized
    })


def reg_write(node_id, servos):
    # 按舵机分组写入寄存器
    serialized = []
    for req in servos:
        item = {
            "id": req["id"],
            "regs": [list(r) for r in req["regs"]]
        }
        if "area" in req:
            item["area"] = req["area"]
        serialized.append(item)
    
    return send_cmd(node_id, {
        "cmd": "reg_write",
        "servos": serialized
    })


def get_modes(node_id):
    """查询节点所有舵机当前模式
    模式编号: 0=position, 1=speed, 2=pwm, 3=step
    """
    return send_cmd(node_id, {"cmd": "get_modes"})


def ping(node_id):
    """心跳测试
    返回: {"ok": true} 在线; None 离线
    """
    return send_cmd(node_id, {"cmd": "ping"})


def info(node_id):
    # 节点详细信息
    return send_cmd(node_id, {"cmd": "info"})


def emergency_stop(node_id):
    """紧急停止（节点进入安全模式，需 reset 恢复）"""
    return send_cmd(node_id, {"cmd": "emergency"})


def reset(node_id):
    """重启 ESP32"""
    return send_cmd(node_id, {"cmd": "reset"})


def latency_test(node_id):

    ip = monitor.get_node_ip(node_id)
    if ip is None:
        return None
    
    cmd = json.dumps({"cmd": "latency"}).encode()
    
    t_pi_send = time.perf_counter()
    
    with _sock_lock:
        _sock.sendto(cmd, (ip, CMD_PORT))
        try:
            response, _ = _sock.recvfrom(2048)
            t_pi_recv = time.perf_counter()
        except socket.timeout:
            return None
    
    data = json.loads(response.decode())
    if not data.get("ok"):
        return None
    
    rtt_ms = (t_pi_recv - t_pi_send) * 1000
    esp_proc_ms = data["esp_send"] - data["esp_recv"]
    
    return {
        "rtt": rtt_ms,
        "esp_proc": esp_proc_ms,
        "network": rtt_ms - esp_proc_ms,
        "pi_send": t_pi_send,
        "pi_recv": t_pi_recv,
        "esp_recv": data["esp_recv"],
        "esp_send": data["esp_send"],
    }


# 智能初始化（保护 EEPROM）
def initialize_node(node_id):

    if node_id not in NODES_CONFIG:
        print(f"Node {node_id} not in config")
        return False
    
    if not ping(node_id):
        print(f"Node {node_id} offline")
        return False
    
    config = NODES_CONFIG[node_id]
    print(f"Initializing node {node_id} ({config.get('name','')})...")
    
    # 1. 读取当前 EEPROM 配置
    read_requests = []
    for servo in config["servos"]:
        sid = servo["id"]
        read_requests.append({
            "id": sid,
            "regs": [(33, 1), (9, 2), (11, 2)]   # 模式, 最小, 最大
        })
    
    current = reg_read(node_id, read_requests)
    if not current or not current.get("ok"):
        print(f"  Failed to read current config")
        return False
    
    # 2. 整理当前值
    current_values = {}
    for r in current["results"]:
        sid = r["id"]
        if "values" in r:
            current_values[(sid, 33)] = r["values"][0]
            current_values[(sid, 9)] = r["values"][1]
            current_values[(sid, 11)] = r["values"][2]
    
    # 3. 找出不一致的项
    writes_by_servo = {}
    for servo in config["servos"]:
        sid = servo["id"]
        target_mode = MODE_MAP.get(servo["mode"], 0)
        
        regs_to_write = []
        
        if current_values.get((sid, 33)) != target_mode:
            print(f"  Servo {sid}: mode {current_values.get((sid, 33))} -> {target_mode}")
            regs_to_write.append((33, target_mode, 1))
        
        if current_values.get((sid, 9)) != servo["min_pos"]:
            print(f"  Servo {sid}: min_pos -> {servo['min_pos']}")
            regs_to_write.append((9, servo["min_pos"], 2))
        
        if current_values.get((sid, 11)) != servo["max_pos"]:
            print(f"  Servo {sid}: max_pos -> {servo['max_pos']}")
            regs_to_write.append((11, servo["max_pos"], 2))
        
        if regs_to_write:
            writes_by_servo[sid] = regs_to_write
    
    # 4. 写入
    if writes_by_servo:
        write_requests = [
            {"id": sid, "area": "eeprom", "regs": regs}
            for sid, regs in writes_by_servo.items()
        ]
        result = reg_write(node_id, write_requests)
        if result and result.get("ok"):
            print(f"  Updated {sum(len(r) for r in writes_by_servo.values())} registers")
            return True
        else:
            print(f"  Update failed")
            return False
    else:
        print(f"  No changes needed")
        return True


def initialize_all():
    """初始化所有 config.py 中的节点"""
    for node_id in NODES_CONFIG:
        initialize_node(node_id)
