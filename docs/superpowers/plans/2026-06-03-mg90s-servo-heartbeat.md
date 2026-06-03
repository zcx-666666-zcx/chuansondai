# MG90S 舵机心跳保持改造实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 MG90S 上电回到初始位，并在 Python 持续发送目标心跳时保持在绝对 60 度，信号消失后约 150ms 自动回位。

**Architecture:** Python 侧负责“目标存在时持续发送 `R` 心跳”，STM32 侧负责“最近收到过 `R` 就保持工作位，超时后回位”。这样把视觉波动与执行安全解耦，串口协议仍保持单字节 `R`。

**Tech Stack:** Python + pytest + pyserial；STM32 HAL + TIM2 PWM + USART1 轮询接收。

---

### 任务 1：固定 Python 心跳发送规则

**Files:**
- Create: `yolov5/tests/test_serial_heartbeat.py`
- Create: `yolov5/serial_heartbeat.py`
- Modify: `yolov5/yolo_serial_control2.py`

- [ ] **步骤 1：先写失败测试**

验证两个规则：

- 发送间隔未到时不应重复发心跳
- 发送间隔到达时应允许再次发心跳

- [ ] **步骤 2：运行测试并确认先失败**

运行：

```bash
pytest yolov5/tests/test_serial_heartbeat.py -q
```

预期：因为辅助模块或函数还不存在而失败。

- [ ] **步骤 3：补最小实现**

新增一个仅负责心跳节流判断的辅助模块，并在 `yolo_serial_control2.py` 中使用它。

- [ ] **步骤 4：再次运行测试确认通过**

运行：

```bash
pytest yolov5/tests/test_serial_heartbeat.py -q
```

预期：测试通过。

### 任务 2：修改 Python 主流程为“目标存在即持续发送”

**Files:**
- Modify: `yolov5/yolo_serial_control2.py`

- [ ] **步骤 1：删除触发线作为串口发送前提**

保留画线显示可以，但串口发送条件改为“当前最佳红色目标存在”。

- [ ] **步骤 2：统一文案与状态显示**

让界面文案反映“保持/回位”语义，而不是“单次推出”语义。

- [ ] **步骤 3：本地做语法级验证**

运行：

```bash
python3 -m py_compile yolov5/serial_heartbeat.py yolov5/yolo_serial_control2.py
```

预期：无输出，退出码为 0。

### 任务 3：修改 STM32 为超时保持 60 度

**Files:**
- Modify: `2026416/Core/Src/main.c`

- [ ] **步骤 1：替换角度与超时常量**

把现有 `SERVO_PUSH_ANGLE` / `SERVO_HOLD_MS` 改为：

- `SERVO_ACTIVE_ANGLE 60`
- `SERVO_SIGNAL_TIMEOUT_MS 150`

- [ ] **步骤 2：保留上电复位，去掉旧的自检摆动流程**

上电后直接回 `HOME`，不再执行 `90 -> 150 -> 90` 的旧测试动作。

- [ ] **步骤 3：修改串口处理逻辑**

收到 `R` 时只刷新最近接收时间并保持在 60 度；如果超过超时时间未再收到 `R`，则自动回位。

- [ ] **步骤 4：做源码级复核**

确认 PWM 引脚、UART 波特率和阻塞轮询方式未被破坏。

### 任务 4：执行验证并记录限制

**Files:**
- Modify: `docs/superpowers/specs/2026-06-03-mg90s-servo-heartbeat-design.md`

- [ ] **步骤 1：运行 Python 侧测试**

```bash
pytest yolov5/tests/test_serial_heartbeat.py -q
```

- [ ] **步骤 2：运行 Python 语法检查**

```bash
python3 -m py_compile yolov5/serial_heartbeat.py yolov5/yolo_serial_control2.py
```

- [ ] **步骤 3：记录固件侧限制**

明确说明：本地未做 Keil/MDK 编译，改完后仍需你在 MDK 中重新编译并烧录验证硬件。
