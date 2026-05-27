import time
import robot_api as api
import robot_monitor as monitor
from config import NODES_CONFIG


def wait_and_discover():
    
    print("=== 启动监控 ===")
    monitor.start()
    
    expected_nodes = list(NODES_CONFIG.keys())
    print(f"等待节点: {expected_nodes}")
    
    if monitor.wait_for_nodes(expected_nodes, timeout=10):
        print("✅ 所有节点已发现")
    else:
        discovered = monitor.get_discovered()
        missing = [n for n in expected_nodes if n not in discovered]
        print(f"⚠️ 缺失节点: {missing}")
    
    print("\n=== 已发现的节点 ===")
    for nid, ip in monitor.get_discovered().items():
        info = monitor.get_node_info(nid)
        print(f"  节点 {nid}: {ip}  fw={info.get('firmware')}  "
              f"servos={info.get('servo_ids')}")


def switch_mode(node_id, ids, mode_value):

    api.reg_write(node_id, [
        {"id": sid, "area": "eeprom", "regs": [(33, mode_value, 1)]}
        for sid in ids
    ])
    time.sleep(0.5)


def monitor_during(duration, interval=0.5):

    steps = int(duration / interval)
    for i in range(steps):
        pos1 = monitor.get_servo_pos(1, 1)
        pos2 = monitor.get_servo_pos(1, 2)
        print(f"  [{i*interval:.1f}s] S1={pos1}, S2={pos2}")
        time.sleep(interval)


def demo_modes():
    print("\n=== Testʾ ===")
    
    print("\n--- [1/4] Position ---")
    switch_mode(1, [1, 2], 0)
    api.mode_position(1, [
        {"id": 1, "pos": -1000, "speed": 1000},
        {"id": 2, "pos": -1000, "speed": 1000}
    ])
    monitor_during(3)
    time.sleep(2)
    
    print("\n--- [2/4] Speed ---")
    switch_mode(1, [1, 2], 1)
    api.mode_speed(1, [{"id": 1, "speed": 1000}, {"id": 2, "speed": -1000}])
    monitor_during(7)
    api.stop(1)
    time.sleep(2)
    
    print("\n--- [3/4] Step ---")
    switch_mode(1, [1, 2], 3)
    api.mode_step(1, [
        {"id": 1, "steps": -2000, "speed": 1000},
        {"id": 2, "steps": -2000, "speed": 1000}
    ])
    monitor_during(3)
    time.sleep(2)
    
    print("\n--- [4/4] PWM ---")
    switch_mode(1, [1, 2], 2)
    api.mode_pwm(1, [{"id": 1, "pwm": 200}, {"id": 2, "pwm": 1224}])
    monitor_during(7)
    api.stop(1)
    time.sleep(2)
    
    switch_mode(1, [1, 2], 0)
    print("\n? Finish")


def demo_state():

    print("\n=== 状态读取 ===")
    
    # 方式 1: 紧凑数组
    result = api.read(1)
    print(f"\nRaw format: {result}")
    
    # 方式 2: 字典格式（更易用）
    servos = api.read_dict(1)
    print(f"\nDict format:")
    if servos:
        for s in servos:
            print(f"  Servo {s['id']}: pos={s['pos']}, "
                  f"temp={s['temp']}°C, voltage={s['voltage']}V")


def demo_register():
    
    print("\n=== 寄存器分组读取 ===")
    
    result = api.reg_read(1, [
        {"id": 1, "regs": [(56, 2), (63, 1)]},   # 舵机1: 位置+温度
        {"id": 2, "regs": [(42, 2), (60, 2)]},   # 舵机2: 目标位置+负载
    ])
    
    if result and result.get("ok"):
        for r in result["results"]:
            print(f"  ID{r['id']} values={r.get('values', [])}")


def demo_latency():
    
    print("\n=== 延迟测试 ===")
    
    result = api.latency_test(1)
    if result:
        print(f"  树莓派发送: {result['pi_send']:.6f} s")
        print(f"  树莓派接收: {result['pi_recv']:.6f} s")
        print(f"  ESP32 接收: {result['esp_recv']} ms")
        print(f"  ESP32 发送: {result['esp_send']} ms")
        print(f"  ")
        print(f"  总往返 RTT:    {result['rtt']:.2f} ms")
        print(f"  ESP32 处理:    {result['esp_proc']} ms")
        print(f"  纯网络延迟:    {result['network']:.2f} ms")


def main():
    # 1. 启动监控 + 等待节点
    wait_and_discover()
    
    if not monitor.get_online_nodes():
        print("没有在线节点，退出")
        return
    
    # 2. 初始化所有节点
    print("\n=== 初始化节点 ===")
    api.initialize_all()
    
    # 3. 查询当前模式
    print("\n=== 当前模式 ===")
    for nid in monitor.get_online_nodes():
        result = api.get_modes(nid)
        if result and result.get("ok"):
            ids_list = result["ids"]
            modes = result["modes"]
            from config import MODE_NAMES
            for i, mid in enumerate(ids_list):
                print(f"  Node {nid} Servo {mid}: {MODE_NAMES.get(modes[i], '?')}")
    
    # 4. 各种演示
    try:
        demo_modes()
        demo_state()
        demo_register()
        demo_latency()
    except KeyboardInterrupt:
        print("\n中断")
    finally:
        # 停止所有舵机
        print("\n=== 停止 ===")
        for nid in monitor.get_online_nodes():
            api.stop(nid)
        
        print("Done")


if __name__ == "__main__":
    main()
