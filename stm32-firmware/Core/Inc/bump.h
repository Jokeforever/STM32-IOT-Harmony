#ifndef __BUMP_H
#define	__BUMP_H
#include "main.h"
#include "TS.h"
#include "oled.h"

#define BUMP_GPIO_PIN 				GPIO_PIN_1

#define BUMP_GPIO_PROT 				GPIOB

#define BUMP_ON 		 HAL_GPIO_WritePin(BUMP_GPIO_PROT,BUMP_GPIO_PIN,GPIO_PIN_SET)
#define BUMP_OFF 	 	HAL_GPIO_WritePin(BUMP_GPIO_PROT,BUMP_GPIO_PIN,GPIO_PIN_RESET)


#define HUMI_THRESHOLD_LOW  30  // 干旱阈值（%RH）
#define HUMI_THRESHOLD_HIGH 75  // 湿润阈值（%RH）
#define AREA                0.08f   // 种植面积（m2）
#define WATER_DEPTH         10.0f  // 目标补水深度（mm）
#define WATER_DENSITY       1.0f   // 水密度（g/mm·m2，简化为1L/m2·mm）
#define FLOW_RATE 1.2f  // 水泵流量（L/min）
#define MIN_WATER_VOLUME 0.1f  // 最小0.1L

typedef enum {
    Bump_OFF = 0,      // 关闭模式
    Bump_CALIBRATION = 1,   // 标准模式
    Bump_AUTO = 2, // 自动灌溉
} Bump_Mode;


void BUMP_Init(void);

void Irrigation_Control(void) ;

void Bump_Control(void);

void Bump_Display(void);

void Bump_Set(int mode);



#endif



