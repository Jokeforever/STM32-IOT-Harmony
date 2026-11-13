#ifndef __BUMP_H
#define	__BUMP_H
#include "main.h"
#include "TS.h"
#include "oled.h"


#define HUMI_THRESHOLD_LOW  30  // 干旱阈值（%RH）
#define HUMI_THRESHOLD_HIGH 75  // 湿润阈值（%RH）

typedef enum {
    Bump_OFF = 0,      // 关闭模式
    Bump_CALIBRATION = 1,   // 标准模式
    Bump_AUTO = 2, // 自动灌溉
} Bump_Mode;


void BUMP_Init(void);

void Irrigation_Control(void) ;

void Bump_Control(void);
/* TIM1 每 10 ms 在按键扫描后调用一次。 */
void Bump_OnTimerTick(void);

void Bump_Display(void);

void Bump_Set(int mode);



#endif



