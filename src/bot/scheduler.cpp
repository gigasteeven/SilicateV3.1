#include "scheduler.hpp"

void ScheduledJob::update(const float dt) {
    m_lastExecuted += dt;

    if (m_lastExecuted < m_interval) return;

    m_lastExecuted = 0.0;
    m_executor();
    m_stale = !m_repeat;
}


BotScheduler::JobId BotScheduler::schedule(const JobExecutor& executor,
                                           const double interval,
                                           const bool repeat) {
    m_nextFreeId++;

    m_jobs.insert({m_nextFreeId, {
        .m_interval = interval,
        .m_repeat = repeat,
        .m_executor = executor,
    }});

    return m_nextFreeId;
}

void BotScheduler::unschedule(const JobId id) {
    if (!m_jobs.contains(id)) {
        return;
    }

    m_jobs.erase(id);
}

void BotScheduler::reschedule(const JobId id, const double interval) {
    if (!m_jobs.contains(id)) {
        return;
    }

    m_jobs[id].m_interval = interval;
}

void BotScheduler::update(const float dt) {
    for (auto& [id, job] : m_jobs) {
        job.update(dt);
        if (job.m_stale) {
            this->unschedule(id);
        }
    }
}
