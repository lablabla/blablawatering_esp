
#include "WaterManager.h"

#include "Storage.h"
#include "Bluetooth.h"
#include "DS1307.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <time.h>


static const char* TAG = "WaterManager";

WaterManager::WaterManager() :
    m_backgroundTaskHandle(nullptr)
{
    printf("Initializing storage\n");
    m_storage = new Storage();

    printf("Initializing bluetooth\n");
    m_bluetooth = new Bluetooth("Blabla Watering System");
    m_bluetooth->start();

    
    printf("Updating stations and events\n");
    m_bluetooth->setStations(m_storage->getStations());
    m_bluetooth->setEvents(m_storage->getEvents());

    printf("Initializing RTC\n");
    m_rtc = new DS1307();
    m_rtc->begin();
    updateTimeFromRTC();
}

WaterManager::~WaterManager()
{
    delete m_storage;
    delete m_bluetooth;
    delete m_rtc;
}

void WaterManager::run()
{
    xTaskCreate(&WaterManager::runTask, "WaterManagerBackground", 8*1024, this, tskNO_AFFINITY, &m_backgroundTaskHandle);
}

void WaterManager::runTask(void* taskStartParameters)
{
    auto manager = (WaterManager*) taskStartParameters;
    manager->vTaskCode();
}

void WaterManager::vTaskCode()
{
    m_bluetooth->setBluetoothCallbacks(this);
    while(true)
    {
        checkEvents();
        {
            // Station& station = m_stations[0];
            // station.is_on = !station.is_on;
            // m_bluetooth->notifyStationStateChanged(station);
        }
        vTaskDelay( 500 / portTICK_PERIOD_MS );
    }
    
    vTaskDelete(NULL);
}

void WaterManager::updateTimeFromRTC()
{
    m_rtc->getTime();
    struct tm tm;
    tm.tm_hour = m_rtc->hour;
    tm.tm_min = m_rtc->minute;
    tm.tm_sec = m_rtc->second;
    tm.tm_mday = m_rtc->dayOfMonth;
    tm.tm_mon = m_rtc->month - 1; // DS1307 is 1 based, settimeofday needs 0 based
    tm.tm_year = m_rtc->year + 100; // DS1307 store the year as "since 2000", settimeofday needs as "since 1900" so 100 offset
    tm.tm_wday = m_rtc->dayOfWeek - 1; // same as month;

    time_t time_sec = mktime(&tm);
    struct timeval tv;
    tv.tv_sec = time_sec;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    printTimeFromRTC();
}

void WaterManager::printTimeFromRTC() const
{
    m_rtc->getTime();    
    printf("current RTC is set to %02d:%02d:%02d %02d/%02d/%04d\n", m_rtc->hour,
        m_rtc->minute, m_rtc->second, m_rtc->dayOfMonth, m_rtc->month, m_rtc->year+2000);
}

void WaterManager::onMessageReceived(MessageType messageType, void* message)
{    
    ESP_LOGI(TAG, "onMessageReceived with message type %d\n", messageType);
    switch (messageType)
    {
    case SET_TIME:
        setTimeMessage(*reinterpret_cast<SetTimeMessage*>(message));
        break;
    
    default:
        break;
    }
}

void WaterManager::setTimeMessage(const SetTimeMessage& timeMessage) const
{
    struct timeval now;
    ESP_LOGD(TAG, "In %s", __FUNCTION__);
    gettimeofday(&now, NULL);
    auto time = localtime(&now.tv_sec);
    ESP_LOGI(TAG, "current time is set to %02d:%02d:%02d.%03ld", time->tm_hour,
		   time->tm_min, time->tm_sec, now.tv_usec / 1000);

    struct timeval newTime;
    newTime.tv_sec = timeMessage.timeval.tv_sec;
    newTime.tv_usec = timeMessage.timeval.tv_usec;
    settimeofday(&newTime, nullptr);
    ESP_LOGI(TAG, "Setting TZ to %s", timeMessage.tz.c_str());
    setenv("TZ", timeMessage.tz.c_str(), 1);
    tzset();

    gettimeofday(&now, NULL);
    time = localtime(&now.tv_sec);
    ESP_LOGI(TAG, "new time is set to %02d:%02d:%02d.%03ld", time->tm_hour,
		   time->tm_min, time->tm_sec, now.tv_usec / 1000);

    m_rtc->fillByHMS(time->tm_hour, time->tm_min, time->tm_sec);
    m_rtc->fillByYMD(time->tm_year+1900, // gettimeofday returns year since 1900, DS1307 needs actual 4 digit year
                        time->tm_mon+1,
                        time->tm_mday);

    m_rtc->fillDayOfWeek(time->tm_wday+1);
    m_rtc->setTime();
    printTimeFromRTC();
}

void WaterManager::checkEvents()
{
    for (auto& eventsPair : m_storage->getEvents())
    {
        auto& event = eventsPair.second;
        if (event.start_time >= 0)
        {
            struct timeval now;                    
            gettimeofday(&now, NULL);

            time_t start_time = event.start_time;
            auto time = localtime(&start_time);
            printf("check events start time %02d:%02d:%02d\n", time->tm_hour,
                time->tm_min, time->tm_sec);
            if (now.tv_sec > start_time)
            {
                printf("Start time elapsed\n");
            }
        }
    }
}
