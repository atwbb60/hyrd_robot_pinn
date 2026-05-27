CMD_PORT = 8888           # ESP32 接收指令端口
REPORT_PORT = 9999        # 树莓派接收上报端口

TIMEOUT = 0.5
RETRIES = 2

NODES_CONFIG = {
    1: {
        "name": "shoulder",       # 语义名称（你自己定义）
        "servos": [
            {
                "id": 1,
                "name": "left",
                "mode": "position",   # position/speed/pwm/step
                "min_pos": 0,
                "max_pos": 0,
            },
            {
                "id": 2,
                "name": "right",
                "mode": "position",
                "min_pos": 0,
                "max_pos": 0,
            }
        ]
    },
}

MODE_MAP = {
    "position": 0,
    "speed": 1,
    "pwm": 2,
    "step": 3,
}

MODE_NAMES = {v: k for k, v in MODE_MAP.items()}
