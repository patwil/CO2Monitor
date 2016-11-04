/*
 * utils.cpp
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#include "utils.h"

CO2::ThreadFSM::ThreadFSM(const char* threadName, zmq::socket_t* pSendSocket)
{
    if (threadName && *threadName) {
        threadName_ = std::string(threadName);
    } else {
        threadName_ = std::string("unknown thread");
    }
    state_.store(co2Message::ThreadState_ThreadStates_INIT, std::memory_order_relaxed);
    stateChanged_.store(false, std::memory_order_relaxed);
    stateChangeTime_ = 0;
    pSendSocket_ = pSendSocket; // might be nullptr if arg uses default
}

CO2::ThreadFSM::~ThreadFSM()
{
    // Delete all dynamic memory
}

bool CO2::ThreadFSM::stateChanged() {
    bool stateChangedCopy = stateChanged_;
    stateChanged_ = false;
    return stateChangedCopy;
}

void CO2::ThreadFSM::stateEvent(CO2::ThreadFSM::ThreadEvent event)
{
    co2Message::ThreadState_ThreadStates currentState = state_.load(std::memory_order_relaxed);
    co2Message::ThreadState_ThreadStates nextState = currentState;
    time_t timeNow = time(0);

    switch (currentState) {

    case co2Message::ThreadState_ThreadStates_INIT:
        switch (event) {
        case CO2::ThreadFSM::ReadyForConfig:
            nextState = co2Message::ThreadState_ThreadStates_AWAITING_CONFIG;
            break;
        case CO2::ThreadFSM::ConfigOk:
            // initialisation included config, so no need to wait
            nextState = co2Message::ThreadState_ThreadStates_STARTED;
            break;
        case CO2::ThreadFSM::ConfigError:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::InitOk:
            nextState = co2Message::ThreadState_ThreadStates_STARTED;
            break;
        case CO2::ThreadFSM::InitFail:
        case CO2::ThreadFSM::RunTimeFail:
        case CO2::ThreadFSM::Timeout:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::Terminate:
            nextState = co2Message::ThreadState_ThreadStates_STOPPING;
            break;
        }
        break;

    case co2Message::ThreadState_ThreadStates_AWAITING_CONFIG:
        switch (event) {
        case CO2::ThreadFSM::ReadyForConfig:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigOk:
            nextState = co2Message::ThreadState_ThreadStates_STARTED;
            break;
        case CO2::ThreadFSM::ConfigError:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::InitOk:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::InitFail:
        case CO2::ThreadFSM::RunTimeFail:
        case CO2::ThreadFSM::Timeout:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::Terminate:
            nextState = co2Message::ThreadState_ThreadStates_STOPPING;
            break;
        }
        break;

    case co2Message::ThreadState_ThreadStates_STARTED:
        switch (event) {
        case CO2::ThreadFSM::ReadyForConfig:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigOk:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigError:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::InitOk:
            nextState = co2Message::ThreadState_ThreadStates_RUNNING;
            break;
        case CO2::ThreadFSM::InitFail:
        case CO2::ThreadFSM::RunTimeFail:
        case CO2::ThreadFSM::Timeout:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::Terminate:
            nextState = co2Message::ThreadState_ThreadStates_STOPPING;
            break;
        }
        break;

    case co2Message::ThreadState_ThreadStates_RUNNING:
        switch (event) {
        case CO2::ThreadFSM::ReadyForConfig:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigOk:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigError:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::InitOk:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::InitFail:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::RunTimeFail:
        case CO2::ThreadFSM::Timeout:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::Terminate:
            nextState = co2Message::ThreadState_ThreadStates_STOPPING;
            break;
        }
        break;

    case co2Message::ThreadState_ThreadStates_STOPPING:
        switch (event) {
        case CO2::ThreadFSM::ReadyForConfig:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigOk:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::ConfigError:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::InitOk:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::InitFail:
            // ignore this event when in this state
            break;
        case CO2::ThreadFSM::RunTimeFail:
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::Timeout:
            nextState = co2Message::ThreadState_ThreadStates_STOPPED;
            break;
        case CO2::ThreadFSM::Terminate:
            nextState = co2Message::ThreadState_ThreadStates_STOPPING;
            break;
        }
        break;

    case co2Message::ThreadState_ThreadStates_STOPPED:
        // ignore all events in this state
        break;

    case co2Message::ThreadState_ThreadStates_FAILED:
        // ignore all events in this state
        break;
    }

    if (currentState != nextState) {
        syslog(LOG_INFO, "%s state change from %s to %s", threadName_.c_str(), stateStr(currentState), stateStr(nextState));
        state_.store(nextState, std::memory_order_relaxed);
        stateChanged_.store(true, std::memory_order_relaxed);
        stateChangeTime_ = timeNow;
        sendThreadState();
    }
}

void CO2::ThreadFSM::sendThreadState()
{
    if (pSendSocket_ == nullptr) {
        // this thread doesn't send messages
        return;
    }

    // Only send thread state if it has changed
    if (!stateChanged_.load(std::memory_order_relaxed)) {
        return;
    }

    std::string threadStateStr;
    co2Message::Co2Message co2Msg;
    co2Message::ThreadState* threadState = co2Msg.mutable_threadstate();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_THREAD_STATE);

    threadState->set_threadstate(state_.load(std::memory_order_relaxed));

    co2Msg.SerializeToString(&threadStateStr);

    zmq::message_t threadStateMsg(threadStateStr.size());
    memcpy (threadStateMsg.data(), threadStateStr.c_str(), threadStateStr.size());

    pSendSocket_->send(threadStateMsg);
}


const char* CO2::ThreadFSM::stateStr(co2Message::ThreadState_ThreadStates state)
{
    switch (state) {
    case co2Message::ThreadState_ThreadStates_INIT:
        return "INIT";
    case co2Message::ThreadState_ThreadStates_AWAITING_CONFIG:
        return "AWAITING_CONFIG";
    case co2Message::ThreadState_ThreadStates_STARTED:
        return "STARTED";
    case co2Message::ThreadState_ThreadStates_RUNNING:
        return "RUNNING";
    case co2Message::ThreadState_ThreadStates_STOPPING:
        return "STOPPING";
    case co2Message::ThreadState_ThreadStates_STOPPED:
        return "STOPPED";
    case co2Message::ThreadState_ThreadStates_FAILED:
        return "FAILED";
    default:
        return "Unknown thread state";
    }
}

void CO2::Globals::setProgName(char* pathname)
{
    std::lock_guard<std::mutex> lock(Globals::mutex_);

    if (progName_) {
        // can only set this once
        return;
    }

    char* p = strrchr(pathname, '/');

    if (p) {
        p++; // skip '/'
    } else {
        p = pathname;
    }

    progName_ = new char[strlen(p)];
    strcpy(progName_, p);
}

//Globals* Globals::pInstance = nullptr;

std::shared_ptr<CO2::Globals> CO2::globals = CO2::Globals::getInstance();
std::mutex CO2::Globals::mutex_;

int CO2::getLogLevelFromStr(const char* pLogLevelStr)
{
    //DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT
    const char* logName2Level[] = {
        "EMERG",
        "ALERT",
        "CRIT",
        "ERR",
        "WARNING",
        "NOTICE",
        "INFO",
        "DEBUG",
        0
    };

    for (int i = 0; logName2Level[i]; i++) {
        if (!strcmp(pLogLevelStr, logName2Level[i])) {
            return i;
        }
    }

    return -1;
}

const char* CO2::getLogLevelStr(int logLevel)
{
    switch (logLevel) {
        case LOG_EMERG:
            return "EMERG";

        case LOG_ALERT:
            return "ALERT";

        case LOG_CRIT:
            return "CRIT";

        case LOG_ERR:
            return "ERR";

        case LOG_WARNING:
            return "WARNING";

        case LOG_NOTICE:
            return "NOTICE";

        case LOG_INFO:
            return "INFO";

        case LOG_DEBUG:
            return "DEBUG";

        default:
            return "UNKNOWN";
    }
}


const char* CO2::threadStateStr(co2Message::ThreadState_ThreadStates threadState)
{
    switch (threadState) {
    case co2Message::ThreadState_ThreadStates_INIT:
        return "INIT";
    case co2Message::ThreadState_ThreadStates_AWAITING_CONFIG:
        return "AWAITING CONFIG";
    case co2Message::ThreadState_ThreadStates_STARTED:
        return "STARTED";
    case co2Message::ThreadState_ThreadStates_RUNNING:
        return "RUNNING";
    case co2Message::ThreadState_ThreadStates_STOPPING:
        return "STOPPING";
    case co2Message::ThreadState_ThreadStates_STOPPED:
        return "STOPPED";
    case co2Message::ThreadState_ThreadStates_FAILED:
        return "FAILED";
    default:
        return "Unknown thread state";
    }
}

// ZMQ endpoint names
const char* CO2::netMonEndpoint     = "inproc://netMon";
const char* CO2::co2MonEndpoint     = "inproc://co2Mon";
const char* CO2::uiEndpoint         = "inproc://ui";
const char* CO2::co2MainPubEndpoint = "inproc://co2MainPub";
const char* CO2::co2MainSubEndpoint = "inproc://co2MainSub";

//const char* CO2::kReadyStr     = "ready";
//const char* CO2::kGoStr        = "go";
//const char* CO2::kTerminateStr = "terminate";

