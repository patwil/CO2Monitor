/*
 * netMonitor.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <dirent.h>

#include "netMonitor.h"
#include "ping.h"
#include "netLink.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

using namespace std;

NetMonitor::NetMonitor() : _shouldTerminate(false), _currentState(Start), _prevState(Start)
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
    } catch (exception& e) {
        throw e;
    } catch (...) {
        throw;
    }
    syslog(LOG_DEBUG, "NetMonitor init: NetDevice=\"%s\"  NetworkCheckPeriod=%lus  NetDeviceDownRebootMinTime=%lus",
                      _netDevice.c_str(), _networkCheckPeriod, _netDeviceDownRebootMinTime);
}

NetMonitor::~NetMonitor ()
{
    // Delete all dynamic memory.
}

State NetMonitor::precheckNetInterfaces()
{
    DIR* pd;
    struct dirent* p;

    pd = opendir("/sys/class/net");
    if (pd) {
        while (p = readdir(pd)) {
            std::cout << p->d_name << std::endl;
        }
        closedir(pd);
    }
    return 0;
}

void NetMonitor::loop()
{
    time_t timeNow = time(0);
    time_t timeOfNextNetCheck = timeNow;
    time_t timeOfNextWdogKick = timeNow;
    time_t netLinkTimeout = 0;

    // Number of times ping is allowed to fail consecutively
    // before we terminate or reboot, i.e. third time is unlucky
    const int nConsecFailsAllowed = 2;

    int nPingFails = 0;
    Ping *singlePing = 0;
    NetLink* devNetLink = 0;
    State currentState = Up;
    State prevState = Up;
    time_t stateChangeTime = timeNow;

    try {
        singlePing = new Ping();
        devNetLink = new NetLink(_netDevice.c_str());
        devNetLink->open();
    } catch (...) {
        throw;
    }

    // A simple monitor loop which endeavours to make sure that:
    //  - watchdog is kicked every _wdogKickPeriod seconds
    //  - gateway is pinged every _networkCheckPeriod seconds
    //  - network interface is checked for netlink status change in the
    //    intervening periods.
    //
    while (!_shouldTerminate) {
        timeNow = time(0);

        if (timeNow >= timeOfNextWdogKick) {
#ifdef SYSTEMD_WDOG
            sdWatchdog->kick();
#endif
            timeOfNextWdogKick += _wdogKickPeriod;
        }

        if (currentState == Up) {
            if (timeNow >= timeOfNextNetCheck) {
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
                timeOfNextNetCheck = timeNow + _networkCheckPeriod;
            }

        } else {
            // We haven't pinged because the network is down
            timeOfNextNetCheck = timeNow + _networkCheckPeriod;

            switch (currentState) {
            case Down:
                break;
            case DownReboot:
                break;
            case DownPowerOff:
                break;
            case DownTerminate:
                break;
            default:
                break;
            }
        }

        netLinkTimeout =  (timeOfNextNetCheck < timeOfNextWdogKick) ? timeOfNextNetCheck : timeOfNextWdogKick;
        netLinkTimeout -= timeNow;

        syslog(LOG_DEBUG, "netLinkTimeout:%lu\n", netLinkTimeout);

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

        if (netLinkEvent) {
            currentState = devNetLink->linkState();
            if (currentState != prevState) {
                stateChangeTime = timeNow;
            }
        }

        switch (currentState) {
        case Up:
            switch (prevState) {
            case Up:
                break;
            case Down:
                prevState = currentState;
                break;
            case DownReboot:

            }
            break;
        case Down:
            break;
        case DownReboot:
            break;
        case DownPowerOff:
            break;
        case DownTerminate:
            break;
        default:
            break;
        }
        if (netLinkEvent && (devNetLink->linkState() == NetLink::DOWN)) {
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

