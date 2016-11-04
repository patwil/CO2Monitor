/*
 * utils.h
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <typeinfo>
#include <exception>
#include <new>
#include <cstdlib>
#include <cstring>
#include <syslog.h>
#include <ctime>
#include <sys/time.h>
#include <zmq.hpp>

#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

#include "config.h"

namespace CO2 {

class ThreadFSM
{
    public:
        typedef enum {
            ReadyForConfig,
            ConfigOk,
            ConfigError,
            InitOk,
            InitFail,
            RunTimeFail,
            Timeout,
            Terminate
        } ThreadEvent;

        ThreadFSM(const char* threadName, zmq::socket_t* pSendSocket = nullptr);
        ~ThreadFSM();

        co2Message::ThreadState_ThreadStates state() {
            return state_.load(std::memory_order_relaxed);
        }

        bool stateChanged();
        void stateEvent(ThreadEvent event);
        void sendThreadState();

        const char* stateStr() {
            return stateStr(state_.load(std::memory_order_relaxed));
        }
        const char* stateStr(co2Message::ThreadState_ThreadStates state);

    private:
        ThreadFSM();

        std::string threadName_;
        std::atomic<co2Message::ThreadState_ThreadStates> state_;
        std::atomic<bool> stateChanged_;
        time_t stateChangeTime_;

        zmq::socket_t* pSendSocket_;
};

class exceptionLevel: public std::exception
{
        std::string errorStr_;
        bool isFatal_;
    public:
        exceptionLevel(const std::string errorStr = "exception", bool isFatal = false) noexcept :
            errorStr_(errorStr), isFatal_(isFatal) {}

        virtual const char* what() const throw() {
            return errorStr_.c_str();
        }

        bool isFatal() noexcept {
            return isFatal_;
        }
};

class Globals
{
        static std::mutex mutex_;

        Globals() {};
        Globals(const Globals& rhs);
        Globals& operator=(const Globals& rhs);

        char* progName_;
        ConfigMap* pCfg_;

    public:
        static std::shared_ptr<Globals>& getInstance() {
            static std::shared_ptr<Globals> instance = nullptr;

            if (!instance) {
                std::lock_guard<std::mutex> lock(mutex_);

                if (!instance) {
                    instance.reset(new Globals());
                }
            }

            return instance;
        }

        void setProgName(char* pathname);

        const char* progName() {
            return progName_;
        }

        void setCfg(ConfigMap* pCfg) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (pCfg) {
                pCfg_ = pCfg;
            }
        }

        ConfigMap* cfg() {
            return pCfg_;
        }
};

extern std::shared_ptr<Globals> globals;

int getLogLevelFromStr(const char* pLogLevelStr);
const char* getLogLevelStr(int logLevel);

const char* threadStateStr(co2Message::ThreadState_ThreadStates);

// ZMQ endpoint names
extern const char* netMonEndpoint;
extern const char* co2MonEndpoint;
extern const char* uiEndpoint;
extern const char* co2MainPubEndpoint;
extern const char* co2MainSubEndpoint;

//extern const char* kReadyStr;
//extern const char* kGoStr;
//extern const char* kTerminateStr;

} // namespace CO2

#endif /*_UTILS_H_*/