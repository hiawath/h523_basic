#include "myI2c.h"

void i2cInit(void)
{
  // 필요한 I2C 초기화 작업 (CubeMX에서 MX_I2C1_Init 호출 후 추가 작업 필요 시)
}

/**
 * @brief  I2C 버스 복구 (Bus Recovery)
 * @note   I2C 슬레이브가 SDA를 LOW로 잡고 있는 경우(버스 Stuck 상태),
 *         SCL을 최대 9회 토글하여 슬레이브 트랜잭션을 강제로 완료시킨 뒤
 *         STOP 조건을 생성하고 I2C 페리페럴을 재초기화합니다.
 *         I2C1 핀: PB6(SCL), PB7(SDA)
 */
void i2cBusRecover(void)
{
  /* 1. I2C 페리페럴 비활성화 - GPIO를 직접 제어하기 위해 필요 */
  HAL_I2C_DeInit(&hi2c1);

  /* 2. PB6(SCL), PB7(SDA)를 Open-Drain 출력으로 전환 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

  GPIO_InitStruct.Pin = GPIO_PIN_6; /* SCL */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = GPIO_PIN_7; /* SDA */
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 3. SDA가 LOW이면 SCL을 최대 9회 토글하여 슬레이브 해방 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); /* SCL = H */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET); /* SDA = H */
  HAL_Delay(1);

  for (int i = 0; i < 9; i++)
  {
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
    {
      break; /* SDA가 HIGH이면 슬레이브가 해방됨 */
    }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); /* SCL = L */
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   /* SCL = H */
    HAL_Delay(1);
  }

  /* 4. STOP 조건 수동 생성: SDA L→H (SCL=H 상태에서) */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET); /* SDA = L */
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   /* SCL = H */
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);   /* SDA = H (STOP) */
  HAL_Delay(1);

  /* 5. I2C 페리페럴 재초기화 */
  MX_I2C1_Init();
}

void i2cScan(void)
{
  uint8_t count = 0;

  printf("\r\n==================================\r\n");
  printf("     STM32 I2C Bus Scanner        \r\n");
  printf("==================================\r\n");
  printf("Scanning I2C1 bus...\r\n\r\n");

  /* 7비트 유효 주소 범위: 0x01 ~ 0x77 (1 ~ 119) */
  for (uint8_t i = 1; i < 128; i++)
  {
    /* HAL은 8비트 주소 체계를 사용하므로 (i << 1) 전달 */
    HAL_StatusTypeDef result = HAL_I2C_IsDeviceReady(&hi2c1, (uint8_t)(i << 1), 1, 10);

    if (result == HAL_OK)
    {
      printf(" [*] Device Found at 7-bit Addr: 0x%02X (HAL 8-bit: 0x%02X)\r\n", i, (i << 1));
      count++;
    }
  }

  if (count == 0)
  {
    printf(" [!] No I2C devices found.\r\n");
    printf("     Check wiring, pull-up resistors, and power supply.\r\n");
  }
  else
  {
    printf("\r\nDone! Total %d device(s) found.\r\n", count);
  }
  printf("==================================\r\n\r\n");
}
