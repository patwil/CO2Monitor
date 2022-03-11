/*
 * utils.h
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <zmq.hpp>

#include "config.h"
#include "co2Message.pb.h"

#ifdef DEBUG
#define DBG_TRACE_MSG(MSG)  syslog(LOG_DEBUG, "%s::%s: (line %u) - " MSG, typeid(this).name(), __FUNCTION__, __LINE__)
#define DBG_TRACE()         syslog(LOG_DEBUG, "%s::%s: (line %u)", typeid(this).name(), __FUNCTION__, __LINE__)
#define DBG_MSG(...)        syslog(__VA_ARGS__)
#else
#define DBG_TRACE_MSG(MSG)
#define DBG_TRACE()
#define DBG_MSG(...)
#endif

namespace CO2 {

const char* stateStr(co2Message::ThreadState_ThreadStates state);

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
            HardwareFail,
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
            return CO2::stateStr(state_.load(std::memory_order_relaxed));
        }

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

        Globals() : progName_(0), pCfg_(0) {};
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

bool isInRange(const char* key, int val);

extern std::shared_ptr<Globals> globals;

int getLogLevelFromStr(const char* pLogLevelStr);
const char* getLogLevelStr(int logLevel);

const char* threadStateStr(co2Message::ThreadState_ThreadStates);

std::string zeroPadNumber(int width, double num, char pad = '0', int precision = 0);
std::string zeroPadNumber(int width, int num, char pad = '0');

// ZMQ endpoint names
extern const char* netMonEndpoint;
extern const char* co2MonEndpoint;
extern const char* uiEndpoint;
extern const char* co2MainPubEndpoint;

} // namespace CO2

#endif /*_UTILS_H_*/
