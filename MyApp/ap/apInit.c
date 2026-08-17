#include "apInit.h"
#include "apMain.h"
#include "myHw.h"

void apInit(void)
{
  /* 하드웨어 레이어 초기화 (GPIO, UART, I2C, SSD1306) */
  hwInit();

  /* 보드 부팅 및 I2C 버스 안정화를 위해 100ms 대기 */
  HAL_Delay(100);

  /* I2C1 버스 스캔 실행 */
  i2cScan();

  /* 스캔 후 I2C 버스 정리 및 SSD1306 초기화 확인 */
  i2cBusRecover();
  HAL_Delay(10);
  ssd1306Init();
}