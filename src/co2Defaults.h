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
#include "config.h"

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
        void clearConfigDefaults(ConfigMap& cfg);

        ~Co2Defaults();
        const int kLogLevelDefault;
    protected:
};

namespace CO2 {

extern std::shared_ptr<Co2Defaults> co2Defaults;

} // namespace CO2

#endif /* CO2DEFAULTS_H */
