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

class NetMonitor
{
        typedef enum {
            Unknown,
            Start,
            Up,
            NoConnection,
            Down,
            Restart,
            StillDown,
            Missing,
            NoNetDevices,
            Invalid
        } State;

        NetMonitor();
        NetMonitor(const NetMonitor& rhs);
        NetMonitor& operator= (const NetMonitor& rhs);
        NetMonitor* operator& ();
        const NetMonitor* operator& () const;

        int getGCD(int a, int b);

        time_t networkCheckPeriod_;

        std::string netDevice_;
        time_t netDeviceDownRebootMinTime_;

        zmq::context_t& ctx_;
        zmq::socket_t mainSocket_;
        zmq::socket_t subSocket_;

        //std::mutex mutex_; // used to control access to attributes used by multiple threads

        std::atomic<bool> shouldTerminate_;
        std::atomic<co2Message::NetState_NetStates> currentState_;
        std::atomic<co2Message::NetState_NetStates> prevState_;
        std::atomic<bool> stateChanged_;
        std::atomic<co2Message::ThreadState_ThreadStates> threadState_;
        std::atomic<bool> threadStateChanged_;

        NetLink::LinkState linkState_;

    public:
        NetMonitor(zmq::context_t& ctx, int sockType);

        ~NetMonitor();

        co2Message::NetState_NetStates checkNetInterfacesPresent();
        void run();
        void terminate();

        void listener();
        void sendNetState();
        void sendThreadState();
};

#endif /* NETMONITOR_H */
