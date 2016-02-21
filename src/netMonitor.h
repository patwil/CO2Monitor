/*
 * netMonitor.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef NETMONITOR_H
#define NETMONITOR_H

#include <iostream>

#include <time.h>
#include <sys/time.h>

class NetMonitor
{
    NetMonitor(const NetMonitor& rhs);
    NetMonitor& operator= (const NetMonitor& rhs);
    NetMonitor* operator& ();
    const NetMonitor* operator& () const;

    int getGCD(int a, int b);

    time_t _networkCheckPeriod;
    time_t _wdogKickPeriod;

    std::string _netDevice;
    time_t _netDeviceDownRebootMinTime;
    time_t _netDeviceDownPowerOffMinTime;
    time_t _netDeviceDownPowerOffMaxTime;

    bool _shouldTerminate;

public:
    NetMonitor();

    ~NetMonitor();

    void loop();
    void terminate();
};

#endif /* NETMONITOR_H */
