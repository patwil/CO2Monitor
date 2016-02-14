/*
 * sysdWatchdog.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef SYSDWATCHDOG_H
#define SYSDWATCHDOG_H

#include <iostream>
#include <systemd/sd-daemon.h>
#include <mutex>
#include <memory>

using namespace std;

class SysdWatchdog
{
    const char* kWatchdogStr = "WATCHDOG=1";
    int bWdogEnabled;
    uint64_t wdogTimoutUsec;

    static mutex _mutex;

    SysdWatchdog ();
    SysdWatchdog (const SysdWatchdog& rhs);
    SysdWatchdog& operator= (const SysdWatchdog& rhs);
    SysdWatchdog* operator& ();
    const SysdWatchdog* operator& () const;

public:

    static shared_ptr<SysdWatchdog>& getInstance() {
        static shared_ptr<SysdWatchdog> instance = nullptr;
        if (!instance) {
            lock_guard<mutex> lock(_mutex);

            if (!instance) {
                instance.reset(new SysdWatchdog());
            }
        }
        return instance;
    }

    ~SysdWatchdog ();
    bool isEnabled();
    void kick();

};

extern shared_ptr<SysdWatchdog> sdWatchdog;

#endif /* SYSDWATCHDOG_H */
