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
        int tempInt;

        if (pCfg->find("NetDevice") != pCfg->end()) {
            //const char* p = pCfg->find("NetDevice")->second->getStr();
            //_netDevice = string(p);
            _netDevice = string(pCfg->find("NetDevice")->second->getStr());
        }
        if (pCfg->find("NetworkCheckPeriod") != pCfg->end()) {
            tempInt = pCfg->find("NetworkCheckPeriod")->second->getInt();
            _networkCheckPeriod = (time_t)tempInt;
        }
        if (pCfg->find("WatchdogKickPeriod") != pCfg->end()) {
            tempInt = pCfg->find("WatchdogKickPeriod")->second->getInt();
            _wdogKickPeriod = (time_t)tempInt;
        }
        if (pCfg->find("NetDeviceDownRebootMinTime") != pCfg->end()) {
            tempInt = pCfg->find("NetDeviceDownRebootMinTime")->second->getInt();
            _netDeviceDownRebootMinTime = (time_t)tempInt;
        }
        if (pCfg->find("NetDeviceDownPowerOffMinTime") != pCfg->end()) {
            tempInt = pCfg->find("NetDeviceDownPowerOffMinTime")->second->getInt();
            _netDeviceDownPowerOffMinTime = (time_t)tempInt;
        }
        if (pCfg->find("NetDeviceDownPowerOffMaxTime") != pCfg->end()) {
            tempInt = pCfg->find("NetDeviceDownPowerOffMaxTime")->second->getInt();
            _netDeviceDownPowerOffMaxTime = (time_t)tempInt;
        }
    } catch (exception& e) {
        throw e;
    } catch (...) {
        throw;
    }
    syslog(LOG_DEBUG, "NetMonitor init: NetDevice=\"%s\"  NetworkCheckPeriod=%lus  NetDeviceDownRebootMinTime=%lus  "
                      "NetDeviceDownPowerOffMinTime=%lus  NetDeviceDownPowerOffMaxTime=%lus",
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

        if ( (timeNow - timeLastWdogKick) >= _wdogKickPeriod ) {
#ifdef SYSTEMD_WDOG
            sdWatchdog->kick();
#endif
            timeLastWdogKick = timeNow;
        }

        if ( (timeNow - timeLastNetCheck) >= _networkCheckPeriod ) {
            try {
                singlePing->pingGateway();
                nPingFails = 0; // all is forgiven, start over
            } catch (pingException& pe) {
                if (++nPingFails > nConsecFailsAllowed) {
                    _shouldTerminate = true;
                    throw pe;
                }
                syslog(LOG_ERR, "Ping error (%1d): %s", nPingFails, pe.what());
            } catch (exceptionLevel& el) {
                if ( el.isFatal() || (++nPingFails > nConsecFailsAllowed) ) {
                    _shouldTerminate = true;
                    throw el;
                }
                syslog(LOG_ERR, "Ping (non-fatal) exception (%1d): %s", nPingFails, el.what());
            } catch (exception& e) {
                _shouldTerminate = true;
                throw e;
            } catch (...) {
                _shouldTerminate = true;
                throw;
            }

            timeNow = time(0);
            // ping may take a while so we cannot assume that
            // less than a second has elapsed.
            timeLastNetCheck = timeNow;
        }

        time_t timeToNextNetCheck = timeLastNetCheck + _networkCheckPeriod - timeNow;
        time_t timeToNextWdogKick = timeLastWdogKick + _wdogKickPeriod - timeNow;
        time_t netLinkTimeout =  (timeToNextNetCheck < timeToNextWdogKick) ? timeToNextNetCheck : timeToNextWdogKick;

        syslog(LOG_DEBUG, "timeToNextNetCheck:%lu  timeToNextWdogKick:%lu netLinkTimeout:%lu\n",
                           timeToNextNetCheck, timeToNextWdogKick, netLinkTimeout);

        bool netLinkEvent = false;
        try {
            netLinkEvent = devNetLink->readEvent(netLinkTimeout);
        } catch (exceptionLevel& el) {
            if (el.isFatal()) {
                _shouldTerminate = true;
                throw el;
            }
            netLinkEvent = true;
            syslog(LOG_ERR, "NetLink (non-fatal) exception (%1d): %s", nPingFails, el.what());
        } catch (...) {
            _shouldTerminate = true;
            throw;
        }

        if ( netLinkEvent && (devNetLink->linkState() == NetLink::DOWN) ) {
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

