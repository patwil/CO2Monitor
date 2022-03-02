/*
 * co2PersistentConfigStore.h
 *
 * Created on: 2022-02-23
 *     Author: patw
 */

#ifndef CO2PERSISTENTCONFIGSTORE_H
#define CO2PERSISTENTCONFIGSTORE_H

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include <string>
#include "utils.h"

class Co2PersistentConfigStore
{
public:
        Co2PersistentConfigStore(void);

        ~Co2PersistentConfigStore(void);

        void init(const char* fileName);

        bool hasConfig(void) const;
        bool read(void);
        void write(void);

        void setRelHumFanOnThreshold(int relHumFanOnThreshold);
        int relHumFanOnThreshold(void) const { return relHumFanOnThreshold_; }

        void setCo2FanOnThreshold(int relHumFanOnThreshold);
        int co2FanOnThreshold(void) const { return co2FanOnThreshold_; }

        void setFanOverride(std::string& fanOverrideStr);
        void setFanOverride(co2Message::FanConfig_FanOverride fanOverride);
        co2Message::FanConfig_FanOverride const fanOverride(void) { return fanOverride_; }

private:
        std::string pathName_;

        int relHumFanOnThreshold_;
        bool relHumSyncNeeded_;
        int co2FanOnThreshold_;
        bool co2SyncNeeded_;
        co2Message::FanConfig_FanOverride fanOverride_;
        bool fanOverrideSyncNeeded_;

        static const std::string titleSetting_;
        static const std::string titleValue_;
        static const std::string fanConfigSetting_;
        static const std::string relHumSetting_;
        static const std::string co2Setting_;
        static const std::string fanOverrideSetting_;

protected:
};


#endif /* CO2PERSISTENTCONFIGSTORE_H */
