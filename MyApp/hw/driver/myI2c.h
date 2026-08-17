#pragma once

#include "main.h"
#include "i2c.h"
#include <stdio.h>
#include <stdbool.h>

void i2cInit(void);
void i2cScan(void);
void i2cBusRecover(void); // I2C 버스 Stuck 복구 (SCL 클럭 9회 토글)
