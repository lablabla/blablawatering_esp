

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "WaterManager.h"

#include "DS1307.h"
#include "esp_log.h"


void setup() 
{
    struct timeval now;
    gettimeofday(&now, NULL);
    auto time = localtime(&now.tv_sec);
    printf("Initial time is set to %02d:%02d:%02d.%03ld\n", time->tm_hour,
		   time->tm_min, time->tm_sec, now.tv_usec / 1000);


    printf("Starting Water Manager\n");
    auto manager = new WaterManager();
    manager->run();
    printf("Water Manager is now running..\n");
    fflush(stdout);
}

void loop() {}
