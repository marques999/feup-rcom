#ifndef __ALARM_H_
#define __ALARM_H_

#include "shared.h"

extern int alarmFlag;
extern int alarmCounter;

void alarm_start();
void alarm_stop();

#endif /* __ALARM_H_ */
