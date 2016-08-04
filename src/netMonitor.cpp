/*
 * netMonitor.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include <vector>
#include <thread>
#include <memory>
#include <functional>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <dirent.h>

#include <zmq.hpp>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include <zmq.hpp>

#include "netMonitor.h"
#include "ping.h"
#include "netLink.h"
#include "utils.h"

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

NetMonitor::NetMonitor(zmq::context_t& ctx, int sockType) :
    ctx_(ctx),
    mainSocket_(ctx, sockType),
    shouldTerminate_(false),
    currentState_(Start),
    prevState_(Start)
{
    syslog(LOG_DEBUG, "NetMonitor init: NetDevice=\"%s\"  NetworkCheckPeriod=%lus  NetDeviceDownRebootMinTime=%lus",
           netDevice_.c_str(), networkCheckPeriod_, netDeviceDownRebootMinTime_);
}

NetMonitor::~NetMonitor()
{
    // Delete all dynamic memory.
}

State NetMonitor::checkNetInterfacesPresent()
{
    DIR* pd;
    struct dirent* p;
    State netState = NoNetDevices;

    pd = opendir("/sys/class/net");

    if (pd) {
        while (p = readdir(pd)) {
            if (!strncmp(p->d_name, netDevice_.c_str(), netDevice_.length())) {
                netState = Unknown;
                break;
            }

            netState = Missing;
        }

        closedir(pd);
    }

    return netState;
}

void NetMonitor::runloop()
{
    time_t timeNow = time(0);
    time_t timeOfNextNetCheck = timeNow  + networkCheckPeriod_;
    time_t netLinkTimeout = 0;

    Ping* singlePing = 0;
    NetLink* devNetLink = 0;
    currentState_;
    prevState_ = Unknown;
    const int kAllowedPingFails = 5;

    //
    currentState_ =  this->checkNetInterfacesPresent();

    switch (currentState_) {
        case NoNetDevices:
        case Missing:
            syslog (LOG_ERR, "No network interface present");
            throw exceptionLevel("No network interface present", true);

        default:
            break;
    }

    try {
        singlePing = new Ping();
        singlePing->setAllowedFailCount(kAllowedPingFails);
        devNetLink = new NetLink(netDevice_.c_str());
        devNetLink->open();
    } catch (...) {
        throw;
    }

    while (!shouldTerminate_) {
        timeNow = time(0);

        netLinkTimeout =  timeOfNextNetCheck - timeNow;

        syslog(LOG_DEBUG, "netLinkTimeout:%lu\n", netLinkTimeout);

        bool netLinkEvent = false;

        try {
            netLinkEvent = devNetLink->readEvent(netLinkTimeout);
        } catch (exceptionLevel& el) {
            if (el.isFatal()) {
                shouldTerminate_ = true;
                throw el;
            }

            netLinkEvent = true;
            syslog(LOG_ERR, "NetLink (non-fatal) exception: %s", el.what());
        } catch (...) {
            shouldTerminate_ = true;
            throw;
        }

        if (netLinkEvent) {
            currentState = devNetLink->linkState();

            if (currentState != prevState) {
                stateChangeTime = timeNow;
            }
        }

        if (devNetLink->linkState() == NetLink::UP) {

            if (timeNow >= timeOfNextNetCheck) {
                try {
                    singlePing->pingGateway();
                } catch (pingException& pe) {
                    if (singlePing->state() == Ping::Fail) {
                        shouldTerminate_ = true;
                        syslog(LOG_ERR, "Ping FAIL: %s", pe.what());
                    } else {

                    }
                } catch (exceptionLevel& el) {
                    if (el.isFatal()) {
                        shouldTerminate_ = true;
                        throw el;
                    }

                    syslog(LOG_ERR, "Ping (non-fatal) exception: %s", el.what());
                } catch (std::exception& e) {
                    shouldTerminate_ = true;
                    throw e;
                } catch (...) {
                    shouldTerminate_ = true;
                    throw;
                }

                timeNow = time(0);
                // ping may take a while so we cannot assume that
                // less than a second has elapsed.
                timeOfNextNetCheck = timeNow + networkCheckPeriod_;
            }

        } else {
            // We haven't pinged because the network is down
            timeOfNextNetCheck = timeNow + networkCheckPeriod_;

        }


        Start,
        Up,
        NoConnection,
        Down,
        Restart,
        StillDown,
        Missing,
        NoNetDevices,
        Invalid

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

