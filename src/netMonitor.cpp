/*
 * netMonitor.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "netMonitor.h"
#include "config.h"
#include "parseConfigFile.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

using namespace std;

NetMonitor::NetMonitor(int deviceCheckRetryPeriod, int networkCheckPeriod) :
    _deviceCheckRetryPeriod(deviceCheckRetryPeriod),
    _networkCheckPeriod(networkCheckPeriod),
    _shouldTerminate(false)
{
}

NetMonitor::~NetMonitor ()
{
    // Delete all dynamic memory.
}

void NetMonitor::loop()
{
    while (!_shouldTerminate) {
#ifdef SYSTEMD_WDOG
        sdWatchdog.kick();
#endif
    }
}

int NetMonitor::getGCD(int a, int b)
{
    if ( (a == 0) || (b == 0) ) {
        return 1;
    }

    a = abs(a);
    b = abs(b);

    if (a == b) {
        return a;
    }

    int x;
    int y;

    if (a > b) {
        x = a;
        y = b;
    } else {
        x = b;
        y = a;
    }

    while (true) {
        x = x % y;
        if (x == 0) {
            return y;
        }

        y = y % x;
        if (y == 0) {
            return x;
        }
    }
}

