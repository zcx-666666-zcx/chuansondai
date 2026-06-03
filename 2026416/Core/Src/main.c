/* USER CODE BEGIN Header */
/**
  * MG90S/SG90 参考 CSDN 113447204：20ms PWM，0.5ms~2.5ms 对应 0~180度
  * 信号线 -> PA3 (TIM2_CH4)
  */
/* USER CODE END Header */

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN PD */
/* 与文章一致：0.5ms=0度, 1.5ms=90度, 2.5ms=180度 */
#define SERVO_MIN_US           500U
#define SERVO_MAX_US           2500U
#define SERVO_NEUTRAL_US       1500U
#define SERVO_HOME_ANGLE       0U
#define SERVO_ACTIVE_ANGLE     70U
#define SERVO_BOOT_SETTLE_MS   600U
#define SERVO_SIGNAL_TIMEOUT_MS 200U
/* USER CODE END PD */

void SystemClock_Config(void);

/* USER CODE BEGIN PV */
typedef enum { SERVO_HOME = 0, SERVO_ACTIVE = 1 } ServoRun_t;

volatile uint8_t uart_rx_byte = 0;
volatile uint8_t uart_rx_flag = 0;

static ServoRun_t servo_run = SERVO_HOME;
static uint8_t servo_signal_live = 0;
static uint32_t servo_last_r_tick = 0;
static uint16_t servo_pulse_us = 0;
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
static uint16_t Servo_UsFromAngle(uint8_t angle)
{
  if (angle > 180U) angle = 180U;
  return (uint16_t)(SERVO_MIN_US +
                    ((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US)) / 180U);
}

static void Servo_SetUs(uint16_t us)
{
  if (us < SERVO_MIN_US) us = SERVO_MIN_US;
  if (us > SERVO_MAX_US) us = SERVO_MAX_US;
  if (us == servo_pulse_us) return;
  servo_pulse_us = us;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, us);
}

static void Servo_SetAngle(uint8_t angle)
{
  Servo_SetUs(Servo_UsFromAngle(angle));
}

static void Servo_Init(void)
{
  /* 先 90 度中位再开 PWM，避免一上电就用 0 度顶死限位乱转 */
  Servo_SetUs(SERVO_NEUTRAL_US);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
  HAL_Delay(SERVO_BOOT_SETTLE_MS);

  Servo_SetAngle(SERVO_HOME_ANGLE);
  HAL_Delay(SERVO_BOOT_SETTLE_MS);
  servo_run = SERVO_HOME;
  servo_signal_live = 0;
  servo_last_r_tick = 0;
}

static void Servo_GoHome(void)
{
  if (servo_run == SERVO_HOME) return;
  Servo_SetAngle(SERVO_HOME_ANGLE);
  servo_run = SERVO_HOME;
}

static void Servo_GoActive(void)
{
  if (servo_run == SERVO_ACTIVE) return;
  Servo_SetAngle(SERVO_ACTIVE_ANGLE);
  servo_run = SERVO_ACTIVE;
}

static void Servo_OnR(void)
{
  servo_signal_live = 1;
  servo_last_r_tick = HAL_GetTick();
  Servo_GoActive();
}

static void Servo_Task(void)
{
  if (!servo_signal_live) return;
  if ((HAL_GetTick() - servo_last_r_tick) >= SERVO_SIGNAL_TIMEOUT_MS)
  {
    servo_signal_live = 0;
    Servo_GoHome();
  }
}
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  Servo_Init();
  HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);

  while (1)
  {
    if (uart_rx_flag)
    {
      uart_rx_flag = 0;
      if (uart_rx_byte == 'R') Servo_OnR();
    }
    Servo_Task();
  }
}

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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    uart_rx_flag = 1;
    HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
  }
}
/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif
