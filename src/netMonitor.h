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
    NetMonitor();
    NetMonitor(const NetMonitor& rhs);
    NetMonitor& operator= (const NetMonitor& rhs);
    NetMonitor* operator& ();
    const NetMonitor* operator& () const;

    int getGCD(int a, int b);

    int _deviceCheckRetryPeriod;
    int _networkCheckPeriod;

    bool _shouldTerminate;

public:
    NetMonitor(int deviceCheckRetryPeriod, int networkCheckPeriod);

    ~NetMonitor();

    void loop();
    void terminate();
};

#endif /* NETMONITOR_H */
