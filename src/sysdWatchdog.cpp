/*
 * sysdWatchdog.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include "sysdWatchdog.h"

SysdWatchdog::SysdWatchdog() :
    bWdogEnabled_(0),
    kickPeriod_(0),
    timeOfLastKick_(0)
{
}

SysdWatchdog::~SysdWatchdog()
{
    // Delete all dynamic memory.
}

bool SysdWatchdog::isEnabled()
{
    return (bWdogEnabled_ > 0);
}

void SysdWatchdog::kick()
{
#ifdef SYSTEMD_WDOG
    bWdogEnabled_ = sd_watchdog_enabled(0, &wdogTimoutUsec_);

    if (bWdogEnabled_ > 0) {
        sd_notify(0, kWatchdogStr_);
    }

#endif
    timeOfLastKick_ = time(0); // we pretend to kick wdog even when not present
}

time_t SysdWatchdog::timeUntilNextKick()
{
    time_t timeSinceLastKick = time(0) - timeOfLastKick_;

    return (timeSinceLastKick > kickPeriod_) ? 0 : kickPeriod - timeSinceLastKick;
}


std::mutex SysdWatchdog::mutex_;


std::shared_ptr<SysdWatchdog> sdWatchdog = SysdWatchdog::getInstance();


