import socket
import json
import threading
import time
from config import REPORT_PORT

_node_states = {}        
_discovered_ips = {}     
_node_info = {}          

_state_lock = threading.Lock()
_listener_thread = None
_running = False


def _listener():
    
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("", REPORT_PORT))
    sock.settimeout(1.0)
    
    print(f"[Monitor] Listening on port {REPORT_PORT}")
    
    while _running:
        try:
            data, addr = sock.recvfrom(1024)
            sender_ip = addr[0]
            
            msg = json.loads(data.decode())
            node_id = msg.get("node")
            
            if node_id is None:
                continue
            
            with _state_lock:
                # === 自动发现：记录 IP ===
                if node_id not in _discovered_ips:
                    print(f"[Discover] Node {node_id} at {sender_ip}")
                _discovered_ips[node_id] = sender_ip
                
                # === Hello 消息：记录节点信息 ===
                if msg.get("type") == "hello":
                    _node_info[node_id] = {
                        "firmware": msg.get("firmware"),
                        "num_servos": msg.get("num_servos"),
                        "servo_ids": msg.get("servo_ids", []),
                    }
                    print(f"[Hello] Node {node_id} fw={msg.get('firmware')} "
                          f"servos={msg.get('servo_ids')}")
                
                # === 状态上报 ===
                else:
                    _node_states[node_id] = {
                        "data": msg,
                        "received_at": time.time(),
                    }
        except socket.timeout:
            continue
        except Exception as e:
            print(f"[Monitor] Error: {e}")
    
    sock.close()


def start():
    """启动监控"""
    global _listener_thread, _running
    if _listener_thread and _listener_thread.is_alive():
        return
    _running = True
    _listener_thread = threading.Thread(target=_listener, daemon=True)
    _listener_thread.start()


def stop():
    global _running
    _running = False


def get_node_ip(node_id):
    """获取节点 IP（自动发现的）"""
    with _state_lock:
        return _discovered_ips.get(node_id)


def get_discovered():
    """获取所有已发现的节点 {node_id: ip}"""
    with _state_lock:
        return dict(_discovered_ips)


def get_node_info(node_id):
    """获取节点的 hello 消息内容"""
    with _state_lock:
        return _node_info.get(node_id, {})


def wait_for_nodes(node_ids, timeout=10.0):
    """等待指定节点全部上线
    
    返回: True 全部上线, False 超时
    """
    end_time = time.time() + timeout
    while time.time() < end_time:
        with _state_lock:
            if all(nid in _discovered_ips for nid in node_ids):
                return True
        time.sleep(0.2)
    return False


def get_state(node_id):
    """获取节点最新状态字典"""
    with _state_lock:
        info = _node_states.get(node_id)
        if info:
            return info["data"]
    return None


def get_all_states():
    """所有节点最新状态"""
    with _state_lock:
        return {nid: info["data"] for nid, info in _node_states.items()}


def get_servo_pos(node_id, servo_id):
    """获取某舵机当前位置
    
    state 格式: {"ids":[1,2],"servos":[[pos,spd,load,temp,v10],...]}
    """
    state = get_state(node_id)
    if not state:
        return None
    
    ids_list = state.get("ids", [])
    servos = state.get("servos", [])
    
    try:
        idx = ids_list.index(servo_id)
        return servos[idx][0]   # 第0个字段是 pos
    except (ValueError, IndexError):
        return None


def get_servo_data(node_id, servo_id):
    """获取某舵机的完整数据
    
    返回: dict {pos, spd, load, temp, voltage}
    """
    state = get_state(node_id)
    if not state:
        return None
    
    ids_list = state.get("ids", [])
    servos = state.get("servos", [])
    
    try:
        idx = ids_list.index(servo_id)
        data = servos[idx]
        return {
            "pos": data[0],
            "spd": data[1],
            "load": data[2],
            "temp": data[3],
            "voltage": data[4] / 10.0   # 转回 V
        }
    except (ValueError, IndexError):
        return None


def is_online(node_id, timeout=2.0):
    """检查节点是否在线"""
    with _state_lock:
        info = _node_states.get(node_id)
        if not info:
            return False
        return (time.time() - info["received_at"]) < timeout


def get_online_nodes():
    """获取所有在线节点 ID"""
    with _state_lock:
        return [nid for nid in list(_node_states.keys())
                if (time.time() - _node_states[nid]["received_at"]) < 2.0]
