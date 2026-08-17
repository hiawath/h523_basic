#include "myDht11.h"

/* 마이크로초 딜레이 (STM32H523 250MHz 기준 소프트웨어 루프) */
static void delayUs(uint32_t us)
{
  volatile uint32_t count = us * 42;
  while (count--)
  {
    __NOP();
  }
}

static void dht11SetPinOutput(dht11Handle_t *hdht)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin   = hdht->pins.pin;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(hdht->pins.port, &GPIO_InitStruct);
}

static void dht11SetPinInput(dht11Handle_t *hdht)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin  = hdht->pins.pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(hdht->pins.port, &GPIO_InitStruct);
}

static void dht11RestorePin(dht11Handle_t *hdht)
{
  dht11SetPinOutput(hdht);
  HAL_GPIO_WritePin(hdht->pins.port, hdht->pins.pin, GPIO_PIN_SET);
}

void dht11Init(dht11Handle_t *hdht, const dht11Pin_t *pins)
{
  if (!hdht || !pins)
    return;

  hdht->pins               = *pins;
  hdht->latest_temperature = 0.0f;
  hdht->latest_humidity    = 0.0f;
  hdht->initialized        = false;

  dht11RestorePin(hdht);
  hdht->initialized = true;
}

/**
  * @brief  DHT11 센서로부터 온습도 데이터(40비트)를 읽어옴
  * @param  hdht: DHT11 핸들 포인터
  * @param  data: 수신된 온도/습도 데이터를 저장할 구조체 포인터
  * @retval true: 성공, false: 실패(타임아웃 또는 체크섬 에러)
  */
bool dht11Read(dht11Handle_t *hdht, dht11Data_t *data)
{
  if (!hdht || !hdht->initialized)
    return false;

  uint8_t raw_bytes[5] = {0};
  uint32_t timeout = 0;

  /* 1. MCU 시작 신호: 버스를 LOW로 최소 18ms 유지 */
  dht11SetPinOutput(hdht);
  HAL_GPIO_WritePin(hdht->pins.port, hdht->pins.pin, GPIO_PIN_RESET);
  HAL_Delay(18);

  /* 2. 버스를 HIGH로 30us 올린 후 입력 모드로 전환 */
  HAL_GPIO_WritePin(hdht->pins.port, hdht->pins.pin, GPIO_PIN_SET);
  delayUs(30);
  dht11SetPinInput(hdht);

  /* 타이밍 보호를 위해 임계 구역 진입 */
  __disable_irq();

  /* 3. DHT11 응답 대기: LOW 80us -> HIGH 80us */
  timeout = 1000;
  while (HAL_GPIO_ReadPin(hdht->pins.port, hdht->pins.pin) == GPIO_PIN_SET)
  {
    delayUs(1);
    if (--timeout == 0)
    {
      dht11RestorePin(hdht);
      __enable_irq();
      return false;
    }
  }

  timeout = 1000;
  while (HAL_GPIO_ReadPin(hdht->pins.port, hdht->pins.pin) == GPIO_PIN_RESET)
  {
    delayUs(1);
    if (--timeout == 0)
    {
      dht11RestorePin(hdht);
      __enable_irq();
      return false;
    }
  }

  timeout = 1000;
  while (HAL_GPIO_ReadPin(hdht->pins.port, hdht->pins.pin) == GPIO_PIN_SET)
  {
    delayUs(1);
    if (--timeout == 0)
    {
      dht11RestorePin(hdht);
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
      timeout = 1000;
      while (HAL_GPIO_ReadPin(hdht->pins.port, hdht->pins.pin) == GPIO_PIN_RESET)
      {
        delayUs(1);
        if (--timeout == 0)
        {
          dht11RestorePin(hdht);
          __enable_irq();
          return false;
        }
      }

      /* HIGH 지속 시간 측정 (26~28us: 0, 70us: 1) */
      uint32_t high_duration_us = 0;
      while (HAL_GPIO_ReadPin(hdht->pins.port, hdht->pins.pin) == GPIO_PIN_SET)
      {
        delayUs(1);
        high_duration_us++;
        if (high_duration_us > 150)
        {
          dht11RestorePin(hdht);
          __enable_irq();
          return false;
        }
      }

      if (high_duration_us > 40)
      {
        raw_bytes[i] |= (1 << j);
      }
    }
  }

  /* 버스 상태 즉시 복구 및 인터럽트 재활성화 */
  dht11RestorePin(hdht);
  __enable_irq();

  /* 5. 체크섬 검증 */
  uint8_t checksum = (uint8_t)(raw_bytes[0] + raw_bytes[1] + raw_bytes[2] + raw_bytes[3]);
  if (checksum != raw_bytes[4])
  {
    return false;
  }

  hdht->latest_humidity    = (float)raw_bytes[0] + ((float)raw_bytes[1] * 0.1f);
  hdht->latest_temperature = (float)raw_bytes[2] + ((float)raw_bytes[3] * 0.1f);

  if (data)
  {
    data->humidity    = hdht->latest_humidity;
    data->temperature = hdht->latest_temperature;
    data->is_valid    = true;
  }

  return true;
}

float dht11GetTemperature(dht11Handle_t *hdht)
{
  return hdht ? hdht->latest_temperature : 0.0f;
}

float dht11GetHumidity(dht11Handle_t *hdht)
{
  return hdht ? hdht->latest_humidity : 0.0f;
}
