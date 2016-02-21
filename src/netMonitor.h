/*
 * netMonitor.h
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#ifndef NETMONITOR_H
#define NETMONITOR_H

#include <iostream>

class NetMonitor
{
    NetMonitor(const NetMonitor& rhs);
    NetMonitor& operator= (const NetMonitor& rhs);
    NetMonitor* operator& ();
    const NetMonitor* operator& () const;

    int getGCD(int a, int b);

    int _networkCheckPeriod;
    int _wdogKickPeriod;

    std::string _netDevice;
    int _netDeviceDownRebootMinTime;
    int _netDeviceDownPowerOffMinTime;
    int _netDeviceDownPowerOffMaxTime;

    bool _shouldTerminate;

public:
    NetMonitor();

    ~NetMonitor();

    void loop();
    void terminate();
};

#endif /* NETMONITOR_H */
