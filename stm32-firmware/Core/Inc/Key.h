#ifndef _KEY_H_
#define _KEY_H_


#include "main.h"
#include "LED.h"
#include "Display.h"
#include "bump.h"
#include "fan.h"

#define LDR_STEP 100
#define CO2_STEP 100
#define TEMP_STEP 2
#define BUMP_STEP 2

void Key_Func(void);

typedef struct Key
{
	uint8_t State;
	uint8_t Pin_state;
	
	uint8_t ClickFlag;
	uint8_t LongClickFlag;
	
	uint32_t Presstime;
	uint32_t Releasetime;
}Key;

#endif
