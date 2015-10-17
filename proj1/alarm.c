#include "alarm.h"

int alarmFlag = FALSE;
int alarmCounter = 0;

static void alarm_handler() {
	alarmFlag = TRUE;
	alarmCounter++;
	alarm(3);
}

void alarm_start() {

	struct sigaction action;

	action.sa_handler = alarm_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	alarmCounter = 0;
	alarmFlag = FALSE;
	alarm(3);
}

void alarm_stop() {

	struct sigaction action;

	action.sa_handler = NULL;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	sigaction(SIGALRM, &action, NULL);
	alarmCounter = 0;
	alarm(0);
}
