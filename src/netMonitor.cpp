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
    subSocket_(ctx, ZMQ_SUB),
    linkState_(NetLink::DOWN)
{
    shouldTerminate_.store(false, std::memory_order_relaxed);
    currentState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
    prevState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
    stateChanged_.store(false, std::memory_order_relaxed),
    threadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);
    threadStateChanged_.store(false, std::memory_order_relaxed);
}

NetMonitor::~NetMonitor()
{
    // Delete all dynamic memory.
}

co2Message::NetState_NetStates NetMonitor::checkNetInterfacesPresent()
{
    DIR* pd;
    struct dirent* p;
    co2Message::NetState_NetStates netState = co2Message::NetState_NetStates_NO_NET_INTERFACE;

    pd = opendir("/sys/class/net");

    if (pd) {
        while ( (p = readdir(pd)) ) {
            //
            if (!strncmp(p->d_name, netDevice_.c_str(), netDevice_.length())) {
                netState = co2Message::NetState_NetStates_UNKNOWN;
                break;
            }
            //
            netState = co2Message::NetState_NetStates_MISSING;
        }
        //
        closedir(pd);
    }

    return netState;
}

void NetMonitor::run()
{
    threadState_.store(co2Message::ThreadState_ThreadStates_AWAITING_CONFIG, std::memory_order_relaxed);
    sendThreadState();

    std::thread* listenerThread = new std::thread(std::bind(&NetMonitor::listener, this));

    // We'll continue after receiving our configuration
    while (threadState_.load(std::memory_order_relaxed) == co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (shouldTerminate_.load(std::memory_order_relaxed)) {
        threadState_.store(co2Message::ThreadState_ThreadStates_FAILED, std::memory_order_relaxed);
        sendThreadState();
    }

    time_t timeNow = time(0);
    time_t timeOfNextNetCheck = timeNow  + networkCheckPeriod_;
    time_t netLinkTimeout = 0;
    time_t stateChangeTime = 0;

    Ping* singlePing = 0;
    NetLink* devNetLink = 0;
    const int kAllowedPingFails = 5;

    //
    currentState_.store(this->checkNetInterfacesPresent(), std::memory_order_relaxed);

    switch (currentState_.load(std::memory_order_relaxed)) {
        case co2Message::NetState_NetStates_NO_NET_INTERFACE:
        case co2Message::NetState_NetStates_MISSING:
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

    while (!shouldTerminate_.load(std::memory_order_relaxed)) {
        timeNow = time(0);

        netLinkTimeout =  timeOfNextNetCheck - timeNow;

        syslog(LOG_DEBUG, "netLinkTimeout:%lu\n", netLinkTimeout);

        bool netLinkEvent = false;

        try {
            netLinkEvent = devNetLink->readEvent(netLinkTimeout);
        } catch (exceptionLevel& el) {
            if (el.isFatal()) {
                shouldTerminate_.store(true, std::memory_order_relaxed);
                throw el;
            }

            netLinkEvent = true;
            syslog(LOG_ERR, "NetLink (non-fatal) exception: %s", el.what());
        } catch (...) {
            shouldTerminate_.store(true, std::memory_order_relaxed);
            throw;
        }

        if (netLinkEvent) {
            if (linkState_ != devNetLink->linkState()) {

                if (currentState_.load(std::memory_order_relaxed) != prevState_.load(std::memory_order_relaxed)) {
                    stateChangeTime = timeNow;
                }
            }
        }

        if (devNetLink->linkState() == NetLink::UP) {

            if (timeNow >= timeOfNextNetCheck) {
                try {
                    singlePing->pingGateway();
                } catch (pingException& pe) {
                    if (singlePing->state() == Ping::Fail) {
                        shouldTerminate_.store(true, std::memory_order_relaxed);
                        syslog(LOG_ERR, "Ping FAIL: %s", pe.what());
                    } else {

                    }
                } catch (exceptionLevel& el) {
                    if (el.isFatal()) {
                        shouldTerminate_.store(true, std::memory_order_relaxed);
                        throw el;
                    }

                    syslog(LOG_ERR, "Ping (non-fatal) exception: %s", el.what());
                } catch (std::exception& e) {
                    shouldTerminate_.store(true, std::memory_order_relaxed);
                    throw e;
                } catch (...) {
                    shouldTerminate_.store(true, std::memory_order_relaxed);
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

        if (netLinkEvent && (devNetLink->linkState() == NetLink::DOWN)) {
        }
    }
}

void NetMonitor::listener()
{
    subSocket_.connect(co2MainPubEndpoint);

    subSocket_.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    while (!shouldTerminate_.load(std::memory_order_relaxed)) {
        try {
            zmq::message_t msg;
            subSocket_.recv(&msg);
            std::string msg_str(static_cast<char*>(msg.data()), msg.size());
            co2Message::Co2Message co2Msg;

            if (!co2Msg.ParseFromString(msg_str)) {
                throw exceptionLevel("couldn't parse published message", false);
            }

            switch (co2Msg.messagetype()) {
            case co2Message::Co2Message_Co2MessageType_NET_CFG:
                if (co2Msg.has_netconfig()) {
                    const co2Message::NetConfig& netCfg = co2Msg.netconfig();
                    if (netCfg.has_netdevice()) {
                        netDevice_ = netCfg.netdevice();
                    } else {
                        throw exceptionLevel("missing net device", true);
                    }
                    if (netCfg.has_networkcheckperiod()) {
                        networkCheckPeriod_ = netCfg.networkcheckperiod();
                    } else {
                        throw exceptionLevel("missing network check period", true);
                    }
                    if (netCfg.has_netdevicedownrebootmintime()) {
                        netDeviceDownRebootMinTime_ = netCfg.netdevicedownrebootmintime();
                    } else {
                        throw exceptionLevel("missing net device down reboot time", true);
                    }

                    if (threadState_.load(std::memory_order_relaxed) == co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {
                        threadState_.store(co2Message::ThreadState_ThreadStates_STARTED, std::memory_order_relaxed);
                        threadStateChanged_.store(true, std::memory_order_relaxed);
                    }
                } else {
                    syslog(LOG_CRIT, "missing netMonitor netConfig");
                    threadState_.store(co2Message::ThreadState_ThreadStates_FAILED, std::memory_order_relaxed);
                    threadStateChanged_.store(true, std::memory_order_relaxed);
                }
                syslog(LOG_DEBUG, "NetMonitor config: NetDevice=\"%s\"  NetworkCheckPeriod=%lus  NetDeviceDownRebootMinTime=%lus",
                       netDevice_.c_str(), networkCheckPeriod_, netDeviceDownRebootMinTime_);
                break;

            case co2Message::Co2Message_Co2MessageType_TERMINATE:
                shouldTerminate_.store(true, std::memory_order_relaxed);
                break;

            default:
                // ignore other message types
                break;
            }

        } catch (exceptionLevel& el) {
            if (el.isFatal()) {
                syslog(LOG_ERR, "%s exception: %s", __FUNCTION__, el.what());
                shouldTerminate_.store(true, std::memory_order_relaxed);
            }
            syslog(LOG_ERR, "%s exception: %s", __FUNCTION__, el.what());
        } catch (...) {
            syslog(LOG_ERR, "%s unknown exception", __FUNCTION__);
        }
    }
}

void NetMonitor::sendNetState()
{
    std::string netStateStr;
    co2Message::Co2Message co2Msg;
    co2Message::NetState* netState = co2Msg.mutable_netstate();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_NET_STATE);

    netState->set_netstate(currentState_.load(std::memory_order_relaxed));

    co2Msg.SerializeToString(&netStateStr);

    zmq::message_t netStateMsg(netStateStr.size());
    memcpy (netStateMsg.data(), netStateStr.c_str(), netStateStr.size());

    mainSocket_.send(netStateMsg);
}

void NetMonitor::sendThreadState()
{
    std::string threadStateStr;
    co2Message::Co2Message co2Msg;
    co2Message::ThreadState* threadState = co2Msg.mutable_threadstate();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_THREAD_STATE);

    threadState->set_threadstate(threadState_.load(std::memory_order_relaxed));

    co2Msg.SerializeToString(&threadStateStr);

    zmq::message_t threadStateMsg(threadStateStr.size());
    memcpy (threadStateMsg.data(), threadStateStr.c_str(), threadStateStr.size());

    mainSocket_.send(threadStateMsg);
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

