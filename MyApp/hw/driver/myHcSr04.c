#include "myHcSr04.h"

static float latest_distance = 0.0f;

/* 마이크로초 딜레이 (STM32H523 250MHz 기준 소프트웨어 루프) */
static void delayUs(uint32_t us)
{
  volatile uint32_t count = us * 42;
  while (count--)
  {
    __NOP();
  }
}

void hcSr04Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* Trig Pin (PA8): Output Push-Pull */
  GPIO_InitStruct.Pin   = HCSR04_TRIG_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(HCSR04_TRIG_PORT, &GPIO_InitStruct);

  /* Echo Pin (PB10): Input with Pull-Down */
  GPIO_InitStruct.Pin   = HCSR04_ECHO_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
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

  /* 1. Trig 핀에 10us HIGH 펄스 인가 */
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
  delayUs(2);
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
  delayUs(10);
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

  /* 2. Echo 핀이 HIGH가 될 때까지 대기 (최대 10ms 타임아웃) */
  timeout = 5000;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_RESET)
  {
    delayUs(2);
    if (--timeout == 0)
    {
      if (distance_cm) *distance_cm = latest_distance;
      return false;
    }
  }

  /* 3. Echo 핀이 LOW가 될 때까지 HIGH 펄스 지속시간 측정 (최대 25ms ≈ 430cm) */
  /* 루프 1회당 약 2us(delayUs(1) + GPIO 읽기 오버헤드) */
  uint32_t count = 0;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
  {
    delayUs(1);
    count++;
    if (count > 25000)
    {
      if (distance_cm) *distance_cm = latest_distance;
      return false;
    }
  }

  /* 4. 시간(us) 및 거리(cm) 환산 (루프 1회 = 약 2.0us 보정) */
  float duration_us = (float)count * 2.0f;
  float dist = duration_us / 58.0f;

  /* 유효 거리 범위 체크 (2cm ~ 400cm) */
  if (dist >= 2.0f && dist <= 400.0f)
  {
    latest_distance = dist;
  }

  if (distance_cm)
  {
    *distance_cm = latest_distance;
  }

  return (dist >= 2.0f && dist <= 400.0f);
}

float hcSr04GetDistance(void)
{
  return latest_distance;
}
