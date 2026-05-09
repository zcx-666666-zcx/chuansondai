/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
    SERVO_IDLE = 0,
    SERVO_PUSH_HOLD,
    SERVO_RETURN_HOLD
} ServoState_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// 舵机参数（TIM2_CH4）
#define SERVO_MIN_US        1000
#define SERVO_MAX_US        2000
#define SERVO_MID_ANGLE     90
#define SERVO_PUSH_ANGLE    140
#define SERVO_PUSH_TIME_MS  250
#define SERVO_BACK_TIME_MS  250

// 步进参数
#define STEPPER_HALF_PERIOD_MS      2

#define STEPPER_FORWARD             0
#define STEPPER_REVERSE             1

#define STEPPER_EN_ACTIVE_LOW       1
#define STEPPER_DIR_ACTIVE_LOW      1
#define STEPPER_PUL_ACTIVE_LOW      1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile uint8_t uart_rx_byte = 0;
volatile uint8_t cmd_servo_push_flag = 0;
volatile uint8_t uart_ack_flag = 0;

volatile uint8_t uart_rx_seen_flag = 0;
volatile uint8_t uart_last_byte = 0;

ServoState_t servo_state = SERVO_IDLE;
uint32_t servo_state_tick = 0;

uint8_t stepper_pul_state = 0;
uint32_t stepper_last_toggle_ms = 0;
uint8_t stepper_running = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Uart_SendString(char *str);

void Servo_WriteUs(uint16_t us);
void Servo_SetAngle(uint8_t angle);
void Servo_StartPushSequence(void);
void Servo_Service(void);

void Stepper_Enable(void);
void Stepper_Disable(void);
void Stepper_SetDir(uint8_t dir);
void Stepper_PulOn(void);
void Stepper_PulOff(void);
void Stepper_Service(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Uart_SendString(char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 100);
}

/* =========================
   舵机控制
   ========================= */
void Servo_WriteUs(uint16_t us)
{
    if (us < SERVO_MIN_US) us = SERVO_MIN_US;
    if (us > SERVO_MAX_US) us = SERVO_MAX_US;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, us);
}

void Servo_SetAngle(uint8_t angle)
{
    uint16_t us;

    if (angle > 180) angle = 180;
    us = SERVO_MIN_US + (uint16_t)angle * (SERVO_MAX_US - SERVO_MIN_US) / 180;
    Servo_WriteUs(us);
}

void Servo_StartPushSequence(void)
{
    if (servo_state == SERVO_IDLE)
    {
        Servo_SetAngle(SERVO_PUSH_ANGLE);
        servo_state = SERVO_PUSH_HOLD;
        servo_state_tick = HAL_GetTick();
    }
}

void Servo_Service(void)
{
    uint32_t now = HAL_GetTick();

    switch (servo_state)
    {
        case SERVO_IDLE:
            break;

        case SERVO_PUSH_HOLD:
            if (now - servo_state_tick >= SERVO_PUSH_TIME_MS)
            {
                Servo_SetAngle(SERVO_MID_ANGLE);
                servo_state = SERVO_RETURN_HOLD;
                servo_state_tick = now;
            }
            break;

        case SERVO_RETURN_HOLD:
            if (now - servo_state_tick >= SERVO_BACK_TIME_MS)
            {
                servo_state = SERVO_IDLE;
            }
            break;

        default:
            servo_state = SERVO_IDLE;
            break;
    }
}

/* =========================
   步进电机控制
   ========================= */
void Stepper_Enable(void)
{
#if STEPPER_EN_ACTIVE_LOW
    HAL_GPIO_WritePin(ENA_GPIO_Port, ENA_Pin, GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(ENA_GPIO_Port, ENA_Pin, GPIO_PIN_SET);
#endif
}

void Stepper_Disable(void)
{
#if STEPPER_EN_ACTIVE_LOW
    HAL_GPIO_WritePin(ENA_GPIO_Port, ENA_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(ENA_GPIO_Port, ENA_Pin, GPIO_PIN_RESET);
#endif
}

void Stepper_SetDir(uint8_t dir)
{
    if (dir == STEPPER_FORWARD)
    {
#if STEPPER_DIR_ACTIVE_LOW
        HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
#else
        HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_SET);
#endif
    }
    else
    {
#if STEPPER_DIR_ACTIVE_LOW
        HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_SET);
#else
        HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);
#endif
    }
}

void Stepper_PulOn(void)
{
#if STEPPER_PUL_ACTIVE_LOW
    HAL_GPIO_WritePin(PUL_GPIO_Port, PUL_Pin, GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(PUL_GPIO_Port, PUL_Pin, GPIO_PIN_SET);
#endif
}

void Stepper_PulOff(void)
{
#if STEPPER_PUL_ACTIVE_LOW
    HAL_GPIO_WritePin(PUL_GPIO_Port, PUL_Pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(PUL_GPIO_Port, PUL_Pin, GPIO_PIN_RESET);
#endif
}

void Stepper_Service(void)
{
    uint32_t now = HAL_GetTick();

    if (!stepper_running)
    {
        Stepper_PulOff();
        return;
    }

    if (now - stepper_last_toggle_ms >= STEPPER_HALF_PERIOD_MS)
    {
        stepper_last_toggle_ms = now;

        if (stepper_pul_state == 0)
        {
            Stepper_PulOn();
            stepper_pul_state = 1;
        }
        else
        {
            Stepper_PulOff();
            stepper_pul_state = 0;
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */

  // 启动舵机PWM
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
  HAL_Delay(500);
  Servo_SetAngle(SERVO_MID_ANGLE);
  HAL_Delay(300);

  // 步进电机初始化
  Stepper_PulOff();
  Stepper_SetDir(STEPPER_FORWARD);
  Stepper_Enable();
  stepper_running = 1;
  stepper_last_toggle_ms = HAL_GetTick();

  // 开启串口接收中断（1字节）
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rx_byte, 1);

  Uart_SendString("System Ready\r\n");
  Uart_SendString("Stepper Running...\r\n");
  Uart_SendString("Send 'R' to push servo\r\n");

  /* USER CODE END 2 */

  while (1)
  {
    Stepper_Service();
    Servo_Service();

    // 打印收到的原始字节
    if (uart_rx_seen_flag)
    {
        char msg[40];
        uart_rx_seen_flag = 0;

        if (uart_last_byte >= 32 && uart_last_byte <= 126)
        {
            sprintf(msg, "[RX BYTE] 0x%02X '%c'\r\n", uart_last_byte, uart_last_byte);
        }
        else
        {
            sprintf(msg, "[RX BYTE] 0x%02X\r\n", uart_last_byte);
        }

        Uart_SendString(msg);
    }

    // 收到R后，舵机动作
    if (cmd_servo_push_flag)
    {
        cmd_servo_push_flag = 0;
        Servo_StartPushSequence();
    }

    // 收到R后的ACK
    if (uart_ack_flag)
    {
        uart_ack_flag = 0;
        Uart_SendString("[OK] GET R\r\n");
    }
  }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uart_last_byte = uart_rx_byte;
        uart_rx_seen_flag = 1;

        if (uart_rx_byte == 'R')
        {
            cmd_servo_push_flag = 1;
            uart_ack_flag = 1;
        }

        HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rx_byte, 1);
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */