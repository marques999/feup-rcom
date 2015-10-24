#include "alarm.h"
#include <signal.h>

int alarmFlag = FALSE;
int alarmCounter = 0;
int alarmTimeout = 3;

void alarm_handler() {
	alarmFlag = TRUE;
	alarmCounter++;
	alarm(alarmTimeout);
}

void alarm_set(int timeout) {
	alarmTimeout = timeout;
}

void alarm_start(void) {

	struct sigaction action;

	action.sa_handler = alarm_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	alarmCounter = 0;
	alarmFlag = FALSE;
	alarm(alarmTimeout);
}

void alarm_stop(void) {

	struct sigaction action;

	action.sa_handler = NULL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	alarmCounter = 0;
	alarm(0);
}