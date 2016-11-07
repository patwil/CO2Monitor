/*
 * networkMonitor.cpp
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#include <iostream>
#include <thread>         // std::thread
#include <mutex>          // std::mutex
#include <vector>
#include <atomic>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
//#include <linux/reboot.h>
#include <sys/reboot.h>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include <zmq.hpp>

#include "netMonitor.h"
#include "restartMgr.h"
#include "config.h"
#include "parseConfigFile.h"
#include "co2Defaults.h"
#include "utils.h"

#include "sysdWatchdog.h"

class Co2Main
{
private:
    typedef enum {
        OK,
        NoFail = OK,
        SoftwareFail,
        HardwareFail
    } FailType;


    typedef enum {
        UserReq,
        SignalReceived,
        FatalException
    } TerminateReasonType;

    typedef enum {
        Restart,
        Reboot,
        Shutdown
    } UserReqType;

    ConfigMap& cfg_;

    void publishCo2Cfg();
    void readCo2CfgMsg(std::string& cfgStr, bool bPublish);
    void readMsgFromCo2Monitor();

    void netMonFSM();
    void publishNetCfg();
    void readMsgFromNetMonitor();

    void publishUICfg();

    void publishFanCfg();

    void publishAllConfig();
    void publishNetState();

    void terminateAllThreads();

    void listener();

    void threadStateChangeNotify(co2Message::ThreadState_ThreadStates threadState, const char* threadName);

    zmq::context_t context_;
    int zSockType_;
    zmq::socket_t mainPubSkt_;
    zmq::socket_t netMonSkt_;

    //std::mutex mutex_; // used to control access to attributes used by multiple threads

    std::atomic<co2Message::NetState_NetStates> netState_;
    std::atomic<co2Message::NetState_NetStates> newNetState_;
    std::atomic<bool> netStateChanged_;

    std::atomic<bool> threadStateChanged_; // one or more threads have changed state
    CO2::ThreadFSM* myThreadState_;

    std::atomic<co2Message::ThreadState_ThreadStates> netMonThreadState_;
    std::atomic<co2Message::ThreadState_ThreadStates> co2MonThreadState_;
    std::atomic<co2Message::ThreadState_ThreadStates> uiThreadState_;

    long rxTimeoutMsec_;
    time_t startTimeoutSec_;

    RestartMgr* restartMgr_;
    static std::atomic<bool> shouldTerminate_;

    static Co2Main::FailType failType_;
    static Co2Main::TerminateReasonType terminateReason_;
    static Co2Main::UserReqType userReqType_;

public:
    Co2Main(ConfigMap& cfg) :
        cfg_(cfg),
        context_(1),
        zSockType_(ZMQ_PAIR),
        mainPubSkt_(context_, ZMQ_PUB),
        netMonSkt_(context_, zSockType_)
    {
        shouldTerminate_.store(false, std::memory_order_relaxed);

        myThreadState_ = new CO2::ThreadFSM("Co2MonitorMain");
        netState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
        newNetState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
        netStateChanged_.store(false, std::memory_order_relaxed);
        threadStateChanged_.store(false, std::memory_order_relaxed);
        netMonThreadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);
        co2MonThreadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);
        uiThreadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);

        restartMgr_ = new RestartMgr;

        // If an uncaught exception occurs it will probably
        // be because of a software error and be fatal -
        // so we'll use these as defaults for terminating
        // this program.
        //
        failType_ = SoftwareFail;
        terminateReason_ = FatalException;
        userReqType_ = Restart;

        mainPubSkt_.bind(CO2::co2MainPubEndpoint);
        netMonSkt_.bind(CO2::netMonEndpoint);

        // set up signal handler. We only have one
        // to handle all trapped signals
        //
        struct sigaction action;
        //
        action.sa_handler = Co2Main::sigHandler;
        action.sa_flags = 0;
        sigemptyset(&action.sa_mask);
        sigaction(SIGHUP, &action, 0);
        sigaction(SIGINT, &action, 0);
        sigaction(SIGQUIT, &action, 0);
        sigaction(SIGTERM, &action, 0);
    }

    ~Co2Main()
    {
        if (restartMgr_) {
            delete restartMgr_;
        }
    }

    int readConfigFile(const char* pFilename);

    void init(const char* progName) {
        restartMgr_->init(progName);
    }

    void runloop();

    void terminate();

    static void sigHandler(int sig);
    static const char* failTypeStr();
    static const char* terminateReasonStr();
    static const char* userReqTypeStr();
};

std::atomic<bool> Co2Main::shouldTerminate_;

Co2Main::FailType Co2Main::failType_;
Co2Main::TerminateReasonType Co2Main::terminateReason_;
Co2Main::UserReqType Co2Main::userReqType_;

const char* Co2Main::failTypeStr()
{
    switch (Co2Main::failType_) {
    case Co2Main::FailType::OK:
        return "OK";
    case Co2Main::FailType::SoftwareFail:
        return "SoftwareFail";
    case Co2Main::FailType::HardwareFail:
        return "HardwareFail";
    default:
        return "Unknown Fail Type";
    }
}

const char* Co2Main::terminateReasonStr()
{
    switch (Co2Main::terminateReason_) {
    case Co2Main::TerminateReasonType::UserReq:
        return "UserReq";
    case Co2Main::TerminateReasonType::SignalReceived:
        return "SignalReceived";
    case Co2Main::TerminateReasonType::FatalException:
        return "FatalException";
    default:
        return "Unknown Terminate Reason Type";
    }
}

const char* Co2Main::userReqTypeStr()
{
    switch (Co2Main::userReqType_) {
    case Co2Main::UserReqType::Restart:
        return "Restart";
    case Co2Main::UserReqType::Reboot:
        return "Reboot";
    case Co2Main::UserReqType::Shutdown:
        return "Shutdown";
    default:
        return "Unknown User Req Type";
    }
}

int Co2Main::readConfigFile(const char* pFilename)
{
    DBG_TRACE();

    int rc = 0;
    std::string cfgLine;
    std::string key;
    std::string val;
    std::ifstream inFile(pFilename);
    int lineNumber = 0;
    int errorCount = 0;

    if (inFile.fail()) {
        syslog (LOG_ERR, "unable to open config file: \"%s\"", pFilename);
        return -2;
    }

    while (inFile) {
        getline(inFile, cfgLine, '\n');
        ++lineNumber;

        bool valIsQuoted;
        rc = CO2::parseStringForKeyAndValue(cfgLine, key, val, &valIsQuoted);

        if (rc > 0) {
            ConfigMapCI entry = cfg_.find(key);

            if (entry != cfg_.end()) {
                Config* pCfg = entry->second;

                // Set config value based on perceived type (double, integer or char string)
                // Quoted numbers are treated as char strings
                if (isdigit(val[0]) && !valIsQuoted) {
                    if (strchr(val.c_str(), '.')) {
                        pCfg->set(stod(val, 0));
                    } else {
                        pCfg->set(stoi(val, 0, 0));
                    }
                } else {
                    pCfg->set(val);
                }
            }
        } else if (rc < 0) {
            syslog (LOG_ERR, "Error in config file \"%s\":%u", pFilename, lineNumber);

            if (++errorCount > 3) {
                syslog (LOG_ERR, "Too many errors in config file - bailing...");
                rc = -1;
                break;
            }
        }
    }

    inFile.close();

    if (rc < 0) {
        return rc;
    }

    return lineNumber;
}

void Co2Main::publishCo2Cfg()
{
    DBG_TRACE();

    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::Co2Config* co2Cfg = co2Msg.mutable_co2config();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_CO2_CFG);

    if (cfg_.find("CO2Port") != cfg_.end()) {
        co2Cfg->set_co2port(cfg_.find("CO2Port")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing CO2Port config");
    }

    if (configIsOk) {
        std::string cfgStr;
        co2Msg.SerializeToString(&cfgStr);

        zmq::message_t configMsg(cfgStr.size());

        memcpy(configMsg.data(), cfgStr.c_str(), cfgStr.size());
        mainPubSkt_.send(configMsg);
        syslog(LOG_DEBUG, "sent Co2 config");
    } else {
        throw CO2::exceptionLevel("Missing Co2Config", true);
    }
}

void Co2Main::readCo2CfgMsg(std::string& cfgStr, bool bPublish = false)
{
}


void Co2Main::netMonFSM()
{
    DBG_TRACE();

    co2Message::NetState_NetStates newNetState = newNetState_.load(std::memory_order_relaxed);
    co2Message::NetState_NetStates netState = netState_.load(std::memory_order_relaxed);

    if (newNetState == netState) {
        return;
    }

    switch (newNetState) {

    case co2Message::NetState_NetStates_START:
        break;

    case co2Message::NetState_NetStates_UP:
    case co2Message::NetState_NetStates_DOWN:
        netState_.store(newNetState, std::memory_order_relaxed);
        publishNetState();
        break;

    case co2Message::NetState_NetStates_MISSING:
        netState_.store(newNetState, std::memory_order_relaxed);
        publishNetState();
        break;

    case co2Message::NetState_NetStates_FAILED:
    case co2Message::NetState_NetStates_NO_NET_INTERFACE:
        netState_.store(newNetState, std::memory_order_relaxed);
        shouldTerminate_.store(true, std::memory_order_relaxed);
        failType_ = HardwareFail;
        terminateReason_ = FatalException;
        break;

    default:
        syslog (LOG_ERR, "%s: Unknown NetState (%d)", __FUNCTION__, newNetState);
        break;
    }
}

void Co2Main::publishNetCfg()
{
    DBG_TRACE();

    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::NetConfig* netCfg = co2Msg.mutable_netconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_NET_CFG);

    if (cfg_.find("NetDevice") != cfg_.end()) {
        netCfg->set_netdevice(cfg_.find("NetDevice")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDevice config");
    }

    if (cfg_.find("NetworkCheckPeriod") != cfg_.end()) {
        netCfg->set_networkcheckperiod(cfg_.find("NetworkCheckPeriod")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetworkCheckPeriod config");
    }

    if (cfg_.find("NetDeviceDownRebootMinTime") != cfg_.end()) {
        netCfg->set_netdevicedownrebootmintime(cfg_.find("NetDeviceDownRebootMinTime")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDeviceDownRebootMinTime config");
    }

    if (cfg_.find("NetDownRebootMinTime") != cfg_.end()) {
        netCfg->set_netdownrebootmintime(cfg_.find("NetDownRebootMinTime")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing NetDownRebootMinTime config");
    }

    if (configIsOk) {
        std::string cfgStr;
        co2Msg.SerializeToString(&cfgStr);

        zmq::message_t configMsg(cfgStr.size());

        memcpy(configMsg.data(), cfgStr.c_str(), cfgStr.size());
        mainPubSkt_.send(configMsg);
        syslog(LOG_DEBUG, "sent Net config");
    } else {
        throw CO2::exceptionLevel("Missing NetConfig", true);
    }
}

void Co2Main::readMsgFromNetMonitor()
{
    DBG_TRACE();

    try {
        zmq::message_t msg;
        if (netMonSkt_.recv(&msg)) {

            std::string msg_str(static_cast<char*>(msg.data()), msg.size());

            co2Message::Co2Message co2Msg;

            if (!co2Msg.ParseFromString(msg_str)) {
                throw CO2::exceptionLevel("couldn't parse message from netMonitor", false);
            }

            switch (co2Msg.messagetype()) {
            case co2Message::Co2Message_Co2MessageType_NET_STATE:
                if (co2Msg.has_netstate()) {
                    const co2Message::NetState& netStateMsg = co2Msg.netstate();
                    if (netState_.load(std::memory_order_relaxed) != netStateMsg.netstate()) {
                        // handle netMon state change
                        //
                        newNetState_.store(netStateMsg.netstate(), std::memory_order_relaxed);
                        netStateChanged_.store(true, std::memory_order_relaxed);
                    }
                } else {
                    throw CO2::exceptionLevel("missing netMonitor netState", false);
                }
                break;
            case co2Message::Co2Message_Co2MessageType_THREAD_STATE:
                if (co2Msg.has_threadstate()) {
                    const co2Message::ThreadState& threadStateMsg = co2Msg.threadstate();
                    if (netMonThreadState_.load(std::memory_order_relaxed) != threadStateMsg.threadstate()) {
                        netMonThreadState_.store(threadStateMsg.threadstate(), std::memory_order_relaxed);
                        threadStateChanged_.store(true, std::memory_order_relaxed);
                        syslog(LOG_DEBUG, "%s: rx thread State=%s", __FUNCTION__,
                                          CO2::stateStr(netMonThreadState_.load(std::memory_order_relaxed)));
                    }
                } else {
                    throw CO2::exceptionLevel("missing netMonitor threadState", false);
                }
                break;
            default:
                throw CO2::exceptionLevel("unexpected message from netMonitor", false);
            }
        }

    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            throw el;
        }
        syslog(LOG_ERR, "%s exception: %s", __FUNCTION__, el.what());
    } catch (...) {
        syslog(LOG_ERR, "%s unknown exception", __FUNCTION__);
    }
}

void Co2Main::publishUICfg()
{
    DBG_TRACE();

    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::UIConfig* uiCfg = co2Msg.mutable_uiconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_UI_CFG);

    if (cfg_.find("SDL_FBDEV") != cfg_.end()) {
        uiCfg->set_fbdev(cfg_.find("SDL_FBDEV")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_FBDEV config");
    }

    if (cfg_.find("SDL_MOUSEDEV") != cfg_.end()) {
        uiCfg->set_mousedev(cfg_.find("SDL_MOUSEDEV")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSEDEV config");
    }

    if (cfg_.find("SDL_MOUSEDRV") != cfg_.end()) {
        uiCfg->set_mousedrv(cfg_.find("SDL_MOUSEDRV")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSEDRV config");
    }

#if 1
    if (cfg_.find("SDL_MOUSE_RELATIVE") != cfg_.end()) {
        uiCfg->set_mouserelative(cfg_.find("SDL_MOUSE_RELATIVE")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSE_RELATIVE config");
    }
#endif

    if (cfg_.find("SDL_VIDEODRIVER") != cfg_.end()) {
        uiCfg->set_videodriver(cfg_.find("SDL_VIDEODRIVER")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_VIDEODRIVER config");
    }

    if (cfg_.find("SDL_TTF_DIR") != cfg_.end()) {
        uiCfg->set_ttfdir(cfg_.find("SDL_TTF_DIR")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_TTF_DIR config");
    }

    if (cfg_.find("SDL_BMP_DIR") != cfg_.end()) {
        uiCfg->set_bitmapdir(cfg_.find("SDL_BMP_DIR")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_BMP_DIR config");
    }

    if (cfg_.find("ScreenRefreshRate") != cfg_.end()) {
        uiCfg->set_screenrefreshrate(cfg_.find("ScreenRefreshRate")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing ScreenRefreshRate config");
    }

    if (cfg_.find("ScreenTimeout") != cfg_.end()) {
        uiCfg->set_screentimeout(cfg_.find("ScreenTimeout")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing ScreenTimeout config");
    }

    if (configIsOk) {
        std::string cfgStr;
        co2Msg.SerializeToString(&cfgStr);

        zmq::message_t configMsg(cfgStr.size());

        memcpy(configMsg.data(), cfgStr.c_str(), cfgStr.size());
        mainPubSkt_.send(configMsg);
        syslog(LOG_DEBUG, "sent UI config");
    } else {
        throw CO2::exceptionLevel("Missing UIConfig", true);
    }
}

void Co2Main::publishFanCfg()
{
    DBG_TRACE();

    bool configIsOk = true;
    co2Message::Co2Message co2Msg;

    co2Message::FanConfig* fanCfg = co2Msg.mutable_fanconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_FAN_CFG);

    if (cfg_.find("FanOnOverrideTime") != cfg_.end()) {
        fanCfg->set_fanonoverridetime(cfg_.find("FanOnOverrideTime")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing FanOnOverrideTime config");
    }

    if (cfg_.find("RelHumFanOnThreshold") != cfg_.end()) {
        fanCfg->set_relhumfanonthreshold(cfg_.find("RelHumFanOnThreshold")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing RelHumFanOnThreshold config");
    }

    if (cfg_.find("CO2FanOnThreshold") != cfg_.end()) {
        fanCfg->set_co2fanonthreshold(cfg_.find("CO2FanOnThreshold")->second->getInt());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing CO2FanOnThreshold config");
    }

    if (configIsOk) {
        std::string cfgStr;
        co2Msg.SerializeToString(&cfgStr);

        zmq::message_t configMsg(cfgStr.size());

        memcpy(configMsg.data(), cfgStr.c_str(), cfgStr.size());
        mainPubSkt_.send(configMsg);
        syslog(LOG_DEBUG, "sent Fan config");
    } else {
        throw CO2::exceptionLevel("Missing Fan Config", true);
    }
}

void Co2Main::publishAllConfig()
{
    DBG_TRACE();

    publishCo2Cfg();

    publishNetCfg();

    publishUICfg();

    publishFanCfg();
}

void Co2Main::publishNetState()
{
    DBG_TRACE();

    std::string netStateStr;
    co2Message::Co2Message co2Msg;
    co2Message::NetState* netState = co2Msg.mutable_netstate();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_NET_STATE);

    netState->set_netstate(netState_.load(std::memory_order_relaxed));

    co2Msg.SerializeToString(&netStateStr);

    zmq::message_t netStateMsg(netStateStr.size());

    memcpy (netStateMsg.data(), netStateStr.c_str(), netStateStr.size());
    mainPubSkt_.send(netStateMsg);
    syslog(LOG_DEBUG, "Published Net State");
}

void Co2Main::terminateAllThreads()
{
    DBG_TRACE();

    std::string terminateMsgStr;
    co2Message::Co2Message co2Msg;
    co2Message::ThreadState* threadState = co2Msg.mutable_threadstate();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_TERMINATE);

    threadState->set_threadstate(co2Message::ThreadState_ThreadStates_STOPPING);

    co2Msg.SerializeToString(&terminateMsgStr);

    zmq::message_t pubMsg(terminateMsgStr.size());

    memcpy(pubMsg.data(), terminateMsgStr.c_str(), terminateMsgStr.size());
    mainPubSkt_.send(pubMsg);

    // give threads some time to tidy up and terminate
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void Co2Main::listener()
{
    DBG_TRACE();

    zmq::pollitem_t rxItems [] = {
        { static_cast<void*>(netMonSkt_), 0, ZMQ_POLLIN, 0 }
        //{ static_cast<void*>(rxBar), 0, ZMQ_POLLIN, 0 }
    };
    int numRxItems = sizeof(rxItems) / sizeof(rxItems[0]);


    while (!shouldTerminate_.load(std::memory_order_relaxed)) {
        try {
            int nItems = zmq::poll(rxItems, numRxItems, rxTimeoutMsec_);

            if (nItems == 0) {
                // timed out
                continue;
            }

            // netMonitor
            if (rxItems[0].revents & ZMQ_POLLIN) {
                readMsgFromNetMonitor();
            }

        } catch (CO2::exceptionLevel& el) {
            if (el.isFatal()) {
                syslog(LOG_ERR, "%s fatal exception: %s", __FUNCTION__, el.what());
                shouldTerminate_.store(true, std::memory_order_relaxed);
            } else {
                syslog(LOG_ERR, "%s exception: %s", __FUNCTION__, el.what());
            }
        } catch (...) {
            syslog(LOG_ERR, "%s unknown exception", __FUNCTION__);
        }
    }
}


void Co2Main::threadStateChangeNotify(co2Message::ThreadState_ThreadStates threadState, const char* threadName)
{
    DBG_TRACE();

    switch (threadState) {
    case co2Message::ThreadState_ThreadStates_RUNNING:
        break;

    case co2Message::ThreadState_ThreadStates_STOPPING:
    case co2Message::ThreadState_ThreadStates_STOPPED:
        syslog(LOG_INFO, "%s thread state now: %s", threadName, CO2::threadStateStr(threadState));
        shouldTerminate_.store(true, std::memory_order_relaxed);
        break;

    case co2Message::ThreadState_ThreadStates_FAILED:
        syslog(LOG_CRIT, "%s thread state now: %s", threadName, CO2::threadStateStr(threadState));
        shouldTerminate_.store(true, std::memory_order_relaxed);
        failType_ = SoftwareFail;
        terminateReason_ = FatalException;
        break;

    default:
        std::string s(threadName);
        s.append(" thread state has changed from RUNNING to ");
        s.append(CO2::threadStateStr(threadState));
        throw CO2::exceptionLevel(s, true);
    }
}

void Co2Main::runloop()
{
    DBG_TRACE();

    time_t timeNow = time(0);
    std::string exceptionStr;

    // listener thread takes care of receiving all messages destined for us
    std::thread* listenerThread = new std::thread(std::bind(&Co2Main::listener, this));

    NetMonitor* netMon = nullptr;
    std::thread* netMonThread;

    rxTimeoutMsec_ = 2000;  // we give threads this amount of time to initialize
    startTimeoutSec_ = 5; // and this amount of time to be up and running after publishing config

    //std::thread* co2MonThread;
    //std::thread* displayThread;

    /**************************************************************************/
    /*                                                                        */
    /* Start threads                                                          */
    /*                                                                        */
    /**************************************************************************/
    syslog(LOG_DEBUG, "Co2Main::runloop: starting threads");
    const char* threadName;
    try {

        threadName = "NetMonitor";
        netMon = new NetMonitor(context_, zSockType_);

        if (netMon) {
            netMonThread = new std::thread(std::bind(&NetMonitor::run, netMon));
        } else {
            throw CO2::exceptionLevel("failed to initialise NetMonitor", true);
        }
    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_CRIT, "Fatal exception starting thread %s: %s", threadName, el.what());
            throw;
        }
        syslog(LOG_ERR, "exception starting thread %s: %s", threadName, el.what());
    } catch (...) {
        syslog(LOG_CRIT, "Exception starting thread %s", threadName);
        throw;
    }

    DBG_TRACE();

    // Now that we have started all threads we need to wait for them to
    // report that they are ready.
    // All must be ready within the given time otherwise we have to terminate
    // everything.

    /**************************************************************************/
    /*                                                                        */
    /* Send config to all threads                                             */
    /*                                                                        */
    /**************************************************************************/
    syslog(LOG_DEBUG, "Co2Main::runloop: sending config to threads");
    std::this_thread::sleep_for(std::chrono::milliseconds(rxTimeoutMsec_));

    if (netMonThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {
        if (!exceptionStr.empty()) {
            exceptionStr.append(", ");
        }

        exceptionStr.append("NetMonitor");
    }

    if (!exceptionStr.empty()) {
        // at least one thread didn't start
        // exceptionStr will contain a list of
        // each thread which failed to get to AWAITING_CONFIG state.
        //
        exceptionStr.append(" failed to start");
        throw CO2::exceptionLevel(exceptionStr, true);
    }

    // if we get here it means that all threads are ready to receive their configuration
    publishAllConfig();

    // We'll allow some time for everything to start
    time_t threadsStartDeadline = time(0) + startTimeoutSec_;

    while (true) {
        bool allThreadsRunning = true;
        timeNow = time(0);

        if (sdWatchdog->timeOfNextKick() <= timeNow) {
            sdWatchdog->kick();
        }

        if (netMonThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_RUNNING) {
            allThreadsRunning = false;
            if (timeNow > threadsStartDeadline) {
                exceptionStr.append("NetMonitor");
            }
        }

        if (allThreadsRunning) {
            // everything running OK, so we can get on with life
            break;
        }

        if (timeNow > threadsStartDeadline) {
            // at least one thread failed to run.
            // exceptionStr will contain a list of
            // each thread which failed to get to RUNNING state.
            //
            exceptionStr.append(" failed to run");
            throw CO2::exceptionLevel(exceptionStr, true);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(rxTimeoutMsec_));
    }

    // All threads are up and running here

    /**************************************************************************/
    /*                                                                        */
    /* This is the main run loop.                                             */
    /*                                                                        */
    /**************************************************************************/
    syslog(LOG_DEBUG, "Co2Main::runloop: starting main run loop");
    while (!shouldTerminate_.load(std::memory_order_relaxed)) {
        bool somethingHappened = false;

        if (sdWatchdog->timeUntilNextKick() == 0) {
            sdWatchdog->kick();
            syslog(LOG_DEBUG, "Co2Main::runloop: kicked watchdog");
        }

        // We make local copies of state change flags so we
        // keep the lock for as short a time as possible
        //
        bool threadStateChanged;
        bool netStateChanged;

        threadStateChanged = threadStateChanged_.load(std::memory_order_relaxed);
        threadStateChanged_.store(false, std::memory_order_relaxed);

        netStateChanged = netStateChanged_.load(std::memory_order_relaxed);
        netStateChanged_.store(false, std::memory_order_relaxed);

        if (threadStateChanged) {
            somethingHappened = true;

            if (netMonThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_RUNNING) {
                threadStateChangeNotify(netMonThreadState_.load(std::memory_order_relaxed), "Net Monitor");
            }
        }

        if (netStateChanged) {
            somethingHappened = true;
            netMonFSM();
        }

        if (somethingHappened) {
            somethingHappened = false;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    syslog(LOG_DEBUG, "Co2Main::runloop: out of main run loop");
    /**************************************************************************/
    /*                                                                        */
    /* end of main run loop.                                                  */
    /*                                                                        */
    /**************************************************************************/

    terminateAllThreads();
}

void Co2Main::terminate()
{
    syslog(LOG_INFO, "%s: terminateReason=%s  userReqType=%s  failType=%s",
           __FUNCTION__,
           Co2Main::terminateReasonStr(), Co2Main::userReqTypeStr(), Co2Main::failTypeStr());

    switch (terminateReason_) {
    case UserReq:
        switch (userReqType_) {
        case Restart:
            restartMgr_->stop();
            break;
        case Reboot:
            restartMgr_->reboot(true);
            break;
        case Shutdown:
            restartMgr_->shutdown();
            break;
        }
        break;

    case SignalReceived:
        restartMgr_->stop();
        break;

    case FatalException:
        switch (failType_) {
        case OK:
        case SoftwareFail:
            restartMgr_->restart();
            break;

        case HardwareFail:
            restartMgr_->reboot(false);
            break;
        }
        break;
    }
}

void Co2Main::sigHandler(int sig)
{
    switch (sig) {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        // for now we'll make no difference
        // between various signals.
        //
        Co2Main::terminateReason_ = Co2Main::SignalReceived;
        Co2Main::failType_ = Co2Main::NoFail;
        Co2Main::userReqType_ = Co2Main::Restart;
        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
        break;
    default:
        // This signal handler should only be
        // receiving above signals, so we'll just
        // ignore anything else. Note that we cannot
        // log this as signal handlers should not
        // make system calls.
        //
        break;
    }
}

int main(int argc, char* argv[])
{
    int rc = 0;
    ConfigMap cfg;
    Co2Main co2Main(cfg);

    // These configuration may be set in config file
    int logLevel;

    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    CO2::globals->setProgName(argv[0]);

    CO2::co2Defaults->setConfigDefaults(cfg);
    CO2::globals->setCfg(&cfg);

    setlogmask(LOG_UPTO(CO2::co2Defaults->kLogLevelDefault));

    openlog(CO2::globals->progName(), LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_INFO, "%s:%u", __FUNCTION__, __LINE__);

    // no need for anything fancy like getopts() because
    // we only ever take a single argument pair: the config filename
    //
    if ( (argc < 3) || strncmp(argv[1], "-c", 3) ) {
        syslog (LOG_ERR, "too few arguments - usage: %s -c <config_file>", CO2::globals->progName());
        return -1;
    }

    // as we need root permissions for devices we need to run as root
    if (getuid() != 0) {
        syslog (LOG_ERR, "Need to run \"%s\" as root as it needs root priveleges for devices", CO2::globals->progName());
        return -1;
    }

    try {
        co2Main.init(CO2::globals->progName());
    } catch (...) {

    }

    rc = co2Main.readConfigFile(argv[2]);

    if (rc < 0) {
        return rc;
    }

    logLevel =  CO2::getLogLevelFromStr(cfg.find("LogLevel")->second->getStr());

    if ( (logLevel >= 0) && ((logLevel & LOG_PRIMASK) == logLevel) ) {
        setlogmask(LOG_UPTO(logLevel));
    } else {
        // It doesn't matter too much if log level is bad. We can just use default.
        syslog (LOG_ERR, "invalid log level \"%s\" in config file", cfg.find("LogLevel")->second->getStr());
        logLevel = CO2::co2Defaults->kLogLevelDefault;
    }

    syslog(LOG_INFO, "logLevel=%s\n", CO2::getLogLevelStr(logLevel));

    if (cfg.find("WatchdogKickPeriod") != cfg.end()) {
        int tempInt = cfg.find("WatchdogKickPeriod")->second->getInt();
        sdWatchdog->setKickPeriod((time_t)tempInt);
    } else {
        sdWatchdog->setKickPeriod(60); // seconds
        syslog(LOG_ERR, "Missing WatchdogKickPeriod. Using a period of %u seconds.", uint(sdWatchdog->kickPeriod()));
    }

    sdWatchdog->kick();

    try {

        co2Main.runloop();
        syslog(LOG_INFO, "co2Main.runloop() exited normally");

    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_CRIT, "Co2Main::runloop: fatal exception: %s", el.what());
        } else {
            syslog(LOG_ERR, "Co2Main::runloop: exception: %s", el.what());
        }
    } catch (...) {
        syslog(LOG_CRIT, "Co2Main::runloop: unknown exception");
    }

    co2Main.terminate();

    // shouldn't ever get here...
    return 0;
}

