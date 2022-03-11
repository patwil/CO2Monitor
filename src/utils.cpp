/*
 * utils.cpp
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#include <syslog.h>

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
    syslog(LOG_DEBUG, "state event %d for thread: %s", event, threadName_.c_str());

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
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::HardwareFail:
            nextState = co2Message::ThreadState_ThreadStates_HW_FAILED;
            break;
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
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::HardwareFail:
            nextState = co2Message::ThreadState_ThreadStates_HW_FAILED;
            break;
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
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::HardwareFail:
            nextState = co2Message::ThreadState_ThreadStates_HW_FAILED;
            break;
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
            nextState = co2Message::ThreadState_ThreadStates_FAILED;
            break;
        case CO2::ThreadFSM::HardwareFail:
            nextState = co2Message::ThreadState_ThreadStates_HW_FAILED;
            break;
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
        case CO2::ThreadFSM::HardwareFail:
            // ignore this event when in this state
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
    case co2Message::ThreadState_ThreadStates_HW_FAILED:
        // ignore all events in this state
        break;
    default:
        break;
    }

    if (currentState != nextState) {
        syslog(LOG_INFO, "%s state change from %s to %s", threadName_.c_str(), CO2::stateStr(currentState), CO2::stateStr(nextState));
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

    pSendSocket_->send(threadStateMsg, zmq::send_flags::none);
    syslog(LOG_DEBUG, "%s sent new state %s", threadName_.c_str(), stateStr());
}


const char* CO2::stateStr(co2Message::ThreadState_ThreadStates state)
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
    case co2Message::ThreadState_ThreadStates_HW_FAILED:
        return "HW_FAILED";
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

    progName_ = new char[strlen(p) + 1];
    if (!progName_) {
        throw CO2::exceptionLevel("failed to allocate memory for new char[]", true);
    }
    strcpy(progName_, p);
}

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

namespace CO2 {

std::string zeroPadNumber(int width, double num, char pad, int precision)
{
    std::ostringstream ss;
    if (precision) {
        ss << std::fixed;
        ss << std::setprecision(precision);
        width += precision + 1;
    }
    ss << std::setw(width) << std::setfill(pad) << num;
    return ss.str();
}

std::string zeroPadNumber(int width, int num, char pad)
{
    std::ostringstream ss;
    ss << std::setw(width) << std::setfill(pad) << num;
    return ss.str();
}

bool isInRange(const char* key, int val)
{
    ConfigMapCI entry = globals->cfg()->find(key);

    if (entry != globals->cfg()->end()) {
        Config* pCfg = entry->second;

        return pCfg->isInRange(val);
    }

    return false;
}

} // namespace CO2


// ZMQ endpoint names
const char* CO2::netMonEndpoint     = "inproc://netMonEndPoint";
const char* CO2::co2MonEndpoint     = "inproc://co2MonEndPoint";
const char* CO2::uiEndpoint         = "inproc://uiEndPoint";
const char* CO2::co2MainPubEndpoint = "inproc://co2MainPubEndPoint";

