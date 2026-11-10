#ifndef _BEEP_H_
#define _BEEP_H_


#include "main.h"
#include "usart3.h"
#include "LED.h"
#include "bump.h"
#include "fan.h"
#include "jdq.h"
#include "mw_alarm_port.h"



#define BEEP_ON		AlarmPort_SetBuzzer(1U)

#define BEEP_OFF	AlarmPort_SetBuzzer(0U)



void Beep_Init(void);

void Beep_Set(_Bool status);

void IS_Normal(void);

void Handler(void);

#endif
