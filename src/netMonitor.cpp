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
#include <time.h>
#include <sys/time.h>

#include "netMonitor.h"
#include "ping.h"
#include "netLink.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

using namespace std;

NetMonitor::NetMonitor() : _shouldTerminate(false)
{
    try {
        ConfigMap* pCfg = globals->getCfg();

        if (pCfg->find("NetDevice") != pCfg->end()) {
            _netDevice = string(pCfg->find("NetDevice")->second->getStr());
        }
        if (pCfg->find("NetworkCheckPeriod") != pCfg->end()) {
            _networkCheckPeriod = pCfg->find("NetworkCheckPeriod")->second->getInt();
        }
        if (pCfg->find("WatchdogKickPeriod") != pCfg->end()) {
            _wdogKickPeriod = pCfg->find("WatchdogKickPeriod")->second->getInt();
        }
        if (pCfg->find("NetDeviceDownRebootMinTime") != pCfg->end()) {
            _netDeviceDownRebootMinTime = pCfg->find("NetDeviceDownRebootMinTime")->second->getInt();
        }
        if (pCfg->find("NetDeviceDownPowerOffMinTime") != pCfg->end()) {
            _netDeviceDownPowerOffMinTime = pCfg->find("NetDeviceDownPowerOffMinTime")->second->getInt();
        }
        if (pCfg->find("NetDeviceDownPowerOffMaxTime") != pCfg->end()) {
            _netDeviceDownPowerOffMaxTime = pCfg->find("NetDeviceDownPowerOffMaxTime")->second->getInt();
        }
    } catch (exception& e) {
        throw e;
    } catch (...) {
        throw;
    }
    syslog(LOG_DEBUG, "NetMonitor init: NetDevice=\"%s\"  NetworkCheckPeriod=%1ds  NetDeviceDownRebootMinTime=%1ds  "
                      "NetDeviceDownPowerOffMinTime=%1ds  NetDeviceDownPowerOffMaxTime=%1ds",
                      _netDevice.c_str(), _networkCheckPeriod, _netDeviceDownRebootMinTime,
                      _netDeviceDownPowerOffMinTime, _netDeviceDownPowerOffMaxTime);
}

NetMonitor::~NetMonitor ()
{
    // Delete all dynamic memory.
}

void NetMonitor::loop()
{
    time_t timeNow;
    time_t timeLastPing = 0;
    time_t timeLastNetCheck = 0;
    time_t timeLastWdogKick = 0;
    const int nConsecFailsAllowed = 2; // third time is unlucky
    int nPingFails = 0;
    Ping *singlePing = 0;
    NetLink* devNetLink = 0;

    try {
        singlePing = new Ping();
        devNetLink = new NetLink(_netDevice.c_str());
        devNetLink->open();
    } catch (...) {
        throw;
    }

    while (!_shouldTerminate) {
        timeNow = time(0);

#ifdef SYSTEMD_WDOG
        if ( (timeNow - timeLastWdogKick) > _wdogKickPeriod ) {
            sdWatchdog->kick();
            timeLastWdogKick = timeNow;
        }
#else
        timeLastWdogKick = timeNow;
#endif

        if ( (timeNow - timeLastPing) > _networkCheckPeriod ) {
            try {
                singlePing->pingGateway();
                nPingFails = 0; // all is forgiven, start over
            } catch (pingException& pe) {
                if (++nPingFails > nConsecFailsAllowed) {
                    throw pe;
                }
                syslog(LOG_ERR, "Ping error (%1d): %s", nPingFails, pe.what());
            } catch (exceptionLevel& el) {
                if ( el.isFatal() || (++nPingFails > nConsecFailsAllowed) ) {
                    throw el;
                }
                syslog(LOG_ERR, "Ping (non-fatal) exception (%1d): %s", nPingFails, el.what());
            } catch (exception& e) {
                throw e;
            }

            timeNow = time(0);
            // ping may take a while so we cannot assume that
            // less than a second has elapsed.
            timeLastPing = timeNow;
        }

        int netLinkTimeout =  ((timeLastNetCheck + _networkCheckPeriod) < (timeLastWdogKick + _wdogKickPeriod)) ?
                                int(timeLastNetCheck + _networkCheckPeriod) : int(timeLastWdogKick + _wdogKickPeriod);
        printf("timeToNextNetCheck:%ld  timeToNextWdogKick:%ld netLinkTimeout:%d\n",
               (timeLastNetCheck + _networkCheckPeriod - timeNow),
                (timeLastWdogKick + _wdogKickPeriod - timeNow), netLinkTimeout);
        try {
            devNetLink->readEvent(netLinkTimeout);
        } catch (...) {
        }


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

