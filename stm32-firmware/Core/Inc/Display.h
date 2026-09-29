#ifndef _DISPLAY_H_
#define _DISPLAY_H_


#include "main.h"
#include "oled.h"
#include "LED.h"
#include "bump.h"
#include "stdio.h"

typedef enum {
    DISPLAY_NONE = 0,   // 无显示
    DISPLAY_SCREEN1,    // 界面1：环境温湿度           CO2
    DISPLAY_SCREEN2,		// 界面2：	光照   	        台灯状态
		DISPLAY_SCREEN3,		// 界面3：	土壤湿度	 水量   水泵状态 
		DISPLAY_SCREEN4,    // 界面4： 参数1
		DISPLAY_SCREEN5		// 界面5： 参数2
} DisplayScreen;

typedef enum {
		NONE = 0,
		BRIGHT,
		DARK,
		DAMP,
		DROUGHT,
		HIGHCO2
} Abnormal;

void Display1_Init(void);
void Display2_Init(void);
void Display3_Init(void);
void Display4_Init(void);
void Display5_Init(void);
void Refresh1_Data(void);
void Refresh2_Data(void);
void Refresh3_Data(void);
void Refresh4_Data(void);
void Refresh5_Data(void);
void Display_Data(void);
#endif
