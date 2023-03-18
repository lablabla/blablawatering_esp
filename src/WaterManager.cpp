
#include "WaterManager.h"


#include "Storage.h"
#include "Bluetooth.h"
#include "DS1307.h"
#include "CronManager.h"
#include "TFT_eSPI.h" 
#define TEXT "aA MWyz~12" // Text that will be printed on screen in any font

#include "esp32-hal-log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <time.h>

WaterManager::WaterManager() :
    m_backgroundTaskHandle(nullptr)
{
    m_lcd = new TFT_eSPI();
  m_lcd->init();
  m_lcd->setRotation(1);
  m_lcd->fillScreen(TFT_BLUE);
  m_lcd->setTextSize(2);
  m_lcd->setTextColor(TFT_WHITE);
  m_lcd->setCursor(0, 0);
  
  m_lcd->setTextFont(1);                 // Select the font
  m_lcd->drawString("Meow!", 160, 60);// Print the string name of the font
  m_lcd->drawString(TEXT, 160, 120);// Print the string name of the font

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
    m_bluetooth = new Bluetooth("Blabla Watering System", m_storage);
    m_bluetooth->setBluetoothCallbacks(this);
    m_bluetooth->start();

    

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
    delay(1000);
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
    struct timeval now;
    gettimeofday(&now, NULL);
    auto time = localtime(&now.tv_sec);
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
    case SET_STATION_STATE:
        setStationStateMessage(*reinterpret_cast<SetStationStateMessage*>(message));
        break;
    default:
        break;
    }
}

void WaterManager::onEventStateChange(const Event& event, bool newState)
{
    for (auto& station_id : event.stations_ids)
    {
        Station* station = m_storage->getStation(station_id);
        if (station == nullptr)
        {
            log_w("Couldn't find station with ID %d, skipping", station_id);
            continue;
        }
        setStationState(*station, newState);
    }
    
    m_bluetooth->notifyStationStates();
}

void WaterManager::setStationState(Station& station, bool newState)
{
    auto newStateValue = newState ? HIGH : LOW;
    log_d("Setting station id %d to new state %s", station.id, (newState ? "ON" : "OFF"));
    digitalWrite(station.gpio_pin, newStateValue);
    station.is_on = newState;
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

void WaterManager::setStationStateMessage(const SetStationStateMessage& stationStateMessage)
{    
    Station* station = m_storage->getStation(stationStateMessage.station_id);
    if (station == nullptr)
    {
        log_w("Couldn't find station with ID %d, skipping", stationStateMessage.station_id);
        return;
    }
    setStationState(*station, stationStateMessage.is_on);
    m_bluetooth->notifyStationStates();
}
