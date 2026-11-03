#ifndef APP_AP__AP_H
#define APP_AP__AP_H

#include <stdbool.h>

typedef enum {
    APP_INIT_STATE_NOT_STARTED = 0,
    APP_INIT_STATE_INITIALIZING,
    APP_INIT_STATE_READY,
    APP_INIT_STATE_FAULT
} app_init_state_t;

bool apInit(void);
app_init_state_t apGetInitState(void);

void apControlMain(void);
void apWifiMain(void);
void apOtaMain(void);

void StartControlTask(void *argument);
void StartWifiTask(void *argument);
void StartOtaTask(void *argument);

#endif // APP_AP__AP_H
