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

#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

#include "netMonitor.h"
#include "co2Monitor.h"
#include "co2Display.h"
#include "displayElement.h"
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
    void readMsgFromUI();

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
    zmq::socket_t uiSkt_;
    zmq::socket_t co2MonSkt_;

    //std::mutex mutex_; // used to control access to attributes used by multiple threads

    std::atomic<co2Message::NetState_NetStates> netState_;
    std::atomic<co2Message::NetState_NetStates> newNetState_;
    std::atomic<bool> netStateChanged_;

    std::atomic<bool> threadStateChanged_; // one or more threads have changed state
    CO2::ThreadFSM* myThreadState_;

    std::atomic<co2Message::ThreadState_ThreadStates> netMonThreadState_;
    std::atomic<co2Message::ThreadState_ThreadStates> co2MonThreadState_;
    std::atomic<co2Message::ThreadState_ThreadStates> displayThreadState_;

    std::chrono::milliseconds rxTimeoutMsec_;
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
        netMonSkt_(context_, zSockType_),
        uiSkt_(context_, zSockType_),
        co2MonSkt_(context_, zSockType_)
    {
        shouldTerminate_.store(false, std::memory_order_relaxed);

        myThreadState_ = new CO2::ThreadFSM("Co2MonitorMain");
        netState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
        newNetState_.store(co2Message::NetState_NetStates_START, std::memory_order_relaxed);
        netStateChanged_.store(false, std::memory_order_relaxed);
        threadStateChanged_.store(false, std::memory_order_relaxed);
        netMonThreadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);
        co2MonThreadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);
        displayThreadState_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);

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
        uiSkt_.bind(CO2::uiEndpoint);
        co2MonSkt_.bind(CO2::co2MonEndpoint);

        // set up signal handler. We only have one
        // to handle all trapped signals
        //
        struct sigaction action;
        //
        action.sa_sigaction = Co2Main::sigHandler;
        action.sa_flags = SA_SIGINFO;
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

    static void sigHandler(int sig, siginfo_t *siginfo, void *context);
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
        mainPubSkt_.send(configMsg, zmq::send_flags::none);
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
        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
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
        mainPubSkt_.send(configMsg, zmq::send_flags::none);
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
        if (netMonSkt_.recv(msg, zmq::recv_flags::none)) {

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

void Co2Main::readMsgFromCo2Monitor()
{
    DBG_TRACE();

    try {
        zmq::message_t msg;
        if (co2MonSkt_.recv(msg, zmq::recv_flags::none)) {

            std::string msg_str(static_cast<char*>(msg.data()), msg.size());

            co2Message::Co2Message co2Msg;

            if (!co2Msg.ParseFromString(msg_str)) {
                throw CO2::exceptionLevel("couldn't parse message from Co2 monitor", false);
            }

            switch (co2Msg.messagetype()) {

            case co2Message::Co2Message_Co2MessageType_CO2_STATE:
                if (co2Msg.has_co2state()) {

                    mainPubSkt_.send(msg, zmq::send_flags::none);
                    DBG_MSG(LOG_DEBUG, "published Co2 state");

                } else {
                    throw CO2::exceptionLevel("missing Co2 state", false);
                }
                break;

            case co2Message::Co2Message_Co2MessageType_THREAD_STATE:
                if (co2Msg.has_threadstate()) {
                    const co2Message::ThreadState& threadStateMsg = co2Msg.threadstate();
                    if (co2MonThreadState_.load(std::memory_order_relaxed) != threadStateMsg.threadstate()) {
                        co2MonThreadState_.store(threadStateMsg.threadstate(), std::memory_order_relaxed);
                        threadStateChanged_.store(true, std::memory_order_relaxed);
                        syslog(LOG_DEBUG, "%s: rx thread State=%s", __FUNCTION__,
                                          CO2::stateStr(co2MonThreadState_.load(std::memory_order_relaxed)));
                    }
                } else {
                    throw CO2::exceptionLevel("missing Co2 monitor threadState", false);
                }
                break;
            default:
                throw CO2::exceptionLevel("unexpected message from Co2 monitor thread", false);
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

void Co2Main::readMsgFromUI()
{
    DBG_TRACE();

    try {
        zmq::message_t msg;
        if (uiSkt_.recv(msg, zmq::recv_flags::none)) {

            std::string msg_str(static_cast<char*>(msg.data()), msg.size());

            co2Message::Co2Message co2Msg;

            if (!co2Msg.ParseFromString(msg_str)) {
                throw CO2::exceptionLevel("couldn't parse message from UI", false);
            }

            switch (co2Msg.messagetype()) {

            case co2Message::Co2Message_Co2MessageType_FAN_CFG:
                if (co2Msg.has_fanconfig()) {

                    mainPubSkt_.send(msg, zmq::send_flags::none);
                    DBG_MSG(LOG_DEBUG, "published fan config");

                } else {
                    throw CO2::exceptionLevel("missing UI fan config", false);
                }
                break;

            case co2Message::Co2Message_Co2MessageType_RESTART:
                if (co2Msg.has_restartmsg()) {
                    const co2Message::RestartMsg& restartMsg = co2Msg.restartmsg();

                    Co2Main::terminateReason_ = Co2Main::UserReq;
                    Co2Main::failType_ = Co2Main::NoFail;

                    switch (restartMsg.restarttype()) {
                    case co2Message::RestartMsg_RestartType_REBOOT:
                        Co2Main::userReqType_ = Co2Main::Reboot;
                        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
                        break;

                    case co2Message::RestartMsg_RestartType_SHUTDOWN:
                        Co2Main::userReqType_ = Co2Main::Shutdown;
                        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
                        break;
                    default:
                        syslog(LOG_ERR, "Unknown restart type: %d", static_cast<int>(restartMsg.restarttype()));
                        break;
                    }

                } else {
                    throw CO2::exceptionLevel("missing restart message", false);
                }
                break;

            case co2Message::Co2Message_Co2MessageType_THREAD_STATE:
                if (co2Msg.has_threadstate()) {
                    const co2Message::ThreadState& threadStateMsg = co2Msg.threadstate();
                    if (displayThreadState_.load(std::memory_order_relaxed) != threadStateMsg.threadstate()) {
                        displayThreadState_.store(threadStateMsg.threadstate(), std::memory_order_relaxed);
                        threadStateChanged_.store(true, std::memory_order_relaxed);
                        syslog(LOG_DEBUG, "%s: rx thread State=%s", __FUNCTION__,
                                          CO2::stateStr(displayThreadState_.load(std::memory_order_relaxed)));
                    }
                } else {
                    throw CO2::exceptionLevel("missing Display threadState", false);
                }
                break;
            default:
                throw CO2::exceptionLevel("unexpected message from Display thread", false);
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

    if (cfg_.find("SDL_MOUSE_RELATIVE") != cfg_.end()) {
        uiCfg->set_mouserelative(cfg_.find("SDL_MOUSE_RELATIVE")->second->getStr());
    } else {
        configIsOk = false;
        syslog(LOG_ERR, "Missing SDL_MOUSE_RELATIVE config");
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
        mainPubSkt_.send(configMsg, zmq::send_flags::none);
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
        mainPubSkt_.send(configMsg, zmq::send_flags::none);
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
    mainPubSkt_.send(netStateMsg, zmq::send_flags::none);
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
    mainPubSkt_.send(pubMsg, zmq::send_flags::none);

    // give threads some time to tidy up and terminate
    std::this_thread::sleep_for(std::chrono::seconds(5));
}

void Co2Main::listener()
{
    DBG_TRACE();

    zmq::pollitem_t rxItems [] = {
        { static_cast<void*>(netMonSkt_), 0, ZMQ_POLLIN, 0 },   // 0
        { static_cast<void*>(co2MonSkt_), 0, ZMQ_POLLIN, 0 },   // 1
        { static_cast<void*>(uiSkt_), 0, ZMQ_POLLIN, 0 }        // 2
    };
    int numRxItems = sizeof(rxItems) / sizeof(rxItems[0]);


    while (!Co2Main::shouldTerminate_.load(std::memory_order_relaxed)) {
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

            // co2Monitor
            if (rxItems[1].revents & ZMQ_POLLIN) {
                readMsgFromCo2Monitor();
            }

            // UI (Co2Display)
            if (rxItems[2].revents & ZMQ_POLLIN) {
                readMsgFromUI();
            }

        } catch (CO2::exceptionLevel& el) {
            if (el.isFatal()) {
                syslog(LOG_ERR, "%s fatal exception: %s", __FUNCTION__, el.what());
                Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
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
        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
        break;

    case co2Message::ThreadState_ThreadStates_FAILED:
        syslog(LOG_ERR, "%s thread state now: %s", threadName, CO2::threadStateStr(threadState));
        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
        failType_ = SoftwareFail;
        terminateReason_ = FatalException;
        break;

    case co2Message::ThreadState_ThreadStates_HW_FAILED:
        syslog(LOG_ERR, "%s thread state now: %s", threadName, CO2::threadStateStr(threadState));
        Co2Main::shouldTerminate_.store(true, std::memory_order_relaxed);
        failType_ = HardwareFail;
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
    DBG_TRACE_MSG("Start of Co2Main::runloop");

    time_t timeNow = time(0);
    std::string exceptionStr;

    // listener thread takes care of receiving all messages destined for us
    std::thread* listenerThread = new std::thread(std::bind(&Co2Main::listener, this));

    NetMonitor* netMon = nullptr;
    std::thread* netMonThread;

    Co2Monitor* co2Mon = nullptr;
    std::thread* co2MonThread;

    Co2Display* co2Display = nullptr;
    std::thread* displayThread;

    rxTimeoutMsec_ = std::chrono::milliseconds(2000);  // we give threads this amount of time to initialize
    startTimeoutSec_ = 5; // and this amount of time to be up and running after publishing config

#ifdef HAS_WIRINGPI
    wiringPiSetupGpio(); // must be called once (and only once) before any GPIO related calls
#endif

    /**************************************************************************/
    /*                                                                        */
    /* Start threads                                                          */
    /*                                                                        */
    /**************************************************************************/
    DBG_TRACE_MSG("Co2Main::runloop: starting threads");
    const char* threadName;
    try {

        threadName = "NetMonitor";
        netMon = new NetMonitor(context_, zSockType_);

        if (netMon) {
            netMonThread = new std::thread(std::bind(&NetMonitor::run, netMon));
        } else {
            throw CO2::exceptionLevel("failed to initialise NetMonitor", true);
        }

        threadName = "Co2Monitor";
        co2Mon = new Co2Monitor(context_, zSockType_);

        if (co2Mon) {
            co2MonThread = new std::thread(std::bind(&Co2Monitor::run, co2Mon));
        } else {
            throw CO2::exceptionLevel("failed to initialise Co2 Monitor", true);
        }

        threadName = "Co2Display";
        co2Display = new Co2Display(context_, zSockType_);

        if (co2Display) {
            displayThread = new std::thread(std::bind(&Co2Display::run, co2Display));
        } else {
            throw CO2::exceptionLevel("failed to initialise Co2Display", true);
        }

    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_ERR, "Fatal exception starting thread %s: %s", threadName, el.what());
            throw;
        }
        syslog(LOG_ERR, "exception starting thread %s: %s", threadName, el.what());
    } catch (...) {
        syslog(LOG_ERR, "Exception starting thread %s", threadName);
        throw;
    }

    DBG_TRACE_MSG("Co2Main::runloop: all threads started");

    // Now that we have started all threads we need to wait for them to
    // report that they are ready.
    // All must be ready within the given time otherwise we have to terminate
    // everything.

    /**************************************************************************/
    /*                                                                        */
    /* Send config to all threads                                             */
    /*                                                                        */
    /**************************************************************************/
    DBG_TRACE_MSG("Co2Main::runloop: sending config to threads");
    std::this_thread::sleep_for(std::chrono::milliseconds(rxTimeoutMsec_));

    if (netMonThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {
        exceptionStr.append("NetMonitor");
    }

    //displayThreadState_.store(co2Message::ThreadState_ThreadStates_AWAITING_CONFIG, std::memory_order_relaxed);
    if (displayThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {
        if (!exceptionStr.empty()) {
            exceptionStr.append(", ");
        }
        exceptionStr.append("Display");
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

        if (co2MonThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_RUNNING) {
            allThreadsRunning = false;
            if (timeNow > threadsStartDeadline) {
                exceptionStr.append("Co2Monitor");
            }
        }
        //displayThreadState_.store(co2Message::ThreadState_ThreadStates_RUNNING, std::memory_order_relaxed);
        if (displayThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_RUNNING) {
            allThreadsRunning = false;
            if (timeNow > threadsStartDeadline) {
                if (!exceptionStr.empty()) {
                    exceptionStr.append(", ");
                }
                exceptionStr.append("Display");
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
    DBG_TRACE_MSG("Co2Main::runloop: starting main run loop");

    while (!Co2Main::shouldTerminate_.load(std::memory_order_relaxed)) {
        bool somethingHappened = false;

        if (sdWatchdog->timeUntilNextKick() == 0) {
            sdWatchdog->kick();
            DBG_TRACE_MSG("Co2Main::runloop: kicked watchdog");
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

            if (co2MonThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_RUNNING) {
                threadStateChangeNotify(co2MonThreadState_.load(std::memory_order_relaxed), "Co2 Monitor");
            }

            if (displayThreadState_.load(std::memory_order_relaxed) != co2Message::ThreadState_ThreadStates_RUNNING) {
                threadStateChangeNotify(displayThreadState_.load(std::memory_order_relaxed), "Display");
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
    DBG_TRACE_MSG("Co2Main::runloop: out of main run loop");

    /**************************************************************************/
    /*                                                                        */
    /* end of main run loop.                                                  */
    /*                                                                        */
    /**************************************************************************/

    terminateAllThreads();

    co2MonThread->join();
    DBG_TRACE_MSG("joined co2MonThread");
    if (co2Mon) {
        delete co2Mon;
        co2Mon = nullptr;
    }
    if (co2MonThread) {
        delete co2MonThread;
        co2MonThread = nullptr;
    }

    displayThread->join();
    DBG_TRACE_MSG("joined displayThread");
    if (co2Display) {
        delete co2Display;
        co2Display = nullptr;
    }
    DBG_TRACE_MSG("deleted co2Display");
    if (displayThread) {
        delete displayThread;
        displayThread = nullptr;
    }
    DBG_TRACE_MSG("deleted displayThread");
    netMonThread->join();
    DBG_TRACE_MSG("joined netMonThread");
    if (netMon) {
        delete netMon;
        netMon = nullptr;
    }
    if (netMonThread) {
        delete netMonThread;
        netMonThread = nullptr;
    }
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
        case HardwareFail:
            restartMgr_->restart();
            break;
        }
        break;
    }
}

void Co2Main::sigHandler(int sig, siginfo_t *siginfo, void *context)
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
            syslog(LOG_ERR, "Co2Main::runloop: fatal exception: %s", el.what());
        } else {
            syslog(LOG_ERR, "Co2Main::runloop: exception: %s", el.what());
        }
    } catch (...) {
        syslog(LOG_ERR, "Co2Main::runloop: unknown exception");
    }

    CO2::co2Defaults->clearConfigDefaults(cfg);
    co2Main.terminate();

    // shouldn't ever get here...
    return 0;
}

