
#include "WaterManager.h"

#include <LiquidCrystal_I2C.h>
#include "Storage.h"
#include "Bluetooth.h"
#include "DS1307.h"
#include "CronManager.h"

#include "esp32-hal-log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <time.h>

WaterManager::WaterManager() :
    m_backgroundTaskHandle(nullptr)
{
    m_lcd = new LiquidCrystal_I2C(0x27, 20, 4);
    m_lcd->init();
    m_lcd->backlight();
    m_lcd->noBlink();
    m_lcd->setCursor(3, 1);
    m_lcd->print("Initializing...");

    log_i("Initializing storage\n");
    m_storage = new Storage();    
    for (const auto& station : m_storage->getStations())
    {
        // Set station's pin to out        
        pinMode(station.second.gpio_pin, OUTPUT);
        
        // Make sure station is off
        digitalWrite(station.second.gpio_pin, LOW);
    }

    log_i("Initializing bluetooth\n");
    m_bluetooth = new Bluetooth("Blabla Watering System");
    m_bluetooth->setBluetoothCallbacks(this);
    m_bluetooth->start();

    
    log_i("Updating stations and events\n");
    m_bluetooth->setStations(m_storage->getStations());
    m_bluetooth->setEvents(m_storage->getEvents());

    log_i("Initializing RTC\n");
    m_rtc = new DS1307();
    m_rtc->begin();
    updateTimeFromRTC();

    log_i("Initializing cron manager\n");
    m_cronManager = new CronManager();
    m_cronManager->setCronCallbacks(this);
    m_cronManager->begin();    
    for (const auto& e : m_storage->getEvents())
    {
        m_cronManager->addEvent(e.second);
    }

    log_i("Water Manager initialized\n");

}

WaterManager::~WaterManager()
{
    delete m_storage;
    delete m_bluetooth;
    delete m_rtc;
    delete m_cronManager;
}


void WaterManager::loop()
{
    m_cronManager->loop();
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
    log_i("current RTC is set to %02d:%02d:%02d %02d/%02d/%04d\n", m_rtc->hour,
        m_rtc->minute, m_rtc->second, m_rtc->dayOfMonth, m_rtc->month, m_rtc->year+2000);
}

void WaterManager::onMessageReceived(MessageType messageType, void* message)
{    
    log_i("onMessageReceived with message type %d\n", messageType);
    switch (messageType)
    {
    case SET_TIME:
        setTimeMessage(*reinterpret_cast<SetTimeMessage*>(message));
        break;
    
    default:
        break;
    }
}

void WaterManager::onEventStateChange(const Event& event, bool newState)
{
    //log_d("Setting event %d to state %s", event.id, (newState ? "ON" : "OFF"));

    auto& stations = m_storage->getStations();
    for (auto& station_id : event.stations_ids)
    {
        auto it = stations.find(station_id);
        if (it == stations.end())
        {
            log_w("Trying to set state for station id %d, station not found", station_id);
            continue;
        }
        auto newStateValue = newState ? HIGH : LOW;
        //log_d("Writing to station %d (pin %d) value %d", station_id, it->second.gpio_pin, newStateValue);
        digitalWrite(it->second.gpio_pin, newStateValue);
    }
}

void WaterManager::setTimeMessage(const SetTimeMessage& timeMessage) const
{
    struct timeval now;
    log_d("In");
    gettimeofday(&now, NULL);
    auto time = localtime(&now.tv_sec);
    log_i("current time is set to %02d:%02d:%02d.%03ld", time->tm_hour,
		   time->tm_min, time->tm_sec, now.tv_usec / 1000);

    struct timeval newTime;
    newTime.tv_sec = timeMessage.timeval.tv_sec;
    newTime.tv_usec = timeMessage.timeval.tv_usec;
    settimeofday(&newTime, nullptr);
    log_i("Setting TZ to %s", timeMessage.tz.c_str());
    setenv("TZ", timeMessage.tz.c_str(), 1);
    tzset();

    gettimeofday(&now, NULL);
    time = localtime(&now.tv_sec);
    log_i("new time is set to %02d:%02d:%02d.%03ld", time->tm_hour,
		   time->tm_min, time->tm_sec, now.tv_usec / 1000);

    m_rtc->fillByHMS(time->tm_hour, time->tm_min, time->tm_sec);
    m_rtc->fillByYMD(time->tm_year+1900, // gettimeofday returns year since 1900, DS1307 needs actual 4 digit year
                        time->tm_mon+1,
                        time->tm_mday);

    m_rtc->fillDayOfWeek(time->tm_wday+1);
    m_rtc->setTime();
    printTimeFromRTC();
}
