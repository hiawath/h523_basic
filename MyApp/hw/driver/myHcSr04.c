#include "myHcSr04.h"

static float latest_distance = 0.0f;
static uint8_t fail_count = 0;

/* 마이크로초 딜레이 (STM32H523 250MHz 전용 정밀 NOP 루프) */
static inline void delayUs(uint32_t us)
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
  * @retval true: 성공, false: 측정 실패(범위 초과/타임아웃)
  */
bool hcSr04Read(float *distance_cm)
{
  /* 0. 잔류 Echo 신호가 있으면 LOW가 될 때까지 대기 (최대 3ms) */
  uint32_t wait_low = 1500;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
  {
    delayUs(2);
    if (--wait_low == 0)
    {
      if (++fail_count > 4 && distance_cm) *distance_cm = 0.0f;
      return false;
    }
  }

  /* 1. Trig 핀에 10us HIGH 펄스 인가 */
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);
  delayUs(2);
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_SET);
  delayUs(10);
  HAL_GPIO_WritePin(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_PIN_RESET);

  /* 2. Echo 핀이 HIGH가 될 때까지 대기 (최대 6ms 타임아웃) */
  uint32_t timeout = 3000;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_RESET)
  {
    delayUs(2);
    if (--timeout == 0)
    {
      if (++fail_count > 4 && distance_cm) *distance_cm = 0.0f;
      return false;
    }
  }

  /* 3. Echo 핀 HIGH 지속 시간 카운팅 (루프 1회 = 약 1.0us 정밀 보정, 최대 25ms ≈ 430cm) */
  uint32_t duration_count = 0;
  while (HAL_GPIO_ReadPin(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == GPIO_PIN_SET)
  {
    /* 1회 루프당 약 1.0us가 되도록 튜닝된 딜레이 */
    volatile uint32_t c = 34;
    while (c--) { __NOP(); }
    duration_count++;
    if (duration_count > 25000)
    {
      if (++fail_count > 4 && distance_cm) *distance_cm = 0.0f;
      return false;
    }
  }

  /* 4. 거리(cm) 환산 (음속 340m/s: 시간(us) / 58.0) */
  float dist = (float)duration_count / 58.0f;

  /* 유효 거리 범위 체크 (2cm ~ 400cm) */
  if (dist >= 2.0f && dist <= 400.0f)
  {
    fail_count = 0;
    latest_distance = dist;
    if (distance_cm)
    {
      *distance_cm = dist;
    }
    return true;
  }

  if (++fail_count > 4 && distance_cm)
  {
    *distance_cm = 0.0f;
  }
  return false;
}

float hcSr04GetDistance(void)
{
  return latest_distance;
}
