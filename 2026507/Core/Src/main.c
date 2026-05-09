#include "main.h"

/*
 * STM32F103C8T6 + Microstep Driver + 42步进电机
 *
 * 接线：
 * PA0  -> PUL+
 * GND  -> PUL-
 * PA1  -> DIR+
 * GND  -> DIR-
 * ENA  -> 先不接
 *
 * 驱动器：
 * VCC -> 12V+
 * GND -> 12V-
 * STM32 GND 必须和驱动器 GND 共地
 *
 * 电机：
 * 黑 -> A+
 * 红 -> A-
 * 蓝 -> B+
 * 绿 -> B-
 */

TIM_HandleTypeDef htim2;

/* ========== 用户可改参数 ========== */

/*
 * 你的 TIM2 经过 72 分频后，计数频率是 1MHz
 * 72MHz / 72 = 1MHz
 */
#define TIMER_CLK_HZ        1000000UL

/*
 * 启动频率，单位 Hz
 * 越低越容易启动
 */
#define START_FREQ_HZ       100

/*
 * 目标频率，单位 Hz
 * 8细分时，1600脉冲一圈
 * 800Hz 大约 30RPM
 */
#define TARGET_FREQ_HZ      800

/*
 * 加速步进
 */
#define ACCEL_STEP_HZ       20

/*
 * 每次加速间隔
 */
#define ACCEL_DELAY_MS      30

/* 方向控制引脚 */
#define DIR_GPIO_PORT       GPIOA
#define DIR_GPIO_PIN        GPIO_PIN_1

/* 函数声明 */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void Stepper_SetFrequency(uint32_t freq_hz);
static void Stepper_Start_With_Accel(void);
void Error_Handler(void);

int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM2_Init();

    /*
     * 设置方向
     * 如果方向反了，把 GPIO_PIN_SET 改成 GPIO_PIN_RESET
     */
    HAL_GPIO_WritePin(DIR_GPIO_PORT, DIR_GPIO_PIN, GPIO_PIN_SET);

    HAL_Delay(500);

    /*
     * 上电后自动启动，并加速到匀速
     */
    Stepper_Start_With_Accel();

    while (1)
    {
        /*
         * 不需要在 while 里翻转 PA0
         * TIM2 硬件 PWM 会一直自动输出脉冲
         * 所以电机会一直匀速旋转
         */
    }
}

/**
 * @brief 设置步进脉冲频率
 * @param freq_hz 频率，单位 Hz
 */
static void Stepper_SetFrequency(uint32_t freq_hz)
{
    if (freq_hz < 1)
    {
        freq_hz = 1;
    }

    if (freq_hz > 20000)
    {
        freq_hz = 20000;
    }

    uint32_t period = TIMER_CLK_HZ / freq_hz;

    if (period < 10)
    {
        period = 10;
    }

    uint32_t arr = period - 1;
    uint32_t pulse = period / 2;

    __HAL_TIM_DISABLE(&htim2);

    __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
    __HAL_TIM_SET_COUNTER(&htim2, 0);

    __HAL_TIM_ENABLE(&htim2);
}

/**
 * @brief 启动步进电机，并缓慢加速到目标速度
 */
static void Stepper_Start_With_Accel(void)
{
    Stepper_SetFrequency(START_FREQ_HZ);

    if (HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    for (uint32_t f = START_FREQ_HZ; f <= TARGET_FREQ_HZ; f += ACCEL_STEP_HZ)
    {
        Stepper_SetFrequency(f);
        HAL_Delay(ACCEL_DELAY_MS);
    }

    Stepper_SetFrequency(TARGET_FREQ_HZ);
}

/**
 * @brief TIM2 初始化
 */
static void MX_TIM2_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 72 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 10000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 5000;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief GPIO 初始化
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*
     * PA0 -> TIM2_CH1 -> PUL+
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /*
     * PA1 -> DIR+
     */
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);
}

/**
 * @brief 系统时钟配置：72MHz
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /*
     * 使用外部 8MHz 晶振，PLL x9 = 72MHz
     */
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

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;

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
