#include "myGpio.h"

void gpioInit(void)
{
  // 필요한 GPIO 초기화가 있을 경우 여기에 작성
}

/**
  * @brief  EXTI 인터럽트 콜백 함수
  * @param  GPIO_Pin: 인터럽트가 발생한 핀
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_13)
  {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
  }
}
