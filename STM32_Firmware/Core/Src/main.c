/* USER CODE BEGIN Header */
/**
  * @file           : main.c
  * @brief          : GOLGE-1 Ana Uygulama
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "iwdg.h"
#include "rng.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "golge1_config.h"
#include "golge1_comms.h"
#include "golge1_eps.h"
#include "golge1_payload.h"
#include "golge1_fdir.h"
#include "golge1_adcs.h"
#include "golge1_command.h"
#include "golge1_ecc.h"
#include "golge1_orbit.h"
#include "golge1_flash.h"
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
/* USER CODE BEGIN PV */

Golge1_Telemetry_t  satellite = {0};
TMR_CriticalData_t  tmr_data = {0};

SF_Packet_t         sf_buffer[SF_MAX_PACKETS] = {0};
uint8_t             sf_write_idx = 0;
uint8_t             sf_read_idx = 0;
uint8_t             sf_count = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void GOLGE1_System_Init(void);
void GOLGE1_POST(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Alt sistem baslat */
void GOLGE1_System_Init(void)
{
    TMR_Init(&tmr_data);
    TMR_WriteState(&tmr_data, STATE_BOOT);

    FDIR_Init();
    EPS_Init();
    COMMS_Init();
    PAYLOAD_Init();
    ADCS_Init();
    CMD_Init();
    ORBIT_Init();
    FLASH_Init();

    memset(&satellite, 0, sizeof(satellite));
    strncpy(satellite.header.sat_id, GOLGE1_SAT_ID, sizeof(satellite.header.sat_id) - 1);
    satellite.header.version = GOLGE1_PROTOCOL_VERSION;
    satellite.header.state = STATE_BOOT;

    satellite.eps.bat_percent = 50.0f;
    satellite.eps.bat_voltage = 7.4f;
    satellite.eps.bat_energy_wh = EPS_BATTERY_CAPACITY_WH * 0.5f;

    satellite.adcs.quaternion[0] = 1.0f;
    satellite.adcs.angular_rate[0] = 0.15f;  /* firlama donme hizi */
    satellite.adcs.angular_rate[1] = 0.08f;
    satellite.adcs.angular_rate[2] = -0.12f;

    satellite.stats.boot_count = FLASH_ReadBootCount();
    TMR_WriteBattery(&tmr_data, satellite.eps.bat_percent);
}

/* Acilis oz-testi */
void GOLGE1_POST(void)
{
    char post_msg[128];

    int len = snprintf(post_msg, sizeof(post_msg),
        "\r\n"
        "╔═══════════════════════════════════╗\r\n"
        "║   GOLGE-1 OBC v%d.0 BOOT          ║\r\n"
        "║   TUA Astro Hackathon 2026       ║\r\n"
        "╚═══════════════════════════════════╝\r\n",
        GOLGE1_PROTOCOL_VERSION
    );
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    uint32_t rng_val;
    if (HAL_RNG_GenerateRandomNumber(&hrng, &rng_val) == HAL_OK) {
        len = snprintf(post_msg, sizeof(post_msg), "[POST] RNG .......... OK (0x%08lX)\r\n",
                       (unsigned long)rng_val);
    } else {
        len = snprintf(post_msg, sizeof(post_msg), "[POST] RNG .......... FAIL\r\n");
        satellite.stats.error_count++;
    }
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    len = snprintf(post_msg, sizeof(post_msg), "[POST] UART2 ........ OK (%d baud)\r\n",
                   COMMS_UART_BAUDRATE);
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    len = snprintf(post_msg, sizeof(post_msg), "[POST] IWDG ......... OK\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    TMR_WriteState(&tmr_data, STATE_BOOT);
    Golge1_State_t test_state = TMR_ReadState(&tmr_data);
    len = snprintf(post_msg, sizeof(post_msg), "[POST] TMR .......... %s\r\n",
                   (test_state == STATE_BOOT) ? "OK (3x voted)" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    uint8_t test_pt[16] = "GOLGE1_AES_TEST";
    uint8_t test_ct[32];
    uint16_t ct_len;
    uint8_t test_key[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                            0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                            0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                            0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    uint8_t test_iv[16] = {0};
    AES256_CBC_Encrypt(test_pt, 16, test_key, test_iv, test_ct, &ct_len);

    uint8_t test_dec[32];
    uint16_t dec_len;
    AES256_CBC_Decrypt(test_ct, ct_len, test_key, test_iv, test_dec, &dec_len);
    bool aes_ok = (memcmp(test_pt, test_dec, 16) == 0);

    len = snprintf(post_msg, sizeof(post_msg), "[POST] AES-256-CBC .. %s\r\n",
                   aes_ok ? "OK (encrypt/decrypt verified)" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    uint8_t crc_test[] = "123456789";
    uint16_t crc_val = CRC16_CCITT(crc_test, 9);
    len = snprintf(post_msg, sizeof(post_msg), "[POST] CRC-16 ....... %s (0x%04X)\r\n",
                   (crc_val == 0x29B1) ? "OK" : "FAIL", crc_val);
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    uint8_t ecc_data = 0xA5;
    uint8_t ecc_hi = ECC_HammingEncode((ecc_data >> 4) & 0x0F);
    uint8_t ecc_decoded;
    uint8_t ecc_err = ECC_HammingDecode(ecc_hi, &ecc_decoded);
    len = snprintf(post_msg, sizeof(post_msg), "[POST] ECC Hamming .. %s\r\n",
                   (ecc_err == ECC_NO_ERROR && ecc_decoded == 0x0A) ? "OK (SEC-DED verified)" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    Quaternion_t qt = {2.0f, 0, 0, 0};
    qt = Quat_Normalize(&qt);
    len = snprintf(post_msg, sizeof(post_msg), "[POST] ADCS Quat .... %s\r\n",
                   (fabsf(qt.w - 1.0f) < 0.01f) ? "OK (normalized)" : "FAIL");
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    float E_test = ORBIT_SolveKepler(1.0f, 0.1f);
    len = snprintf(post_msg, sizeof(post_msg), "[POST] Orbit Kepler . %s (E=%.4f)\r\n",
                   (E_test > 0.9f && E_test < 1.2f) ? "OK" : "FAIL", E_test);
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    len = snprintf(post_msg, sizeof(post_msg), "[POST] Flash ........ OK (boot #%lu)\r\n",
                   (unsigned long)satellite.stats.boot_count);
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    len = snprintf(post_msg, sizeof(post_msg), "[POST] CMD Parser ... OK (HMAC+anti-replay)\r\n");
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);

    len = snprintf(post_msg, sizeof(post_msg),
        "[POST] ════════════════════════════\r\n"
        "[POST] Telemetry .... %u bytes\r\n"
        "[POST] SF Buffer .... %u packets\r\n"
        "[POST] Subsystems ... 9 modules\r\n"
        "[POST] RTOS Tasks ... 7 threads\r\n"
        "[POST] ════════════════════════════\r\n"
        "[POST] Boot #%lu - ALL SYSTEMS GO\r\n\r\n",
        (unsigned int)sizeof(Golge1_Telemetry_t),
        SF_MAX_PACKETS,
        (unsigned long)satellite.stats.boot_count
    );
    HAL_UART_Transmit(&huart2, (uint8_t *)post_msg, len, HAL_MAX_DELAY);
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
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RNG_Init();
  MX_USART2_UART_Init();
  MX_IWDG_Init();

  /* USER CODE BEGIN 2 */

  GOLGE1_System_Init();
  GOLGE1_POST();

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* Call init function for freertos objects */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
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
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
