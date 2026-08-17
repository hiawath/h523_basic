#include "myHcSr04.h"

static float latest_distance = 0.0f;

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

void hcSr04Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  dwtInit();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Trig Pin (PA8): Output Push-Pull */
  GPIO_InitStruct.Pin = HCSR04_TRIG_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(HCSR04_TRIG_PORT, &GPIO_InitStruct);

  /* Echo Pin (PB10): Input with Pull-Down */
  GPIO_InitStruct.Pin = HCSR04_ECHO_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(HCSR04_ECHO_PORT, &GPIO_InitStruct);

  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  HC-SR04 초음파 센서로 거리를 측정 (단위: cm)
  * @param  distance_cm: 측정된 거리 값을 저장할 포인터
  * @retval true: 성공, false: 타임아웃 또는 측정 범위 초과
  */
bool hcSr04Read(float *distance_cm)
{
  uint32_t timeout = 0;
  uint32_t start_tick = 0;
  uint32_t stop_tick = 0;

  /* 1. Trig 핀에 10us HIGH 펄스 인가 */
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
  delayUs(2);
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
  delayUs(10);
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

  /* 2. Echo 핀이 HIGH가 될 때까지 대기 (최대 5ms 타임아웃) */
  timeout = 50000;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_RESET)
  {
    if (--timeout == 0)
    {
      return false;
    }
  }

  /* 3. Echo HIGH 시작 시점 기록 */
  start_tick = DWT->CYCCNT;

  /* 4. Echo 핀이 LOW가 될 때까지 대기 (최대 약 30ms = 500cm 범위 타임아웃) */
  timeout = 300000;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
  {
    if (--timeout == 0)
    {
      return false;
    }
  }
  stop_tick = DWT->CYCCNT;

  /* 5. 펄스 지속 시간(us) 계산 및 거리(cm) 환산 (음속 340m/s: 시간(us) / 58.0) */
  uint32_t elapsed_ticks = stop_tick - start_tick;
  float duration_us = (float)elapsed_ticks / (float)(SystemCoreClock / 1000000);
  float dist = duration_us / 58.0f;

  /* 유효 거리 범위 체크 (2cm ~ 400cm) */
  if (dist < 2.0f || dist > 400.0f)
  {
    return false;
  }

  latest_distance = dist;
  if (distance_cm)
  {
    *distance_cm = dist;
  }

  return true;
}

float hcSr04GetDistance(void)
{
  return latest_distance;
}
