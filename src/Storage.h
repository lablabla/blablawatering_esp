#pragma once

#include <map>

#include "esp_log.h"

#include "data.h"


class Storage
{
public:
    Storage();
    ~Storage();

    bool loadStations();
    bool loadEvents();

    bool clear();

    bool addStation(const Station& station);
    bool removeStation(uint32_t id);

    bool clearStations();
    bool setStations(const std::map<uint32_t, Station>& stations);
    
    const std::map<uint32_t, Station>& getStations() const { return m_stations; }
    Station* getStation(uint32_t id);


    bool addEvent(const Event& event);
    bool removeEvent(uint32_t id);

    bool clearEvents();
    bool setEvents(const std::map<uint32_t, Event>& events);

    const std::map<uint32_t, Event>& getEvents() const { return m_events; }
    Event* getEvent(uint32_t id);

private:
    std::map<uint32_t, Station> m_stations;
    std::map<uint32_t, Event> m_events;    

};
