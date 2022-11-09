#pragma once

#include <map>

#include "BlablaCallbacks.h"

#include <TaskManagerIO.h>

class BlablaCallbacks;

class CronManager
{
public:
    CronManager() = default;
    ~CronManager() = default;

    void setCronCallbacks(BlablaCallbacks* callbacks);

    void loop();

    void addEvent(const Event& event);
    void removeEvent(const Event& event);

    void begin();

    taskid_t getTaskId(uint32_t eventId) const;

private:

    time_t getNextRun(const Event& event);

    std::map<uint32_t, taskid_t> m_jobs;
    BlablaCallbacks* m_pCallback;

};