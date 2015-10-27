#ifndef __ALARM_H_
#define __ALARM_H_

#include "shared.h"

extern int alarmCounter;
extern int alarmFlag;
extern int alarmTimeout;

void alarm_start(void);
void alarm_stop(void);

#endif /* __ALARM_H_ */
