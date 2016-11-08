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

#ifdef SYSTEMD_WDOG
#include "sysdWatchdog.h"
#endif

NetMonitor::NetMonitor(zmq::context_t& ctx, int sockType) :
    ctx_(ctx),
    mainSocket_(ctx, sockType),
    subSocket_(ctx, ZMQ_SUB),
    stateChangeTime_(0),
    netDeviceDownTime_(0),
    netDownTime_(0),
    linkState_(NetLink::DOWN)
{
    netState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
    threadState_ = new CO2::ThreadFSM("NetMonitor", &mainSocket_);
}

NetMonitor::~NetMonitor()
{
    // Delete all dynamic memory.
    delete threadState_;
}

NetMonitor::StateEvent NetMonitor::checkNetInterfacesPresent()
{
    DBG_TRACE();

    DIR* pDir;
    struct dirent* pDirEntry;
    StateEvent event = NoNetDevices;

    pDir = opendir("/sys/class/net");

    if (pDir) {
        while ( (pDirEntry = readdir(pDir)) ) {
            //
            if (!strncmp(pDirEntry->d_name, netDevice_.c_str(), netDevice_.length())) {
                event = LinkUp;
                break;
            }
            // a network device is present - but not the one we're looking for
            event = NetDeviceMissing;
        }
        //
        closedir(pDir);
    }

    return event;
}

void NetMonitor::netFSM(NetMonitor::StateEvent event)
{
    DBG_TRACE();

    co2Message::NetState_NetStates currentState = netState_.load(std::memory_order_relaxed);
    co2Message::NetState_NetStates nextState = currentState;
    time_t timeNow = time(0);

    switch (currentState) {

    case co2Message::NetState_NetStates_START:
        switch (event) {
        case LinkUp:
        case LinkDown:
        case NetDown:
            nextState = co2Message::NetState_NetStates_DOWN;
            break;
        case NetUp:
            nextState = co2Message::NetState_NetStates_UP;
            break;
        case NetDeviceMissing:
            nextState = co2Message::NetState_NetStates_MISSING;
            break;
        case NetDeviceFail:
        case NoNetDevices:
            nextState = co2Message::NetState_NetStates_NO_NET_INTERFACE;
            break;
        case Timeout:
            break;
        }
        break;

    case co2Message::NetState_NetStates_UP:
        switch (event) {
        case LinkUp:
        case NetUp:
            break;
        case NetDown:
            nextState = co2Message::NetState_NetStates_DOWN;
            netDownTime_ = timeNow;
            break;
        case LinkDown:
        case NetDeviceMissing:
            nextState = co2Message::NetState_NetStates_MISSING;
            netDeviceDownTime_ = timeNow;
            break;
        case NetDeviceFail:
        case NoNetDevices:
            nextState = co2Message::NetState_NetStates_NO_NET_INTERFACE;
            netDeviceDownTime_ = timeNow;
            break;
        case Timeout:
            break;
        }
        break;

    case co2Message::NetState_NetStates_DOWN:
        switch (event) {
        case LinkUp:
        case NetDown:
            break;
        case NetUp:
            nextState = co2Message::NetState_NetStates_UP;
            netDownTime_ = 0;
            break;
        case LinkDown:
        case NetDeviceMissing:
            nextState = co2Message::NetState_NetStates_MISSING;
            netDeviceDownTime_ = timeNow;
            break;
        case NetDeviceFail:
        case NoNetDevices:
            nextState = co2Message::NetState_NetStates_NO_NET_INTERFACE;
            netDeviceDownTime_ = timeNow;
            break;
        case Timeout:
            nextState = co2Message::NetState_NetStates_FAILED;
            netDeviceDownTime_ = timeNow;
            break;
        }
        break;

    case co2Message::NetState_NetStates_FAILED:
        // We're past redemption once we have failed...
        break;

    case co2Message::NetState_NetStates_MISSING:
        switch (event) {
        case LinkUp:
            netDeviceDownTime_ = 0;
            netDownTime_ = timeNow;
            nextState = co2Message::NetState_NetStates_DOWN;
            break;
        case NetUp:
            netDeviceDownTime_ = 0;
            netDownTime_ = 0;
            nextState = co2Message::NetState_NetStates_UP;
            break;
        case NetDown:
            if (!netDownTime_) {
                netDownTime_ = timeNow;
            }
            break;
        case LinkDown:
        case NetDeviceFail:
        case NetDeviceMissing:
            if (!netDeviceDownTime_) {
                netDeviceDownTime_ = timeNow;
            }
            break;
        case NoNetDevices:
            nextState = co2Message::NetState_NetStates_NO_NET_INTERFACE;
            break;
        case Timeout:
            nextState = co2Message::NetState_NetStates_FAILED;
            break;
        }
        break;

    case co2Message::NetState_NetStates_NO_NET_INTERFACE:
        switch (event) {
        case LinkUp:
            netDeviceDownTime_ = 0;
            // fall through to...
        case NetDown:
            if (!netDownTime_) {
                netDownTime_ = timeNow;
            }
            nextState = co2Message::NetState_NetStates_DOWN;
            break;
        case NetUp:
            nextState = co2Message::NetState_NetStates_UP;
            break;
        case LinkDown:
        case NetDeviceMissing:
            if (!netDeviceDownTime_) {
                netDeviceDownTime_ = timeNow;
            }
            nextState = co2Message::NetState_NetStates_MISSING;
            break;
        case NetDeviceFail:
        case NoNetDevices:
            if (!netDeviceDownTime_) {
                netDeviceDownTime_ = timeNow;
            }
            break;
        case Timeout:
            break;
        }
        break;
    }

    if (currentState != nextState) {
        syslog(LOG_INFO, "NetState change from %s to %s", netStateStr(currentState), netStateStr(nextState));
        netState_.store(nextState, std::memory_order_relaxed);
        stateChangeTime_ = timeNow;
        sendNetState();
    }
}

const char* NetMonitor::netStateStr()
{
    return netStateStr(netState_.load(std::memory_order_relaxed));
}

const char* NetMonitor::netStateStr(co2Message::NetState_NetStates netState)
{
    switch (netState) {
    case co2Message::NetState_NetStates_START:
        return "START";
    case co2Message::NetState_NetStates_UP:
        return "UP";
    case co2Message::NetState_NetStates_DOWN:
        return "DOWN";
    case co2Message::NetState_NetStates_FAILED:
        return "FAILED";
    case co2Message::NetState_NetStates_MISSING:
        return "MISSING";
    case co2Message::NetState_NetStates_NO_NET_INTERFACE:
        return "NO_NET_INTERFACE";
    default:
        return "Unknown Net State";
    }
}

void NetMonitor::run()
{
    DBG_TRACE_MSG("Start of NetMonitor::run");

    bool shouldTerminate = false;
    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    Ping* singlePing = 0;
    NetLink* devNetLink = 0;
    const int kAllowedPingFails = 5;

    /**************************************************************************/
    /*                                                                        */
    /* Start listener thread and await config                                 */
    /*                                                                        */
    /**************************************************************************/
    std::thread* listenerThread = new std::thread(std::bind(&NetMonitor::listener, this));

    // mainSocket is used to send status to main thread
    mainSocket_.connect(CO2::netMonEndpoint);

    threadState_->stateEvent(CO2::ThreadFSM::ReadyForConfig);
    if (threadState_->stateChanged()) {
        myThreadState = threadState_->state();
    }

    // We'll continue after receiving our configuration
    while (!threadState_->stateChanged()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    myThreadState = threadState_->state();
    syslog(LOG_DEBUG, "netMon state=%s", CO2::stateStr(myThreadState));

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STARTED) {

        netFSM(checkNetInterfacesPresent());

        switch (netState_.load(std::memory_order_relaxed)) {

        case co2Message::NetState_NetStates_FAILED:
        case co2Message::NetState_NetStates_NO_NET_INTERFACE:
            syslog (LOG_ERR, "No network interface present");
            threadState_->stateEvent(CO2::ThreadFSM::InitFail);
            break;

        default:
                break;
        }
    } else {
        syslog(LOG_CRIT, "NetMonitor failed to get config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }

    /**************************************************************************/
    /*                                                                        */
    /* Initialise network monitoring (if config received OK)                  */
    /*                                                                        */
    /**************************************************************************/

    // Only setup netLink and ping if we have got this far without
    // any trouble.
    //
    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STARTED) {
        try {

            singlePing = new Ping();
            singlePing->setAllowedFailCount(kAllowedPingFails);

            // We only monitor netLink if our named network device is present
            if (netState_.load(std::memory_order_relaxed) != co2Message::NetState_NetStates_MISSING) {
                devNetLink = new NetLink(netDevice_.c_str());
                devNetLink->open();
                linkState_ = devNetLink->linkState();
            } else {
                linkState_ = NetLink::DOWN;
            }
            syslog(LOG_DEBUG, "NetMonitor: link state is:%s", (linkState_ == NetLink::UP) ? "UP" : "DOWN");

            netFSM(NetDown);

        } catch (CO2::exceptionLevel& el) {

            if (el.isFatal()) {
                syslog(LOG_CRIT, "NetLink fatal exception: %s", el.what());
                netFSM(NetDeviceFail);
                threadState_->stateEvent(CO2::ThreadFSM::InitFail);
            } else {
                syslog(LOG_ERR, "NetLink (non-fatal) exception: %s", el.what());
            }

        } catch (pingException& e) {

            syslog(LOG_CRIT, "Ping fatal exception: %s", e.what());
            netFSM(NetDeviceFail);

        } catch (std::runtime_error& re) {

            syslog(LOG_CRIT, "Ping or NetLink exception: %s", re.what());
            netFSM(NetDeviceFail);

        } catch (std::bad_alloc& ba) {

            syslog(LOG_CRIT, "Ping or NetLink exception: %s", ba.what());
            netFSM(NetDeviceFail);

        } catch (...) {

            syslog(LOG_CRIT, "Ping or NetLink exception");
            netFSM(NetDeviceFail);
        }

        switch (netState_.load(std::memory_order_relaxed)) {

        case co2Message::NetState_NetStates_FAILED:
            syslog (LOG_ERR, "Network interface initialisation failure");
            threadState_->stateEvent(CO2::ThreadFSM::InitFail);
            break;

        case co2Message::NetState_NetStates_NO_NET_INTERFACE:
            syslog (LOG_ERR, "No network interface present");
            threadState_->stateEvent(CO2::ThreadFSM::InitFail);
            break;

        default:
                break;
        }
    }

    myThreadState = threadState_->state();

    if (myThreadState == co2Message::ThreadState_ThreadStates_STARTED) {
        // we are now ready to roll
        threadState_->stateEvent(CO2::ThreadFSM::InitOk);
    } else if ( (myThreadState == co2Message::ThreadState_ThreadStates_STOPPING) ||
                (myThreadState == co2Message::ThreadState_ThreadStates_STOPPED) ||
                (myThreadState == co2Message::ThreadState_ThreadStates_FAILED) ) {
        shouldTerminate = true;
    } else {
        syslog(LOG_CRIT, "NetMonitor not Started, Stopping, Stopped or Failed");
        shouldTerminate = true;
    }

    /**************************************************************************/
    /*                                                                        */
    /* This is the main run loop.                                             */
    /*                                                                        */
    /**************************************************************************/
    time_t timeNow = time(0);
    time_t timeOfNextNetCheck = timeNow  + networkCheckPeriod_;
    time_t netLinkTimeout = 0;

    while (!shouldTerminate) {
        timeNow = time(0);

        netLinkTimeout =  (timeOfNextNetCheck > timeNow) ? (timeOfNextNetCheck - timeNow) : 0;

        if (devNetLink) {

            bool netLinkEvent = false;

            try {
                netLinkEvent = devNetLink->readEvent(netLinkTimeout);
            } catch (CO2::exceptionLevel& el) {
                if (el.isFatal()) {
                    threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                    break;
                }

                netLinkEvent = true;
                syslog(LOG_ERR, "NetLink (non-fatal) exception: %s", el.what());
            } catch (...) {
                threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                break;
            }

            if (netLinkEvent || devNetLink->linkStateChanged()) {
                linkState_ = devNetLink->linkState();
            }
            syslog(LOG_DEBUG, "NetMonitor: link state is:%s", (linkState_ == NetLink::UP) ? "UP" : "DOWN");
        } else {
            // Named network device is not present, so we'll sleep
            // for the duration instead of waiting for netLink event
            std::this_thread::sleep_for(std::chrono::seconds(netLinkTimeout));
        }

        timeNow = time(0);

        if (timeNow >= timeOfNextNetCheck) {
            try {
                singlePing->pingGateway();
                netFSM(NetUp);
            } catch (pingException& pe) {
                if (singlePing->state() == Ping::Fail) {
                    netFSM(NetDown);
                    syslog(LOG_ERR, "Ping FAIL: %s", pe.what());
                }
            } catch (CO2::exceptionLevel& el) {
                if (el.isFatal()) {
                    threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                    syslog(LOG_CRIT, "Ping Fatal Exception: %s", el.what());
                    break;
                } else {
                    syslog(LOG_ERR, "Ping (non-fatal) exception: %s", el.what());
                }
            } catch (std::exception& e) {
                threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                break;
            } catch (...) {
                threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                syslog(LOG_CRIT, "Ping Exception");
                break;
            }

            timeNow = time(0);
            //
            // ping may take a while so we cannot assume that
            // less than a second has elapsed.
            timeOfNextNetCheck = timeNow + networkCheckPeriod_;
        }

        // devNetLink is null if we are not monitoring named network device
        // because it is not present.
        //
        if ((linkState_ == NetLink::UP) || (devNetLink == nullptr) ) {
            // netDownTime_ will be non-zero if ping has failed
            if ( netDownTime_ && ((timeNow - netDownTime_) >= netDownRebootMinTime_) ) {
                netFSM(Timeout);
            }
        } else {
            // named network device was present, but is now down.
            //
            // netDeviceDownTime_ will be non-zero if network device is down or missing
            if ( netDeviceDownTime_ && ((timeNow - netDeviceDownTime_) >= netDeviceDownRebootMinTime_) ) {
                netFSM(Timeout);
            }
        }

        switch (netState_.load(std::memory_order_relaxed)) {

        case co2Message::NetState_NetStates_FAILED:
        case co2Message::NetState_NetStates_NO_NET_INTERFACE:
            syslog (LOG_ERR, "No network interface present");
            threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
            break;

        default:
                break;
        }

        if (threadState_->state() != co2Message::ThreadState_ThreadStates_RUNNING) {
            shouldTerminate = true;
        }

    }
    /**************************************************************************/
    /*                                                                        */
    /* end of main run loop.                                                  */
    /*                                                                        */
    /**************************************************************************/
    DBG_TRACE_MSG("end of NetMonitor::run loop");

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STOPPING) {
        threadState_->stateEvent(CO2::ThreadFSM::Timeout);
    }
}

void NetMonitor::listener()
{
    DBG_TRACE();

    bool shouldTerminate = false;

    subSocket_.connect(CO2::co2MainPubEndpoint);
    subSocket_.setsockopt(ZMQ_SUBSCRIBE, "", 0);

    while (!shouldTerminate) {
        try {
            zmq::message_t msg;
            if (subSocket_.recv(&msg)) {

                std::string msg_str(static_cast<char*>(msg.data()), msg.size());
                co2Message::Co2Message co2Msg;

                if (!co2Msg.ParseFromString(msg_str)) {
                    throw CO2::exceptionLevel("couldn't parse published message", false);
                }
                syslog(LOG_DEBUG, "NetMonitor rx msg (type=%d)", co2Msg.messagetype());

                switch (co2Msg.messagetype()) {
                case co2Message::Co2Message_Co2MessageType_NET_CFG:
                    getConfigFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_TERMINATE:
                    threadState_->stateEvent(CO2::ThreadFSM::Terminate);
                    break;

                default:
                    // ignore other message types
                    break;
                }

            }
        } catch (CO2::exceptionLevel& el) {
            if (el.isFatal()) {
                syslog(LOG_ERR, "%s exception: %s", __FUNCTION__, el.what());
            }
            syslog(LOG_ERR, "%s exception: %s", __FUNCTION__, el.what());
        } catch (...) {
            syslog(LOG_ERR, "%s unknown exception", __FUNCTION__);
        }

        co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

        if ( (myThreadState == co2Message::ThreadState_ThreadStates_STOPPING) ||
             (myThreadState == co2Message::ThreadState_ThreadStates_STOPPED) ||
             (myThreadState == co2Message::ThreadState_ThreadStates_FAILED) ) {
            shouldTerminate = true;
        }
    }
}

void NetMonitor::sendNetState()
{
    DBG_TRACE();
    std::string netStateStr;
    co2Message::Co2Message co2Msg;
    co2Message::NetState* netState = co2Msg.mutable_netstate();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_NET_STATE);

    netState->set_netstate(netState_.load(std::memory_order_relaxed));

    co2Msg.SerializeToString(&netStateStr);

    zmq::message_t netStateMsg(netStateStr.size());
    memcpy(netStateMsg.data(), netStateStr.c_str(), netStateStr.size());

    mainSocket_.send(netStateMsg);
}

void NetMonitor::getConfigFromMsg(co2Message::Co2Message& netCfgMsg)
{
    DBG_TRACE();

    if (netCfgMsg.has_netconfig()) {
        const co2Message::NetConfig& netCfg = netCfgMsg.netconfig();

        if (netCfg.has_netdevice()) {
            netDevice_ = netCfg.netdevice();
        } else {
            throw CO2::exceptionLevel("missing net device", true);
        }

        if (netCfg.has_networkcheckperiod()) {
            networkCheckPeriod_ = netCfg.networkcheckperiod();
        } else {
            throw CO2::exceptionLevel("missing network check period", true);
        }

        if (netCfg.has_netdevicedownrebootmintime()) {
            netDeviceDownRebootMinTime_ = netCfg.netdevicedownrebootmintime();
        } else {
            throw CO2::exceptionLevel("missing net device down reboot time", true);
        }

        if (netCfg.has_netdownrebootmintime()) {
            netDownRebootMinTime_ = netCfg.netdownrebootmintime();
        } else {
            throw CO2::exceptionLevel("missing net down reboot time", true);
        }

        threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
        syslog(LOG_DEBUG, "NetMonitor config: NetDevice=\"%s\"  NetworkCheckPeriod=%lus  "
                          "NetDeviceDownRebootMinTime=%lus  NetDownRebootMinTime=%lus",
                          netDevice_.c_str(), networkCheckPeriod_, netDeviceDownRebootMinTime_, netDownRebootMinTime_);

    } else {
        syslog(LOG_CRIT, "missing netMonitor netConfig");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
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

