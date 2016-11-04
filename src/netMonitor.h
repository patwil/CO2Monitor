/*
 * netMonitor.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef NETMONITOR_H
#define NETMONITOR_H

#include <iostream>
#include <thread>         // std::thread
#include <mutex>          // std::mutex
#include <atomic>

#include <ctime>
#include <sys/time.h>
#include <zmq.hpp>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include "netLink.h"
#include "utils.h"

class NetMonitor
{
    public:
        typedef enum {
            LinkUp,
            LinkDown,
            NetUp,
            NetDown,
            NetDeviceMissing,
            NetDeviceFail,
            NoNetDevices,
            Timeout
        } StateEvent;

    private:
        NetMonitor();
        NetMonitor(const NetMonitor& rhs);
        NetMonitor& operator= (const NetMonitor& rhs);
        NetMonitor* operator& ();
        const NetMonitor* operator& () const;

        int getGCD(int a, int b);

        zmq::context_t& ctx_;
        zmq::socket_t mainSocket_;
        zmq::socket_t subSocket_;

        time_t networkCheckPeriod_;

        std::string netDevice_;
        time_t netDeviceDownRebootMinTime_;
        time_t netDownRebootMinTime_;
        time_t stateChangeTime_;
        time_t netDeviceDownTime_;
        time_t netDownTime_;

        //std::mutex mutex_; // used to control access to attributes used by multiple threads

        std::atomic<co2Message::NetState_NetStates> netState_;
        CO2::ThreadFSM* threadState_;

        NetLink::LinkState linkState_;

        StateEvent checkNetInterfacesPresent();
        void netFSM(StateEvent event);
        const char* netStateStr();
        const char* netStateStr(co2Message::NetState_NetStates netState);
        void terminate();
        void listener();
        void sendNetState();
        void getConfigFromMsg(co2Message::Co2Message& netCfgMsg);

    public:
        NetMonitor(zmq::context_t& ctx, int sockType);

        ~NetMonitor();

        void run();

};

#endif /* NETMONITOR_H */
