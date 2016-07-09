/*
 * sysdWatchdog.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include "sysdWatchdog.h"

SysdWatchdog::SysdWatchdog() : bWdogEnabled_(0)
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
}

mutex SysdWatchdog::mutex_;

#ifdef SYSTEMD_WDOG
shared_ptr<SysdWatchdog> sdWatchdog = SysdWatchdog::getInstance();
#endif

