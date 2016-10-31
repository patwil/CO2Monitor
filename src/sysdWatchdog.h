/*
 * sysdWatchdog.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef SYSDWATCHDOG_H
#define SYSDWATCHDOG_H

#include <iostream>
#ifdef SYSTEMD_WDOG
#include <systemd/sd-daemon.h>
#endif
#include <mutex>
#include <memory>

class SysdWatchdog
{
        const char* kWatchdogStr_ = "WATCHDOG=1";
        int bWdogEnabled_;
        uint64_t wdogTimoutUsec_;
        time_t kickPeriod_;
        time_t timeOfLastKick_;

        static std::mutex mutex_;

        SysdWatchdog ();
        SysdWatchdog (const SysdWatchdog& rhs);
        SysdWatchdog& operator= (const SysdWatchdog& rhs);
        SysdWatchdog* operator& ();
        const SysdWatchdog* operator& () const;

    public:

        static std::shared_ptr<SysdWatchdog>& getInstance() {
            static std::shared_ptr<SysdWatchdog> instance = nullptr;

            if (!instance) {
                std::lock_guard<std::mutex> lock(mutex_);

                if (!instance) {
                    instance.reset(new SysdWatchdog());
                }
            }

            return instance;
        }

        ~SysdWatchdog ();
        bool isEnabled();
        void kick();

        void setKickPeriod(time_t kickPeriod) {
            kickPeriod_ = kickPeriod;
        }

        time_t kickPeriod() {
            return kickPeriod_;
        }

        time_t timeUntilNextKick();
        time_t timeOfNextKick();
};

extern std::shared_ptr<SysdWatchdog> sdWatchdog;

#endif /* SYSDWATCHDOG_H */
