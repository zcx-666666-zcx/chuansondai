# 智能传送带（YOLOv5 视觉检测 + STM32 舵机控制）

基于视觉检测的传送带控制项目：PC 端运行 **YOLOv5** 对摄像头画面中的目标进行检测，检测到目标时通过**串口**向 STM32 发送单字节 `R` 心跳指令，STM32 使用 **TIM2 PWM** 驱动 **MG90S 舵机**完成动作。

核心设计是**心跳保持机制**——Python 侧"目标存在时持续发送 `R` 心跳"，STM32 侧"最近收到过 `R` 就保持工作位，信号超时（约 150ms）后自动回位"。这样把视觉检测的抖动与执行机构的安全解耦：单帧漏检不会导致舵机来回摆动，目标消失后舵机可靠回位。

## 目录结构

| 目录 | 内容 |
|------|------|
| `2026416/` | STM32 工程（Keil MDK + CubeMX，`.ioc` 工程文件 + Core/Drivers/MDK-ARM），按日期命名的迭代版本 |
| `2026507/` | STM32 工程的后续迭代版本 |
| `yolov5/` | 基于 [ultralytics/yolov5](https://github.com/ultralytics/yolov5) 上游代码，新增串口控制相关脚本与测试 |
| `docs/` | MG90S 舵机资料（PDF）、设计文档 |

## 串口控制脚本（yolov5/）

| 脚本 | 说明 |
|------|------|
| `yolo_serial_control.py` / `yolo_serial_control1.py` / `yolo_serial_control2.py` | YOLO 检测结果驱动串口的控制脚本（不同迭代版本） |
| `serial_heartbeat.py` | 心跳发送/保持逻辑（含 `should_send_heartbeat`、`should_keep_target` 纯函数，便于单元测试） |
| `serial_test.py` / `serial_send_r_cycle_test.py` | 串口收发联调脚本 |
| `tests/` | pytest 单元测试（心跳间隔、目标保持等规则） |

## 硬件说明

- 主控：STM32（HAL 库，TIM2_CH4 输出 20ms 周期 PWM，0.5ms~2.5ms 脉宽对应舵机 0°~180°）
- 舵机：MG90S / SG90，信号线接 PA3；上电回初始位，检测到目标时保持在工作位（不同迭代版本为 60°/70°）
- 通信：USART1 单字节协议（`R` = 保持动作位）

## 开发文档

- [docs/superpowers/specs/2026-06-03-mg90s-servo-heartbeat-design.md](docs/superpowers/specs/2026-06-03-mg90s-servo-heartbeat-design.md) — 心跳保持机制设计
- [docs/superpowers/plans/2026-06-03-mg90s-servo-heartbeat.md](docs/superpowers/plans/2026-06-03-mg90s-servo-heartbeat.md) — 实施计划
- [docs/mg-90s-servo-motor.pdf](docs/mg-90s-servo-motor.pdf) — MG90S 舵机资料
