
#include "WaterManager.h"

#include "Storage.h"
#include "Bluetooth.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <NimBLEDevice.h>


static const char* TAG = "WaterManager";

WaterManager::WaterManager() :
    m_backgroundTaskHandle(nullptr)
{
    ESP_LOGI(TAG, "Initializing storage\n");
    m_storage = new Storage();

    ESP_LOGI(TAG, "Initializing bluetooth\n");
    m_bluetooth = new Bluetooth("Blabla Watering System");
    m_bluetooth->start();

    
    ESP_LOGI(TAG, "Updating stations and events\n");
    m_bluetooth->setStations(m_storage->getStations());
    m_bluetooth->setEvents(m_storage->getEvents());
}

WaterManager::~WaterManager()
{
    delete m_storage;
    delete m_bluetooth;
}

void WaterManager::run()
{
    xTaskCreate(&WaterManager::runTask, "WaterManagerBackground", 4096, this, tskNO_AFFINITY, &m_backgroundTaskHandle);
}

void WaterManager::runTask(void* taskStartParameters)
{
    auto manager = (WaterManager*) taskStartParameters;
    manager->vTaskCode();
}

void WaterManager::vTaskCode()
{
    const TickType_t xDelay = 1000 / portTICK_PERIOD_MS;
    uint64_t ticks = 0;
    for( ;; )
    {
        ticks++;

        if (ticks % 5 == 0)
        {
            // Station& station = m_stations[0];
            // station.is_on = !station.is_on;
            // m_bluetooth->notifyStationStateChanged(station);
        }
        vTaskDelay( xDelay );
    }
    
    vTaskDelete(NULL);
}

void WaterManager::onMessageReceived(void* message)
{

}
