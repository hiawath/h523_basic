#include "myDht11.h"

static float latest_temperature = 0.0f;
static float latest_humidity = 0.0f;

/* DWT Cycle Counter 기반 마이크로초 딜레이 */
static void dwtInit(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delayUs(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000);
  while ((DWT->CYCCNT - start) < ticks);
}

static void dht11SetPinOutput(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = DHT11_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void dht11SetPinInput(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = DHT11_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

void dht11Init(void)
{
  __HAL_RCC_GPIOC_CLK_ENABLE();
  dwtInit();

  dht11SetPinOutput();
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
}

/**
  * @brief  DHT11 센서로부터 온습도 데이터(40비트)를 읽어옴
  * @param  data: 수신된 온도/습도 데이터를 저장할 구조체 포인터
  * @retval true: 성공, false: 실패(타임아웃 또는 체크섬 에러)
  */
bool dht11Read(dht11Data_t *data)
{
  uint8_t raw_bytes[5] = {0};
  uint32_t timeout = 0;

  /* 1. MCU 시작 신호: 버스를 LOW로 최소 18ms 유지 */
  dht11SetPinOutput();
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
  HAL_Delay(18);

  /* 2. 버스를 HIGH로 20~40us 올린 후 입력 모드로 전환 */
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
  delayUs(30);
  dht11SetPinInput();

  /* 타이밍 보호를 위해 임계 구역 진입 */
  __disable_irq();

  /* 3. DHT11 응답 대기: LOW 80us -> HIGH 80us */
  timeout = 10000;
  while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
  {
    if (--timeout == 0)
    {
      __enable_irq();
      return false;
    }
  }

  timeout = 10000;
  while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
  {
    if (--timeout == 0)
    {
      __enable_irq();
      return false;
    }
  }

  timeout = 10000;
  while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
  {
    if (--timeout == 0)
    {
      __enable_irq();
      return false;
    }
  }

  /* 4. 40비트 데이터 수신 (5 바이트) */
  for (uint8_t i = 0; i < 5; i++)
  {
    for (int8_t j = 7; j >= 0; j--)
    {
      /* 각 비트 시작: 50us LOW 대기 */
      timeout = 10000;
      while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_RESET)
      {
        if (--timeout == 0)
        {
          __enable_irq();
          return false;
        }
      }

      /* HIGH 지속 시간 측정 (26~28us: 0, 70us: 1) */
      uint32_t t_start = DWT->CYCCNT;

      timeout = 10000;
      while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN) == GPIO_PIN_SET)
      {
        if (--timeout == 0)
        {
          __enable_irq();
          return false;
        }
      }

      uint32_t elapsed_us = (DWT->CYCCNT - t_start) / (SystemCoreClock / 1000000);

      if (elapsed_us > 40)
      {
        raw_bytes[i] |= (1 << j);
      }
    }
  }

  __enable_irq();

  /* 버스 상태 복구 */
  dht11SetPinOutput();
  HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);

  /* 5. 체크섬 검증 */
  uint8_t checksum = raw_bytes[0] + raw_bytes[1] + raw_bytes[2] + raw_bytes[3];
  if (checksum != raw_bytes[4])
  {
    return false;
  }

  latest_humidity = (float)raw_bytes[0] + ((float)raw_bytes[1] * 0.1f);
  latest_temperature = (float)raw_bytes[2] + ((float)raw_bytes[3] * 0.1f);

  if (data)
  {
    data->humidity = latest_humidity;
    data->temperature = latest_temperature;
    data->is_valid = true;
  }

  return true;
}

float dht11GetTemperature(void)
{
  return latest_temperature;
}

float dht11GetHumidity(void)
{
  return latest_humidity;
}
