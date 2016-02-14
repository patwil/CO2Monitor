/*
 * sysdWatchdog.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include "sysdWatchdog.h"

using namespace std;

SysdWatchdog::SysdWatchdog()
{
    bWdogEnabled = 0;
}

SysdWatchdog::~SysdWatchdog()
{
    // Delete all dynamic memory.
}

bool SysdWatchdog::isEnabled()
{
    return (bWdogEnabled > 0);
}

void SysdWatchdog::kick()
{
#ifdef SYSTEMD_WDOG
    bWdogEnabled = sd_watchdog_enabled(0, &wdogTimoutUsec);
    if (bWdogEnabled > 0) {
        sd_notify(0, kWatchdogStr);
    }
#endif
}


#ifdef SYSTEMD_WDOG
shared_ptr<SysdWatchdog> sdWatchdog;
#endif

