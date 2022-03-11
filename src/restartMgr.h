/*
 * restartMgr.h
 *
 * Created on: 2016-08-14
 *     Author: patw
 */

#ifndef RESTARTMGR_H
#define RESTARTMGR_H

#include "co2PersistentStore.h"

class RestartMgr
{
    public:
        RestartMgr();
        virtual ~RestartMgr();

        void init(const char* filename);

        void stop();
        void stop(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void restart();
        void restart(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void reboot(bool userReq);
        void reboot(uint32_t temperature, uint32_t co2, uint32_t relHumidity, bool userReq);

        void shutdown();
        void shutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        co2Message::Co2PersistentStore_RestartReason restartReason();

    private:

        void doShutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void waitForShutdown(bool reboot);
        void delayWithWdogKick(uint32_t delay);

        co2Message::Co2PersistentStore_RestartReason restartReason_;

        const uint32_t kMaxRestartDelay = 60 * 30; // 30 minutes
        const uint32_t kMaxPermittedConsecutiveRestarts = 3; // if service fails more than this we'll need to try rebooting

        Co2PersistentStore* persistentStore_;

    protected:
};


#endif /* RESTARTMGR_H */
