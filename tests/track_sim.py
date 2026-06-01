#!/usr/bin/env python3
"""
T-JSON 协议模拟服务器 - 测试地图轨迹跟踪

本脚本模拟 AI 相机端，通过 TCP 8089 向 T-JSON 客户端持续发送
设备位置（ZoomInfo）和跟踪目标（AIInfo）数据。

使用方法：
  1. 运行本脚本
  2. 打开 T-JSON 客户端，连接到本机 IP:8089
  3. 打开地图，观察目标轨迹绘制
"""

import socket
import struct
import json
import time
import math
import sys

# ============================================================
# T-JSON 协议常量
# ============================================================
FRAME_HEADER = b'\xEC\x91'
FRAME_TYPE_STATUS = 0x01  # 状态帧 (S->C)
HEARTBEAT_RESP = bytes.fromhex('EC911100000000')


def build_frame(frame_type: int, payload: bytes) -> bytes:
    """构建 T-JSON 协议帧
    格式: [0xEC][0x91][type(1B)][length(4B 大端)][payload]
    """
    length = len(payload)
    frame = bytearray()
    frame.extend(FRAME_HEADER)
    frame.append(frame_type)
    frame.extend(struct.pack('!I', length))
    frame.extend(payload)
    return bytes(frame)


def zoominfo_frame(lat: float, lon: float, height: float = 50,
                   pan: float = 0, tilt: float = 0, laser_range: float = 800,
                   vis_zoom: float = 10.0, ir_zoom: float = 5.0,
                   cam_show_mode: int = 1) -> bytes:
    """构建 ZoomInfo 状态帧 - 更新设备 GPS / 云台角度 / 激光测距"""
    data = {
        "ControlType": "ZoomInfo",
        "ZoomInfo": vis_zoom,
        "ZoomInfoIR": ir_zoom,
        "CamShowMode": cam_show_mode,
        "Latitude": str(lat),
        "Longitude": str(lon),
        "Height": height,
        "LaserRange": laser_range,
        "PTZInfoH": pan,
        "PTZInfoV": tilt
    }
    return build_frame(FRAME_TYPE_STATUS, json.dumps(data).encode())


def aiinfo_frame(objects: list, work_mode: int = 2) -> bytes:
    """构建 AIInfo 状态帧

    objects: [
        { id, cls, distance, left, top, right, bottom },
        ...
    ]
    work_mode: 2=自动跟踪, 3=点选跟踪, 4=框选跟踪
    """
    obj_dict = {}
    for o in objects:
        obj_dict[o['id']] = {
            "Class": o.get('cls', 0xB1),
            "Distance": o.get('distance', 500),
            "Points": {
                "Left": o['left'],
                "Top": o['top'],
                "Right": o['right'],
                "Bottom": o['bottom']
            }
        }
    data = {
        "ControlType": "AIInfo",
        "WorkMode": work_mode,
        "ObjectCount": len(objects),
        "Object": obj_dict
    }
    return build_frame(FRAME_TYPE_STATUS, json.dumps(data).encode())


# ============================================================
# 模拟服务器
# ============================================================
class SimServer:
    def __init__(self, host: str = '0.0.0.0', port: int = 8089):
        self.host = host
        self.port = port
        self.server = None
        self.client = None
        self.running = False

    def start(self):
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind((self.host, self.port))
        self.server.listen(1)
        print(f"[监听] {self.host}:{self.port}，等待 T-JSON 客户端...")

        self.client, addr = self.server.accept()
        self.client.settimeout(1)
        print(f"[连接] 客户端已连接: {addr}")
        self.running = True

    def send(self, data: bytes) -> bool:
        if self.client and self.running:
            try:
                self.client.sendall(data)
                return True
            except Exception as e:
                print(f"[错误] 发送失败: {e}")
                self.running = False
                return False
        return False

    def discard_input(self):
        """丢弃客户端发来的数据（心跳/控制指令等）"""
        if self.client and self.running:
            try:
                self.client.recv(4096)
            except socket.timeout:
                pass
            except:
                self.running = False

    def stop(self):
        self.running = False
        for s in (self.client, self.server):
            if s:
                try:
                    s.close()
                except:
                    pass
        print("[关闭] 服务器已停止")


# ============================================================
# 测试场景（持续运行）
# ============================================================
def run_track_scenario(server: SimServer):
    """持续模拟多个目标轮换运动，用于长期跟踪测试"""

    # 设备位置（北京）
    LAT, LON = 39.9042, 116.4074
    CX, CY = 1344, 760   # 图像中心 (2688x1520)
    PAN, TILT = 45.0, 10.0
    RANGE = 800

    print(f"\n位置: {LAT:.4f}, {LON:.4f}")
    print(f"方向: Pan={PAN:.1f}°, Tilt={TILT:.1f}°  距离: {RANGE}m")
    print("持续运行中，每轮新轨迹清空旧轨迹\n")

    # 发送初始设备位置
    server.send(zoominfo_frame(LAT, LON, height=50, pan=PAN,
                               tilt=TILT, laser_range=RANGE))
    server.discard_input()
    time.sleep(1.5)

    step = 0
    while server.running:
        # 云台每秒转 5°
        PAN = (45.0 + step * 1.0) % 360  # 每200ms转1° → 每秒5°

        # 每 30 步启用一个新目标 ID（旧轨迹会被保留）
        # 每 90 步清空所有轨迹重新开始
        if step % 90 == 0:
            TID = f"T{step // 90 + 1:03d}"
            print(f"\n[新轨迹] 目标 {TID}")
        elif step % 30 == 0:
            TID = f"T{step // 30:03d}"
            print(f"[新目标] 目标 {TID}")

        TID = f"T{(step // 30) % 3 + 1:03d}"

        # 轨迹参数：不同目标在不同位置/相位
        phase = (step // 30) * math.pi * 0.5
        t = (step % 30) / 29.0
        angle = t * 2 * math.pi * 1.5 + phase
        r = 300 + 100 * math.sin(t * math.pi * 3)
        dx = int(r * math.cos(angle))
        dy = int(r * math.sin(angle) * 0.6)

        bw, bh = 60, 45

        # 模拟失锁：短时丢失(步12~15)和长时丢失(步60~89)
        t_step = step % 90
        cls_val = 0xB2 if (t_step >= 60) or (12 <= t_step < 16) else 0xB1

        obj = {
            'id': TID,
            'cls': cls_val,
            'distance': RANGE,
            'left': max(0, CX + dx - bw // 2),
            'top': max(0, CY + dy - bh // 2),
            'right': min(2687, CX + dx + bw // 2),
            'bottom': min(1519, CY + dy + bh // 2),
        }
        server.send(aiinfo_frame([obj]))
        server.discard_input()

        # 每步刷新 ZoomInfo 保持 FOV 更新
        server.send(zoominfo_frame(LAT, LON, height=50,
                                   pan=PAN, tilt=TILT, laser_range=RANGE))
        server.discard_input()

        timestamp = time.strftime("%H:%M:%S")
        status = "锁定" if cls_val == 0xB1 else "丢失"
        print(f"  [{timestamp}] {TID:5s} {status} Pan={PAN:6.1f}° 偏移 dx={dx:+4d} dy={dy:+4d}")
        time.sleep(0.2)
        step += 1


# ============================================================
# 主程序
# ============================================================
def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8089

    server = SimServer(port=port)
    try:
        server.start()
        print("按 Ctrl+C 终止\n")

        time.sleep(2)
        run_track_scenario(server)

    except KeyboardInterrupt:
        print("\n[中断]")
    finally:
        server.stop()


if __name__ == '__main__':
    main()
