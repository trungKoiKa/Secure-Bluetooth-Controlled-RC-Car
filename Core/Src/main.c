/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Car.h"
#include "Keypad.h"
#include "Lcd.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ---------------Xu ly nhap mat khau----------

typedef enum
{
	NORMAL_STATE,
	LOCK_STATE,
	ENTER_PASS_STATE,
	LOCK_30s_STATE,
}KeypadState;

KeypadState keypad_state = LOCK_STATE;

#define PASSWORD_LEN 8
#define MAX_PASS_ERR 3
#define LOCK_TIME_MS 30000
uint8_t pass[PASSWORD_LEN] = "01234567";
uint8_t count_err;
uint32_t timer_start_err;

typedef struct
{
	uint8_t buff[PASSWORD_LEN + 1]; // +1 de luon co '\0'
	uint8_t index;
}Password_Typedef;

Password_Typedef password;

// ---------------Trang thai xe + failsafe----------
// Mat lenh di chuyen qua CMD_TIMEOUT_MS thi xe tu dung (dat 0 de tat failsafe)
#define CMD_TIMEOUT_MS 500

Car_State car_state = CAR_STOP_STATE;
uint8_t car_speed = 100;
uint32_t last_cmd_tick;

static void car_move(Car_State state, uint8_t speed)
{
	car_control(state, speed);
	car_state = state;
	last_cmd_tick = HAL_GetTick();
}

static void car_halt(void)
{
	car_control(CAR_STOP_STATE, 0);
	car_state = CAR_STOP_STATE;
}

static void password_reset(void)
{
	memset(&password, 0, sizeof(password));
}

void failsafe_handle(void)
{
#if CMD_TIMEOUT_MS > 0
	if(car_state != CAR_STOP_STATE && HAL_GetTick() - last_cmd_tick > CMD_TIMEOUT_MS)
	{
		car_halt();
	}
#endif
}

void KeypadPressingCallback(uint8_t key)
{
	switch(keypad_state)
	{
		case ENTER_PASS_STATE:
		{
			if(key >= '0' && key <= '9')
			{
				if(password.index < PASSWORD_LEN)
				{
					password.buff[password.index++] = key;
				}
			}
			else if(key == 'D')
			{
				//check pass: phai du PASSWORD_LEN ky tu
				if(password.index == PASSWORD_LEN
					&& memcmp(password.buff, pass, PASSWORD_LEN) == 0)
				{
					//mat khau dung
					count_err = 0;
					password_reset();
					keypad_state = NORMAL_STATE;
				}
				else
				{
					count_err++;
					password_reset();
					if(count_err >= MAX_PASS_ERR)
					{
						timer_start_err = HAL_GetTick();
						keypad_state = LOCK_30s_STATE;
					}
				}
			}
			break;
		}
		default:
			break;
	}
}

void KeypadPressingTimeOutCallback(uint8_t key)
{
	if(key != 'D')
	{
		return;
	}
	if(keypad_state == LOCK_STATE)
	{
		password_reset();
		keypad_state = ENTER_PASS_STATE;
	}
	else if(keypad_state == NORMAL_STATE)
	{
		// giu D khi dang dieu khien: khoa lai va dung xe
		car_halt();
		keypad_state = LOCK_STATE;
	}
}

void handle_state_keyboard()
{
	switch(keypad_state)
	{
		case LOCK_30s_STATE:
		{
			if(HAL_GetTick() - timer_start_err >= LOCK_TIME_MS)
			{
				count_err = 0;
				keypad_state = LOCK_STATE;
			}
			break;
		}
		default:
			break;
	}
}


//---------------Xu ly UART---------------------
// Ring buffer: ngat UART ghi vao head, vong lap chinh doc tu tail
#define RX_BUF_SIZE 32
uint8_t data_rx;
static volatile uint8_t rx_buf[RX_BUF_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == huart1.Instance)
	{
		uint8_t next = (rx_head + 1) % RX_BUF_SIZE;
		if(next != rx_tail) // day thi bo byte moi
		{
			rx_buf[rx_head] = data_rx;
			rx_head = next;
		}
		HAL_UART_Receive_IT(&huart1, &data_rx, 1);
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == huart1.Instance)
	{
		// loi overrun/framing/noise: xoa co va nhan lai, tranh treo UART
		__HAL_UART_CLEAR_OREFLAG(&huart1);
		HAL_UART_Receive_IT(&huart1, &data_rx, 1);
	}
}

static void uart_process(uint8_t cmd)
{
	// Lenh dung luon co hieu luc o moi trang thai
	if(cmd == 'S')
	{
		car_halt();
		return;
	}
	if(keypad_state != NORMAL_STATE)
	{
		return;
	}
	switch(cmd)
	{
		case 'F':
			car_move(CAR_FORWARD_STATE, car_speed);
			break;
		case 'B':
			car_move(CAR_BACKWARD_STATE, car_speed);
			break;
		case 'L':
			car_move(CAR_TURNLEFT_STATE, car_speed);
			break;
		case 'R':
			car_move(CAR_TURNRIGHT_STATE, car_speed);
			break;
		default:
			if(cmd >= '0' && cmd <= '9')
			{
				//0 -> 90
				car_speed = (cmd - '0') * 10;
			}
			else if(cmd == 'q')
			{
				car_speed = 100;
			}
			break;
	}
}

void uart_handle()
{
	while(rx_tail != rx_head)
	{
		uint8_t cmd = rx_buf[rx_tail];
		rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
		uart_process(cmd);
	}
}
//---------------Hien thi LCD---------------------
#define LCD_ADDR (0x27 << 1) // PCF8574A: 0x3F << 1
#define LCD_REFRESH_MS 100

static char lcd_last[LCD_ROWS][LCD_COLS + 1];

static void lcd_show(uint8_t row, const char *s)
{
	if(strcmp(lcd_last[row], s) != 0) // chi ve lai khi noi dung doi
	{
		lcd_print_line(row, s);
		strncpy(lcd_last[row], s, LCD_COLS);
		lcd_last[row][LCD_COLS] = 0;
	}
}

// ghi so 0..999 vao dst, tra ve con tro sau so
static char *put_num(char *dst, uint16_t v)
{
	if(v >= 100) *dst++ = '0' + v / 100;
	if(v >= 10) *dst++ = '0' + (v / 10) % 10;
	*dst++ = '0' + v % 10;
	return dst;
}

void lcd_update(void)
{
	static uint32_t t_last;
	if(HAL_GetTick() - t_last < LCD_REFRESH_MS)
	{
		return;
	}
	t_last = HAL_GetTick();

	char l1[LCD_COLS + 1] = "";
	char l2[LCD_COLS + 1] = "";
	switch(keypad_state)
	{
		case LOCK_STATE:
			strcpy(l1, "LOCKED");
			strcpy(l2, "Hold D 3s unlock");
			break;
		case ENTER_PASS_STATE:
			strcpy(l1, "ENTER PASSWORD");
			memset(l2, '*', password.index);
			l2[password.index] = 0;
			break;
		case LOCK_30s_STATE:
		{
			uint32_t left = LOCK_TIME_MS - (HAL_GetTick() - timer_start_err);
			if(left > LOCK_TIME_MS) left = 0; // tranh tran so khi het gio
			strcpy(l1, "WRONG! LOCKED");
			char *p = l2;
			strcpy(p, "Wait: "); p += 6;
			p = put_num(p, (left + 999) / 1000);
			*p++ = 's'; *p = 0;
			break;
		}
		case NORMAL_STATE:
		{
			static const char *dir_name[] = {"STOP", "FWD", "BACK", "LEFT", "RIGHT"};
			strcpy(l1, "READY");
			char *p = l2;
			strcpy(p, dir_name[car_state]); p += strlen(dir_name[car_state]);
			strcpy(p, " SPD:"); p += 5;
			p = put_num(p, car_speed);
			*p++ = '%'; *p = 0;
			break;
		}
	}
	lcd_show(0, l1);
	lcd_show(1, l2);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
	
	car_init(&htim1);
	HAL_UART_Receive_IT(&huart1, &data_rx, 1);
	Keypad_Init();
	lcd_init(&hi2c1, LCD_ADDR);
//	control_motor1(MOTOR_CW, 50);
//	control_motor2(MOTOR_CCW, 50);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		uart_handle();
		Keypad_Handle();
		handle_state_keyboard();
		failsafe_handle();
		lcd_update();
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, MOTOR1_IO_Pin|MOTOR2_IO_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA1 PA2 PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA5 PA6 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : MOTOR1_IO_Pin MOTOR2_IO_Pin */
  GPIO_InitStruct.Pin = MOTOR1_IO_Pin|MOTOR2_IO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
