

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "WaterManager.h"

extern "C"
{
void app_main() 
{
    printf("Starting Water Manager\n");
    auto manager = new WaterManager();
    manager->run();
    printf("Water Manager is now running..\n");
    fflush(stdout);
}
}