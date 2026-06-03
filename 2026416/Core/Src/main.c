/* USER CODE BEGIN Header */
/**
  * MG90S: 上电复位 -> 收到R转到60度 -> 自动回复位
  * 信号线接 PA3 (TIM2_CH4)
  */
/* USER CODE END Header */

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN PD */
#define SERVO_MIN_US           1000U
#define SERVO_MAX_US           2000U
#define SERVO_HOME_ANGLE       0U    /* 复位角(绝对0度)，顶死限位可改成5~15 */
#define SERVO_ACTIVE_ANGLE     60U   /* 工作角(绝对60度) */
#define SERVO_BOOT_SETTLE_MS   800U  /* 上电复位等待 */
#define SERVO_ACTIVE_HOLD_MS   500U  /* 转到60度后保持多久再自动复位 */
/* USER CODE END PD */

void SystemClock_Config(void);

/* USER CODE BEGIN PV */
typedef enum
{
  SERVO_STATE_HOME = 0,
  SERVO_STATE_ACTIVE
} ServoState_t;

volatile uint8_t uart_rx_byte = 0;
volatile uint8_t uart_rx_flag = 0;

static ServoState_t servo_state = SERVO_STATE_HOME;
static uint32_t servo_state_tick = 0;
static uint16_t servo_pulse_us = 0;
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
static uint16_t Servo_AngleToUs(uint8_t angle)
{
  if (angle > 180U) angle = 180U;
  return (uint16_t)(SERVO_MIN_US +
                    ((uint32_t)angle * (SERVO_MAX_US - SERVO_MIN_US)) / 180U);
}

static void Servo_OutputUs(uint16_t us)
{
  if (us < SERVO_MIN_US) us = SERVO_MIN_US;
  if (us > SERVO_MAX_US) us = SERVO_MAX_US;
  if (us == servo_pulse_us) return;
  servo_pulse_us = us;
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, us);
}

static void Servo_GoAngle(uint8_t angle)
{
  Servo_OutputUs(Servo_AngleToUs(angle));
}

static void Servo_ResetHome(void)
{
  Servo_GoAngle(SERVO_HOME_ANGLE);
  servo_state = SERVO_STATE_HOME;
  servo_state_tick = HAL_GetTick();
}

static void Servo_GoActive(void)
{
  Servo_GoAngle(SERVO_ACTIVE_ANGLE);
  servo_state = SERVO_STATE_ACTIVE;
  servo_state_tick = HAL_GetTick();
}

static void Servo_PowerOnReset(void)
{
  Servo_GoAngle(SERVO_HOME_ANGLE);
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
  HAL_Delay(SERVO_BOOT_SETTLE_MS);
  servo_state = SERVO_STATE_HOME;
  servo_pulse_us = Servo_AngleToUs(SERVO_HOME_ANGLE);
}

static void Servo_OnByte(uint8_t b)
{
  if (b != 'R') return;
  if (servo_state == SERVO_STATE_HOME)
  {
    Servo_GoActive();
  }
}

static void Servo_Task(void)
{
  if (servo_state != SERVO_STATE_ACTIVE) return;
  if ((HAL_GetTick() - servo_state_tick) >= SERVO_ACTIVE_HOLD_MS)
  {
    Servo_ResetHome();
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

  Servo_PowerOnReset();
  HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);

  while (1)
  {
    if (uart_rx_flag)
    {
      uart_rx_flag = 0;
      Servo_OnByte(uart_rx_byte);
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
