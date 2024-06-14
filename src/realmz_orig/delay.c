#include "variables.h"
#include "prototypes.h"
/************** delay ***********************/
void delay(short timedelay)
{
long oldtick;

if (!timedelay) timedelay = delayspeed;
oldtick = TickCount();

for(;;) if (TickCount() - oldtick > timedelay) return;
}

