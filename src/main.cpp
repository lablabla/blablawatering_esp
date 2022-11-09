

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "WaterManager.h"

#include "DS1307.h"
#include "esp_log.h"

WaterManager* manager = nullptr;

void setup() 
{
    log_i("Starting Water Manager");
    manager = new WaterManager();
    log_i("Water Manager is now running..");
    fflush(stdout);
}

void loop() 
{
    manager->loop();
}
