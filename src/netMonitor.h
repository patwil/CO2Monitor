/*
 * netMonitor.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef NETMONITOR_H
#define NETMONITOR_H

#include <iostream>

#include <ctime>
#include <sys/time.h>
#include <zmq.hpp>

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
        bool shouldTerminate_;
        State currentState_;
        State prevState_;

    public:
        NetMonitor(zmq::context_t& ctx, int sockType);

        ~NetMonitor();

        State checkNetInterfacesPresent();
        void run();
        void terminate();
};

#endif /* NETMONITOR_H */
