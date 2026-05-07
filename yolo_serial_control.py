import time

import cv2
import numpy as np
import serial
import torch

MODEL_PATH = "best.pt"
CAMERA_INDEX = 0
SERIAL_PORT = "/dev/cu.usbserial-14220"
BAUDRATE = 9600

CONF_THRES = 0.35
IMG_SIZE = 640

# 当前只锁定红色方块触发
TARGET_COLOR = "red_block"

TRIGGER_X_RATIO = 0.70
SEND_COOLDOWN = 1.2
SHOW_WINDOW = True

# ========== 修复1：重新校准HSV颜色阈值（适配你的灯光） ==========
# 精准区分四色，彻底解决颜色认错
COLOR_HSV = {
    "red_block": [(0, 100, 80), (10, 255, 255), (170, 100, 80), (180, 255, 255)],
    "yellow_block": [(15, 80, 100), (35, 255, 255)],
    "green_block": [(40, 70, 70), (80, 255, 255)],
    "blue_block": [(90, 80, 80), (130, 255, 255)],
}


def open_serial(port, baudrate):
    try:
        set = serial.Serial(port, baudrate, timeout=0.1)
        time.sleep(2)
        print(f"[INFO] 串口已打开: {port} @ {baudrate}")
        return set
    except Exception as e:
        print(f"[ERROR] 串口打开失败: {e}")
        return None


def send_command(set, final_color):
    if set is None:
        print("[WARN] 串口未连接")
        return
    # 红色方块触发，发送R给STM32
    if final_color == "red_block":
        set.write(b"R")
        print("[TX 发送指令] R → STM32 准备推料")


def get_center_roi(frame, x1, y1, x2, y2, shrink=0.55):
    # 缩小采样区域，只取方块中心，避开边缘、阴影干扰
    w = x2 - x1
    h = y2 - y1
    cx1 = int(x1 + w * (1 - shrink) / 2)
    cy1 = int(y1 + h * (1 - shrink) / 2)
    cx2 = int(x2 - w * (1 - shrink) / 2)
    cy2 = int(y2 - h * (1 - shrink) / 2)

    cx1, cy1 = max(0, cx1), max(0, cy1)
    cx2, cy2 = min(frame.shape[1], cx2), min(frame.shape[0], cy2)

    if cx2 <= cx1 or cy2 <= cy1:
        return None, (x1, y1, x2, y2)
    return frame[cy1:cy2, cx1:cx2], (cx1, cy1, cx2, cy2)


# ========== 修复2：重写颜色判定函数，彻底解决颜色乱判 ==========
def classify_final_color(roi_bgr):
    if roi_bgr is None or roi_bgr.size == 0:
        return None, {}

    # OpenCV默认读取BGR，转标准HSV
    hsv = cv2.cvtColor(roi_bgr, cv2.COLOR_BGR2HSV)
    scores = {}

    # 红色特殊处理（两段区间）
    low1, high1, low2, high2 = COLOR_HSV["red_block"]
    red_mask1 = cv2.inRange(hsv, np.array(low1), np.array(high1))
    red_mask2 = cv2.inRange(hsv, np.array(low2), np.array(high2))
    scores["red_block"] = cv2.countNonZero(red_mask1 + red_mask2)

    # 其他单色
    for c in ["yellow_block", "green_block", "blue_block"]:
        low, high = COLOR_HSV[c]
        mask = cv2.inRange(hsv, np.array(low), np.array(high))
        scores[c] = cv2.countNonZero(mask)

    # 找占比最高的真实颜色
    best_class = max(scores, key=scores.get)
    best_pixel = scores[best_class]
    total_pixel = roi_bgr.shape[0] * roi_bgr.shape[1]

    # 颜色占比超过20%才判定，过滤误识别
    if best_pixel / total_pixel < 0.2:
        return None, scores

    return best_class, scores


def display_name(color_name):
    mapping = {
        "red_block": "RED",
        "yellow_block": "YELLOW",
        "green_block": "GREEN",
        "blue_block": "BLUE",
        None: "UNKNOWN",
    }
    return mapping.get(color_name, "UNKNOWN")


def main():
    print("[INFO] 正在加载模型...")
    model = torch.hub.load(".", "custom", path=MODEL_PATH, source="local")
    model.conf = CONF_THRES
    model.iou = 0.45
    print("[INFO] 模型加载完成")

    set = open_serial(SERIAL_PORT, BAUDRATE)

    cap = cv2.VideoCapture(CAMERA_INDEX)
    if not cap.isOpened():
        print("[ERROR] 摄像头打开失败")
        return
    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    last_send_time = 0
    print("[INFO] 运行中，按 q 退出")

    while True:
        ret, frame = cap.read()
        if not ret:
            continue

        h, w = frame.shape[:2]
        trigger_x = int(w * TRIGGER_X_RATIO)

        results = model(frame, size=IMG_SIZE)
        df = results.pandas().xyxy[0]

        best_target = None
        best_score = -1.0

        for _, row in df.iterrows():
            conf = float(row["confidence"])
            x1, y1 = int(row["xmin"]), int(row["ymin"])
            x2, y2 = int(row["xmax"]), int(row["ymax"])

            cx = (x1 + x2) // 2
            roi, (rx1, ry1, rx2, ry2) = get_center_roi(frame, x1, y1, x2, y2)
            final_color, _scores = classify_final_color(roi)

            # 画检测框+中心点
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 180, 255), 2)
            cv2.circle(frame, (cx, (y1 + y2) // 2), 5, (0, 0, 255), -1)
            cv2.rectangle(frame, (rx1, ry1), (rx2, ry2), (255, 255, 0), 1)

            # 显示正确颜色名称
            label = f"{display_name(final_color)} {conf:.2f}"
            cv2.putText(frame, label, (x1, max(25, y1 - 10)), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            # 只追踪目标红色方块
            if final_color == TARGET_COLOR:
                score = conf
                if score > best_score:
                    best_score = score
                    best_target = {"final_color": final_color, "cx": cx}

        # 画触发竖线
        cv2.line(frame, (trigger_x, 0), (trigger_x, h), (255, 0, 0), 2)
        cv2.putText(frame, "TARGET: RED", (20, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 0), 2)

        # 越过竖线，防抖发送指令
        if best_target:
            cx = best_target["cx"]
            cv2.putText(frame, "DETECT RED", (20, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
            now = time.time()
            if cx >= trigger_x and (now - last_send_time) >= SEND_COOLDOWN:
                send_command(set, "red_block")
                last_send_time = now
                cv2.putText(frame, "✅ PUSH NOW", (20, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
        else:
            cv2.putText(frame, "FINAL: NONE", (20, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)

        cv2.imshow("Color Sorting System", frame)
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()
    if set and set.is_open:
        set.close()
    print("[INFO] 程序已退出")


if __name__ == "__main__":
    main()
