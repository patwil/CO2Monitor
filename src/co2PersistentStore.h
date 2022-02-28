/*
 * co2PersistentStore.h
 *
 * Created on: 2016-08-31
 *     Author: patw
 */

#ifndef CO2PERSISTENTSTORE_H
#define CO2PERSISTENTSTORE_H

#include <iostream>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "co2Message.pb.h"

class Co2PersistentStore
{
    public:
        Co2PersistentStore();

        ~Co2PersistentStore();

        void setFileName(const char* fileName);
        void read();
        void write();

        void setRestartReason(co2Message::Co2PersistentStore_RestartReason restartReason);

        co2Message::Co2PersistentStore_RestartReason restartReason() { return restartReason_; }

        uint64_t secondsSinceLastRestart();

        void setNumberOfRebootsAfterFail(uint32_t numberOfRebootsAfterFail);

        uint32_t numberOfRebootsAfterFail() { return numberOfRebootsAfterFail_; }

        void setTemperature(uint32_t temperature);

        uint32_t temperature() { return temperature_; }

        void setCo2(uint32_t co2);

        uint32_t co2() { return co2_; }

        void setRelHumidity(uint32_t relHumidity);

        uint32_t relHumidity() { return relHumidity_; }

private:
        std::string pathName_;

        co2Message::Co2PersistentStore_RestartReason restartReason_;
        bool restartReasonWasSet_;

        time_t lastRestartTime_;

        uint32_t numberOfRebootsAfterFail_;
        bool numberOfRebootsAfterFailWasSet_;

        uint32_t temperature_;
        bool temperatureWasSet_;

        uint32_t co2_;
        bool co2WasSet_;

        uint32_t relHumidity_;
        bool relHumidityWasSet_;
};

#endif /* CO2PERSISTENTSTORE_H */
