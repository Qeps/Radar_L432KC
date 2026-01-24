/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    float amp_pp;
    uint8_t motion;
    float speed;
} motion_result_t;

typedef enum {
    MSG_RESULT,
    MSG_DEADLINE,
    MSG_WARN
} uart_msg_type_t;

typedef struct {
    uart_msg_type_t type;
    union {
        motion_result_t result;
        struct {
            uint32_t miss_cnt;
            uint32_t exec_ms;
        } deadline;
        struct {
            uint32_t code;
        } warn;
    } u;
} uart_msg_t;

typedef struct {
    uint16_t*   buf;
    uint32_t    N;
    float       fs;
    TickType_t  t_submit;
    TickType_t  deadline_ticks;
} dsp_job_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_BUF_LEN 256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static TaskHandle_t dispatch_task_handle = NULL;
static QueueHandle_t dsp_queue = NULL;
static QueueHandle_t uart_queue = NULL;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void task_dispatch(void* parameters);
static void task_dsp(void* parameters);
static void task_uart(void* parameters);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static uint16_t adc_buf[ADC_BUF_LEN];
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  BaseType_t status;
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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start(&htim1);
  /* Queue creation */
  uart_queue = xQueueCreate(8, sizeof(uart_msg_t));
  configASSERT(uart_queue != NULL);
  dsp_queue = xQueueCreate(4, sizeof(dsp_job_t));
  configASSERT(dsp_queue != NULL);
  /* Tasks creation */
  status = xTaskCreate(task_dispatch, "DISPATCH", 384, NULL, 3, &dispatch_task_handle);
  configASSERT(status == pdPASS);
  status = xTaskCreate(task_dsp, "DSP", 768, NULL, 2, NULL);
  configASSERT(status == pdPASS);
  status = xTaskCreate(task_uart, "UART", 256, NULL, 1, NULL);
  configASSERT(status == pdPASS);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  vTaskStartScheduler();
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hadc->Instance == ADC1)
    {
        xTaskNotifyFromISR(dispatch_task_handle, 0x01, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (hadc->Instance == ADC1)
    {
        xTaskNotifyFromISR(dispatch_task_handle, 0x02, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void task_dispatch(void* parameters)
{
    (void)parameters;

    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, ADC_BUF_LEN);

    const float fs = 1000.0f;              // Hz
    const uint32_t N = ADC_BUF_LEN / 2;    // pół-bufor

    // Deadline: czas od "bufor gotowy" do "wynik wysłany przez DSP".
    // Przy fs=1000 i N=128 okno ma 128 ms; zwykle deadline ~= okno.
    const TickType_t deadline_ticks = pdMS_TO_TICKS((uint32_t)((1000.0f * (float)N) / fs));

    uint32_t notify = 0;

    while (1)
    {
        xTaskNotifyWait(0, 0xFFFFFFFF, &notify, portMAX_DELAY);

        uint16_t* buf = NULL;
        if (notify & 0x01)      buf = &adc_buf[0];
        else if (notify & 0x02) buf = &adc_buf[N];
        else                    continue;

        dsp_job_t job;
        job.buf = buf;
        job.N   = N;
        job.fs  = fs;
        job.t_submit = xTaskGetTickCount();
        job.deadline_ticks = deadline_ticks;

        if (xQueueSend(dsp_queue, &job, 0) != pdPASS)
        {
            uart_msg_t w;
            w.type = MSG_WARN;
            w.u.warn.code = 1; // 1 = DSP queue overflow
            (void)xQueueSend(uart_queue, &w, 0);
        }
    }
}

static void task_dsp(void* parameters)
{
    (void)parameters;

    const float fc = 10.525e9f;
    const float c  = 299792458.0f;

    static uint32_t deadline_miss_cnt = 0;

    while (1)
    {
        dsp_job_t job;
        xQueueReceive(dsp_queue, &job, portMAX_DELAY);

        TickType_t t0 = xTaskGetTickCount();

        uint16_t* buf = job.buf;
        uint32_t N = job.N;
        float fs = job.fs;

        // DC removal
        float mean = 0.0f;
        for (uint32_t i = 0; i < N; i++) mean += buf[i];
        mean /= (float)N;

        // peak-to-peak amplituda
        float minv =  1e9f;
        float maxv = -1e9f;

        for (uint32_t i = 0; i < N; i++)
        {
            float x = (float)buf[i] - mean;
            if (x < minv) minv = x;
            if (x > maxv) maxv = x;
        }
        float amp_pp = maxv - minv;

        // detekcja ruchu
        uint8_t motion = (amp_pp > 300.0f);

        float speed = 0.0f;
        if (motion)
        {
            uint32_t crossings = 0;
            for (uint32_t i = 1; i < N; i++)
            {
                float x0 = (float)buf[i-1] - mean;
                float x1 = (float)buf[i]   - mean;
                if (x0 < 0.0f && x1 >= 0.0f) crossings++;
            }

            float fd = ((float)crossings * fs) / (float)N;
            speed = (c * fd) / (2.0f * fc);
        }

        // Wynik -> UART
        uart_msg_t m;
        m.type = MSG_RESULT;
        m.u.result.amp_pp = amp_pp;
        m.u.result.motion = motion;
        m.u.result.speed  = speed;
        (void)xQueueSend(uart_queue, &m, 0);

        // Deadline check: latencja od submit do "po obliczeniach"
        TickType_t t1 = xTaskGetTickCount();
        TickType_t latency = t1 - job.t_submit;

        if (latency > job.deadline_ticks)
        {
            deadline_miss_cnt++;
            uart_msg_t d;
            d.type = MSG_DEADLINE;
            d.u.deadline.miss_cnt = deadline_miss_cnt;
            d.u.deadline.exec_ms  = (uint32_t)(latency * portTICK_PERIOD_MS);
            (void)xQueueSend(uart_queue, &d, 0);
        }

        // Dodatkowo możesz mierzyć czysty czas DSP: (t1 - t0)
        (void)t0;
    }
}

static void task_uart(void* parameters)
{
    (void)parameters;

    uart_msg_t m;
    char msg[128];

    while (1)
    {
        xQueueReceive(uart_queue, &m, portMAX_DELAY);

        if (m.type == MSG_RESULT)
        {
            snprintf(msg, sizeof(msg),
                     "amp_pp=%.1f motion=%d speed=%.2f m/s\r\n",
                     m.u.result.amp_pp, m.u.result.motion, m.u.result.speed);
        }
        else if (m.type == MSG_DEADLINE)
        {
            snprintf(msg, sizeof(msg),
                     "DEADLINE MISS: cnt=%lu latency=%lu ms\r\n",
                     (unsigned long)m.u.deadline.miss_cnt,
                     (unsigned long)m.u.deadline.exec_ms);
        }
        else // MSG_WARN
        {
            snprintf(msg, sizeof(msg),
                     "WARN: code=%lu\r\n",
                     (unsigned long)m.u.warn.code);
        }

        HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);
    }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
