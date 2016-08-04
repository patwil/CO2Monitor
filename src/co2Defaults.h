/*
 * co2Defaults.h
 *
 * Created on: 2016-07-09
 *     Author: patw
 */

#ifndef CO2DEFAULTS_H
#define CO2DEFAULTS_H

#include <syslog.h>
#include <thread>
#include <mutex>

class Co2Defaults
{
    private:
        Co2Defaults();
        Co2Defaults(const Co2Defaults& rhs);
        Co2Defaults& operator=(const Co2Defaults& rhs);
        Co2Defaults* operator&();
        const Co2Defaults* operator&() const;

        static std::mutex mutex_;

    public:
        static std::shared_ptr<Co2Defaults>& getInstance() {
            static std::shared_ptr<Co2Defaults> instance = nullptr;

            if (!instance) {
                std::lock_guard<std::mutex> lock(mutex_);

                if (!instance) {
                    instance.reset(new Co2Defaults());
                }
            }

            return instance;
        }

        void setConfigDefaults(ConfigMap& cfg);

        virtual ~Co2Defaults();

        const int kNetworkCheckPeriodDefault;
        const int kWatchdogKickPeriod;
        const int kLogLevelDefault;
        const char* kNetDevice;
        const int kNetDeviceDownRebootMinTime;
        const int kNetDeviceDownPowerOffMinTime;
        const int kNetDeviceDownPowerOffMaxTime;
        const char* kCO2Port;
        const char* kPersistentStoreFileName;
        const char* kSdlFbDev;
        const char* kSdlMouseDev;
        const char* kSdlMouseDrv;
        const char* kSdlMouseRel;
        const char* kSdlVideoDriver;
        const char* kSdlTtfDir;
        const char* kSdlBmpDir;
        const int kScreenRefreshRate;
        const int kScreenTimeout;
        const int kFanOnOverrideTime;
        const int kRelHumFanOnThreshold;
        const int kCO2FanOnThreshold;

    protected:
};

extern std::shared_ptr<Co2Defaults> co2Defaults;

#endif /* CO2DEFAULTS_H */
