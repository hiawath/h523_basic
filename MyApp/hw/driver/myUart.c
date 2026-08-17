#include "myUart.h"
#include <string.h>

static uint8_t rx_data;
static uint8_t rx_buf[UART_RX_BUF_SIZE];

#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  /* Polling 방식으로 1바이트 전송 */
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

/* printf() 출력을 USART2로 리디렉션 */
int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

void uartInit(void)
{
  /* USART2 DMA 수신 대기 시작 */
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, UART_RX_BUF_SIZE);
}

/* UART 수신 콜백 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2)
  {
    if (rx_data == 'a')
      printf("Hello STM32 Cortex-M4 USART Polling!\r\n");
    else
      HAL_UART_Transmit(&huart2, rx_buf, UART_RX_BUF_SIZE, 100);

    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, UART_RX_BUF_SIZE);
  }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance == USART2)
  {
    if (rx_data == 'a')
      printf("Hello STM32 Cortex-M4 USART Polling!\r\n");
    else
      HAL_UART_Transmit(&huart2, rx_buf, Size, 100);

    HAL_UART_DMAStop(&huart2);
    memset(rx_buf, 0, Size);
  }
  HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, UART_RX_BUF_SIZE);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  HAL_UART_Receive_DMA(&huart2, rx_buf, UART_RX_BUF_SIZE);
}
