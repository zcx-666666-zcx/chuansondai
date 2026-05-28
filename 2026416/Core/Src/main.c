/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Servo-only test program
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN PD */
#define SERVO_MIN_US      1000
#define SERVO_MAX_US      2000
#define SERVO_HOME_ANGLE  90
#define SERVO_PUSH_ANGLE  150
#define SERVO_HOLD_MS     500
/* USER CODE END PD */

void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void Servo_WriteUs(uint16_t us);
void Servo_SetAngle(uint8_t angle);
/* USER CODE END PFP */

/* USER CODE BEGIN PV */
volatile uint8_t uart_rx_byte = 0;
volatile uint8_t servo_active = 0;
uint32_t servo_last_rx_tick = 0;
/* USER CODE END PV */

/* USER CODE BEGIN 0 */
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
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();

  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);

  Servo_SetAngle(SERVO_HOME_ANGLE);
  HAL_Delay(1000);
  HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rx_byte, 1);
  /* USER CODE END 2 */

  while (1)
  {
    if (servo_active && (HAL_GetTick() - servo_last_rx_tick >= SERVO_HOLD_MS))
    {
        servo_active = 0;
        Servo_SetAngle(SERVO_HOME_ANGLE);
    }
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

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        if (uart_rx_byte == 'R')
        {
            servo_active = 1;
            servo_last_rx_tick = HAL_GetTick();
            Servo_SetAngle(SERVO_PUSH_ANGLE);
        }

        HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rx_byte, 1);
    }
}
/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */
