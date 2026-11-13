#include "fan.h"
#include "mw_climate_port.h"

void FAN_Init(void)
{
    ClimatePort_SetFan(0U);
}
