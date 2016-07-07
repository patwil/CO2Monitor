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

class SysdWatchdog
{
    const char* kWatchdogStr_ = "WATCHDOG=1";
    int bWdogEnabled_;
    uint64_t wdogTimoutUsec_;

    static mutex mutex_;

    SysdWatchdog ();
    SysdWatchdog (const SysdWatchdog& rhs);
    SysdWatchdog& operator= (const SysdWatchdog& rhs);
    SysdWatchdog* operator& ();
    const SysdWatchdog* operator& () const;

public:

    static shared_ptr<SysdWatchdog>& getInstance() {
        static shared_ptr<SysdWatchdog> instance = nullptr;
        if (!instance) {
            lock_guard<mutex> lock(mutex_);

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
