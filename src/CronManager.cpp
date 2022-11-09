
#include "CronManager.h"
#include "BlablaCallbacks.h"

#include "esp32-hal-log.h"
#include "ccronexpr.h"

#include <TmLongSchedule.h>

void CronManager::setCronCallbacks(BlablaCallbacks* callbacks)
{
    m_pCallback = callbacks;
}

void CronManager::loop()
{
    taskManager.runLoop();
}

time_t CronManager::getNextRun(const Event& event)
{
    struct timeval now;
    gettimeofday(&now, NULL);

    const char *error = NULL;
    cron_expr expression;
    memset(&expression, 0, sizeof(expression));
    cron_parse_expr(event.cron_expr.c_str(), &expression, &error);
    time_t next_run = cron_next(&expression, now.tv_sec);
    return next_run - now.tv_sec;
}

void CronManager::addEvent(const Event& event)
{
    if (m_jobs.find(event.id) != m_jobs.end())
    {
        log_w("Job ID already exists. Please remove before adding");
        return;
    }
    
    time_t next_run = getNextRun(event);
    
    log_d("[addEvent] - next run is in %ld seconds\n", next_run);
    
    auto taskId = taskManager.registerEvent(new TmLongSchedule(next_run*1000, [this, event]() {
        // Turn station on
        if (m_pCallback)
        {
            m_pCallback->onEventStateChange(event, true);
        }

        // Schedule the off event
        auto offTaskId = taskManager.registerEvent(new TmLongSchedule(event.duration*1000, [this, event]() {
            // Turn station off            
            if (m_pCallback)
            {
                m_pCallback->onEventStateChange(event, false);
            }

            // Remove the event and re-add it to start next run
            m_jobs.erase(event.id);
            addEvent(event);
        }, true), true);

        m_jobs[event.id] = offTaskId;

    }, true), true);

    if (taskId == TASKMGR_INVALIDID)
    {
        log_e("Failed scheduling task");
        return;
    }
    m_jobs[event.id] = taskId;
}

void CronManager::removeEvent(const Event& event)
{
    auto it = m_jobs.find(event.id);
    if (it != m_jobs.end())
    {
        taskManager.cancelTask(it->second);
        m_jobs.erase(event.id);
    }
}

void CronManager::begin()
{
    log_i("Starting cron manager");
}

taskid_t CronManager::getTaskId(uint32_t eventId) const
{
    auto it = m_jobs.find(eventId);
    if (it != m_jobs.end())
    {
        return it->second;
    }
    return TASKMGR_INVALIDID;
}
