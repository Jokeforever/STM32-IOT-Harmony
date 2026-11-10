/* 改前Key_Scan快照，仅主机对照；只清理行尾空白，未改语句。 */
#include "Key.h"

void Key_Scan(void)
{
	Keys[0].Pin_state = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_4) == RESET?1:0;//page
	Keys[1].Pin_state = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_5) == RESET?1:0;//page2
	Keys[2].Pin_state = HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_7) == RESET?1:0;//LED
	Keys[3].Pin_state = HAL_GPIO_ReadPin(GPIOA,GPIO_PIN_6) == RESET?1:0;//bump


	for(uint32_t i = 0; i<4;i++)
	{
		switch(Keys[i].State)
		{
			case 0:
				if(Keys[i].Pin_state)
				{
					Keys[i].Presstime++;
				}
				if(!Keys[i].Pin_state)
				{
					if(Keys[i].Presstime>3)
						Keys[i].State = 1;
					else
						Keys[i].Presstime = 0;
				}
				break;
			case 1:
				if(!Keys[i].Pin_state)
				{
					Keys[i].Releasetime++;
				}
				if(Keys[i].Releasetime>3)
				{
					Keys[i].State=2;
				}
				break;
			case 2:
				if(Keys[i].Presstime>150)
					Keys[i].LongClickFlag = 1;
				Keys[i].ClickFlag = 1;
				Keys[i].Presstime = 0;
				Keys[i].Releasetime = 0;
				break;
			default:
				Keys[i].State =0;
				break;
		}
	}
}
