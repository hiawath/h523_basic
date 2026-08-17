#pragma once

#include "main.h"
#include "myGpio.h"
#include "myUart.h"
#include "myI2c.h"
#include "mySsd1306.h"
#include "myAdc.h"
#include "myDs1302.h"
#include "myDht11.h"
#include "myHcSr04.h"
#include "myLcd1602.h"

extern ds1302Handle_t hds1302;

void hwInit(void);
