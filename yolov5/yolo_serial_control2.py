import time
import cv2
import torch
import serial
import numpy as np

MODEL_PATH = "best.pt"
CAMERA_INDEX = 0
SERIAL_PORT = "/dev/cu.usbserial-14220"
BAUDRATE = 9600

CONF_THRES = 0.35
IMG_SIZE = 640

# 当前先测试红色推出
TARGET_COLOR = "red_block"

SHOW_WINDOW = True


def open_serial(port, baudrate):
    try:
        ser = serial.Serial(port, baudrate, timeout=0.05)
        time.sleep(2)

        # 清掉STM32上电时可能残留的启动信息
        ser.reset_input_buffer()
        ser.reset_output_buffer()

        print(f"[INFO] 串口已打开: {port} @ {baudrate}")
        return ser
    except Exception as e:
        print(f"[ERROR] 串口打开失败: {e}")
        return None


def send_command(ser, command):
    if ser is None:
        print("[WARN] 串口未连接")
        return

    if command not in {"R", "S"}:
        return

    try:
        ser.reset_input_buffer()
        ser.write(command.encode("utf-8"))
        ser.flush()
        print(f"[TX] {command}")
    except Exception as e:
        print(f"[ERROR] 串口发送失败: {e}")
        return

    # 等待STM32回复
    ack_timeout_s = 1.0
    start = time.time()
    reply_buf = ""

    while time.time() - start < ack_timeout_s:
        if ser.in_waiting > 0:
            reply_buf += ser.read(ser.in_waiting).decode("utf-8", errors="ignore")

            # 看到关键字就认为回复到了
            if "[OK] GET R" in reply_buf or "[RX BYTE]" in reply_buf:
                break

        time.sleep(0.01)

    if reply_buf.strip():
        print("[RX]", reply_buf.strip())
    else:
        print("[RX] 未收到STM32回复")


def get_center_roi(frame, x1, y1, x2, y2, shrink=0.45):
    w = x2 - x1
    h = y2 - y1

    cx1 = int(x1 + w * (1 - shrink) / 2)
    cy1 = int(y1 + h * (1 - shrink) / 2)
    cx2 = int(x2 - w * (1 - shrink) / 2)
    cy2 = int(y2 - h * (1 - shrink) / 2)

    cx1 = max(0, cx1)
    cy1 = max(0, cy1)
    cx2 = min(frame.shape[1], cx2)
    cy2 = min(frame.shape[0], cy2)

    if cx2 <= cx1 or cy2 <= cy1:
        return None, (x1, y1, x2, y2)

    return frame[cy1:cy2, cx1:cx2], (cx1, cy1, cx2, cy2)


def classify_final_color(roi_bgr):
    if roi_bgr is None or roi_bgr.size == 0:
        return None, {}

    hsv = cv2.cvtColor(roi_bgr, cv2.COLOR_BGR2HSV)

    sat_mask = hsv[:, :, 1] > 60
    val_mask = hsv[:, :, 2] > 50
    valid_mask = sat_mask & val_mask

    if np.count_nonzero(valid_mask) < 20:
        return None, {}

    red1 = cv2.inRange(hsv, np.array([0, 70, 50]),   np.array([12, 255, 255]))
    red2 = cv2.inRange(hsv, np.array([170, 70, 50]), np.array([179, 255, 255]))
    yellow = cv2.inRange(hsv, np.array([18, 70, 50]), np.array([38, 255, 255]))
    green = cv2.inRange(hsv, np.array([40, 60, 40]),  np.array([95, 255, 255]))
    blue = cv2.inRange(hsv, np.array([95, 60, 40]),   np.array([135, 255, 255]))

    valid_u8 = valid_mask.astype(np.uint8) * 255

    red_mask = cv2.bitwise_and(cv2.bitwise_or(red1, red2), valid_u8)
    yellow_mask = cv2.bitwise_and(yellow, valid_u8)
    green_mask = cv2.bitwise_and(green, valid_u8)
    blue_mask = cv2.bitwise_and(blue, valid_u8)

    scores = {
        "red_block": int(np.count_nonzero(red_mask)),
        "yellow_block": int(np.count_nonzero(yellow_mask)),
        "green_block": int(np.count_nonzero(green_mask)),
        "blue_block": int(np.count_nonzero(blue_mask)),
    }

    best_class = max(scores, key=scores.get)
    best_score = scores[best_class]
    total_valid = int(np.count_nonzero(valid_mask))

    ratio = best_score / max(total_valid, 1)
    if ratio < 0.12:
        return None, scores

    return best_class, scores


def display_name(color_name):
    mapping = {
        "red_block": "RED",
        "yellow_block": "YELLOW",
        "green_block": "GREEN",
        "blue_block": "BLUE",
        None: "UNKNOWN"
    }
    return mapping.get(color_name, "UNKNOWN")


def main():
    print("[INFO] 正在加载模型...")
    model = torch.hub.load('.', 'custom', path=MODEL_PATH, source='local')
    model.conf = CONF_THRES
    model.iou = 0.45
    model.classes = None
    model.max_det = 100
    print("[INFO] 模型加载完成")

    ser = open_serial(SERIAL_PORT, BAUDRATE)

    cap = cv2.VideoCapture(CAMERA_INDEX)
    if not cap.isOpened():
        print("[ERROR] 摄像头打开失败")
        return

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 1280)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 720)

    target_present = False

    print("[INFO] 按 q 退出")

    while True:
        ret, frame = cap.read()
        if not ret:
            print("[WARN] 摄像头读取失败")
            continue

        results = model(frame, size=IMG_SIZE)
        df = results.pandas().xyxy[0]

        best_target = None
        best_score = -1.0

        for _, row in df.iterrows():
            conf = float(row["confidence"])
            x1 = int(row["xmin"])
            y1 = int(row["ymin"])
            x2 = int(row["xmax"])
            y2 = int(row["ymax"])

            cx = (x1 + x2) // 2
            cy = (y1 + y2) // 2
            area = max(1, (x2 - x1) * (y2 - y1))

            roi, (rx1, ry1, rx2, ry2) = get_center_roi(frame, x1, y1, x2, y2, shrink=0.45)
            final_color, scores = classify_final_color(roi)

            # 主框
            cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 180, 255), 2)
            cv2.circle(frame, (cx, cy), 4, (0, 0, 255), -1)

            # 中心判色区域
            cv2.rectangle(frame, (rx1, ry1), (rx2, ry2), (255, 255, 0), 2)

            # 保持原有显示逻辑
            label = f"{display_name(final_color)} {conf:.2f}"
            cv2.putText(
                frame,
                label,
                (x1, max(25, y1 - 10)),
                cv2.FONT_HERSHEY_SIMPLEX,
                0.7,
                (0, 255, 0),
                2
            )

            if final_color == TARGET_COLOR:
                score = conf + area * 1e-7
                if score > best_score:
                    best_score = score
                    best_target = {
                        "final_color": final_color,
                        "conf": conf,
                        "cx": cx,
                        "cy": cy,
                        "scores": scores
                    }

        cv2.putText(frame, f"TARGET: {display_name(TARGET_COLOR)}", (20, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 0), 2)

        if best_target is not None:
            conf = best_target["conf"]
            final_color = best_target["final_color"]

            cv2.putText(frame, f"FINAL: {display_name(final_color)} {conf:.2f}", (20, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)

            if not target_present:
                send_command(ser, "R")
                target_present = True

            cv2.putText(frame, "ACTION: ROTATE", (20, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
        else:
            if target_present:
                send_command(ser, "S")
                target_present = False

            cv2.putText(frame, "FINAL: NONE", (20, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
            cv2.putText(frame, "ACTION: RESET", (20, 90),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2)

        if SHOW_WINDOW:
            cv2.imshow("Color Sorting System", frame)

        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

    if ser is not None and ser.is_open:
        ser.close()

    print("[INFO] 程序退出")


if __name__ == "__main__":
    main()
