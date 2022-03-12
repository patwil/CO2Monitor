/*
 * netMonitor.cpp
 *
 * Created on: 2016-02-06
 *     Author: patw
 */

#include <thread>
#include <syslog.h>
#include <dirent.h>

#include "netMonitor.h"
#include "ping.h"

#ifdef SYSTEMD_WDOG
#endif

NetMonitor::NetMonitor(zmq::context_t& ctx, int sockType) :
    ctx_(ctx),
    mainSocket_(ctx, sockType),
    subSocket_(ctx, ZMQ_SUB),
    stateChangeTime_(0),
    netDeviceDownTime_(0),
    netDownTime_(0)
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
    const char* loopbackDevice = "lo";

    // check if there are any network devices present (excluding loopback)
    if (pDir) {
        while ( (pDirEntry = readdir(pDir)) ) {
            if ( (pDirEntry->d_name[0] != '.') && strncmp(pDirEntry->d_name, loopbackDevice, strlen(loopbackDevice)+1) ) {
                event = NetDevicePresent;
                break;
            }

            // a network device is present - but not the one we're looking for
            event = NetDeviceMissing;
        }

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
                case NetDevicePresent:
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
                case NetDevicePresent:
                case NetUp:
                    break;

                case NetDown:
                    nextState = co2Message::NetState_NetStates_DOWN;
                    netDownTime_ = timeNow;
                    break;

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
                case NetDevicePresent:
                case NetDown:
                    break;

                case NetUp:
                    nextState = co2Message::NetState_NetStates_UP;
                    netDownTime_ = 0;
                    break;

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
                case NetDevicePresent:
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
                case NetDown:
                    if (!netDownTime_) {
                        netDownTime_ = timeNow;
                    }

                    nextState = co2Message::NetState_NetStates_DOWN;
                    break;

                case NetDevicePresent:
                case NetUp:
                    nextState = co2Message::NetState_NetStates_UP;
                    break;

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

        default:
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
    const int kAllowedPingFails = 5;

    /**************************************************************************/
    /*                                                                        */
    /* Start listener thread and await config                                 */
    /*                                                                        */
    /**************************************************************************/
    int terminatePipeFileDesc[2]; // 0: read    1: write
    if (pipe(terminatePipeFileDesc) < 0) {
        syslog(LOG_ERR, "Failed to create pipe for terminate ping");
        return;
    }
    std::thread* listenerThread = new std::thread(&NetMonitor::listener, this, terminatePipeFileDesc[1]);

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
        syslog(LOG_ERR, "NetMonitor failed to get config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }

    /**************************************************************************/
    /*                                                                        */
    /* Initialise network monitoring (if config received OK)                  */
    /*                                                                        */
    /**************************************************************************/

    // Only setup ping if we have got this far without
    // any trouble.
    //
    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STARTED) {
        try {

            singlePing = new Ping();
            singlePing->setAllowedFailCount(kAllowedPingFails);
            singlePing->setTerminateFd(terminatePipeFileDesc[0]);

            netFSM(NetDown);

        } catch (pingException& e) {

            syslog(LOG_ERR, "Ping fatal exception: %s", e.what());
            netFSM(NetDeviceFail);

        } catch (std::runtime_error& re) {

            syslog(LOG_ERR, "Ping exception: %s", re.what());
            netFSM(NetDeviceFail);

        } catch (std::bad_alloc& ba) {

            syslog(LOG_ERR, "Ping exception: %s", ba.what());
            netFSM(NetDeviceFail);

        } catch (...) {

            syslog(LOG_ERR, "Ping exception");
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
        syslog(LOG_ERR, "NetMonitor not Started, Stopping, Stopped or Failed");
        shouldTerminate = true;
    }

    /**************************************************************************/
    /*                                                                        */
    /* This is the main run loop.                                             */
    /*                                                                        */
    /**************************************************************************/
    time_t timeNow = time(0);
    time_t timeOfNextNetCheck = timeNow  + networkCheckPeriod_;

    while (!shouldTerminate) {
        timeNow = time(0);

        if (timeNow >= timeOfNextNetCheck) {
            try {
                singlePing->pingGateway();
                netFSM(NetUp);
            } catch (pingException& pe) {
                if (singlePing->state() == Ping::Fail) {
                    netFSM(NetDown);
                    syslog(LOG_ERR, "Ping FAIL: %s", pe.what());
                } else if (singlePing->state() == Ping::HwFail) {
                    netFSM(NetDeviceFail);
                    syslog(LOG_ERR, "Ping HWFAIL: %s", pe.what());
                }
            } catch (CO2::exceptionLevel& el) {
                if (el.isFatal()) {
                    threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                    syslog(LOG_ERR, "Ping Fatal Exception: %s", el.what());
                    break;
                } else {
                    syslog(LOG_ERR, "Ping (non-fatal) exception: %s", el.what());
                }
            } catch (std::exception& e) {
                threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                break;
            } catch (...) {
                threadState_->stateEvent(CO2::ThreadFSM::RunTimeFail);
                syslog(LOG_ERR, "Ping Exception");
                break;
            }

            timeNow = time(0);
            //
            // ping may take a while so we cannot assume that
            // less than a second has elapsed.
            timeOfNextNetCheck = timeNow + networkCheckPeriod_;
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

    listenerThread->join();
    DBG_TRACE_MSG("NetMonitor joined listenerThread");
    close(terminatePipeFileDesc[0]);
    close(terminatePipeFileDesc[1]);
    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STOPPING) {
        threadState_->stateEvent(CO2::ThreadFSM::Timeout);
    }
}

void NetMonitor::listener(int terminatePipeFd)
{
    DBG_TRACE();

    bool shouldTerminate = false;

    subSocket_.connect(CO2::co2MainPubEndpoint);
    subSocket_.set(zmq::sockopt::subscribe, "");

    while (!shouldTerminate) {
        try {
            zmq::message_t msg;

            if (subSocket_.recv(msg, zmq::recv_flags::none)) {

                std::string msg_str(static_cast<char*>(msg.data()), msg.size());
                co2Message::Co2Message co2Msg;

                if (!co2Msg.ParseFromString(msg_str)) {
                    throw CO2::exceptionLevel("couldn't parse published message", false);
                }

                DBG_MSG(LOG_DEBUG, "NetMonitor rx msg (type=%d)", co2Msg.messagetype());

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
            // send message to pipe, which will cause ping()
            // to return instead of waiting for timeout
            write(terminatePipeFd, "XXX", 3);
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

    mainSocket_.send(netStateMsg, zmq::send_flags::none);
}

void NetMonitor::getConfigFromMsg(co2Message::Co2Message& netCfgMsg)
{
    DBG_TRACE();

    if (netCfgMsg.has_netconfig()) {
        const co2Message::NetConfig& netCfg = netCfgMsg.netconfig();

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
        syslog(LOG_DEBUG, "NetMonitor config: NetworkCheckPeriod=%lus  "
               "NetDeviceDownRebootMinTime=%lus  NetDownRebootMinTime=%lus",
               networkCheckPeriod_, netDeviceDownRebootMinTime_, netDownRebootMinTime_);

    } else {
        syslog(LOG_ERR, "missing netMonitor netConfig");
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

