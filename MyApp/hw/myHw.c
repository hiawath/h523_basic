#include "myHw.h"

/* DS1302 드라이버 핸들 */
ds1302Handle_t hds1302;

/* DHT11 드라이버 핸들 */
dht11Handle_t hdht11;

void hwInit(void)
{
  gpioInit();
  uartInit();
  i2cInit();
  lcd1602Init();
  ssd1306Init();
  adcInit();

  /* DS1302: CubeMX Label 매크로로 핀 정보 주입 */
  ds1302Pin_t ds1302_pins = {
    .rst_port = DS1302_RST_PIN_GPIO_Port,
    .rst_pin  = DS1302_RST_PIN_Pin,
    .dat_port = DS1_DAT_PIN_GPIO_Port,
    .dat_pin  = DS1_DAT_PIN_Pin,
    .clk_port = DS1302_CLK_PIN_GPIO_Port,
    .clk_pin  = DS1302_CLK_PIN_Pin,
  };
  ds1302Init(&hds1302, &ds1302_pins);

  /* DHT11: CubeMX Label 매크로로 핀 정보 주입 */
  dht11Pin_t dht11_pins = {
    .port = DH11_GPIO_Port,
    .pin  = DH11_Pin,
  };
  dht11Init(&hdht11, &dht11_pins);

  hcSr04Init();
}
