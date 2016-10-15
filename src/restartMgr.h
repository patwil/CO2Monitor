/*
 * restartMgr.h
 *
 * Created on: 2016-08-14
 *     Author: patw
 */

#ifndef RESTARTMGR_H
#define RESTARTMGR_H

#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <unistd.h>
//#include <linux/reboot.h>
#include <sys/reboot.h>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include <zmq.hpp>

#include "co2PersistentStore.h"

class RestartMgr
{
    public:
        RestartMgr();
        virtual ~RestartMgr();

        void init();

        void stop();
        void stop(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void restart();
        void restart(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void reboot(bool userReq);
        void reboot(uint32_t temperature, uint32_t co2, uint32_t relHumidity, bool userReq);

        void shutdown();
        void shutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void readPersistentStore(const char* progName) {
            persistentStore_->read(progName);
        }

        co2Message::Co2PersistentStore_RestartReason restartReason();

    private:

        void doShutdown(uint32_t temperature, uint32_t co2, uint32_t relHumidity);

        void delayWithWdogKick(uint32_t delay);

        co2Message::Co2PersistentStore_RestartReason restartReason_;

        const int kMaxRestartDelay = 60 * 30; // 30 minutes

        Co2PersistentStore* persistentStore_;

    protected:
};


#endif /* RESTARTMGR_H */
