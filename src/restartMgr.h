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


class RestartMgr
{
    public:
        RestartMgr();
        virtual ~RestartMgr();

        void shutdown();

    private:
        RestartMgr(const RestartMgr& rhs);
        RestartMgr& operator=(const RestartMgr& rhs);
        RestartMgr* operator&();
        const RestartMgr* operator&() const;

        int restartDelay_;

    protected:
};


#endif /* RESTARTMGR_H */
