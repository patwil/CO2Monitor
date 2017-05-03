/*
 * co2Monitor.h
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#ifndef CO2MONITOR_H
#define CO2MONITOR_H

#include <exception>
#include <time.h>

#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

#include <zmq.hpp>
#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>
#include "co2Display.h"
#include "co2Sensor.h"
#include "utils.h"

class Co2Monitor
{
    public:
        Co2Monitor(zmq::context_t& ctx, int sockType);

        ~Co2Monitor();

        void run();

        static std::atomic<bool> shouldTerminate_;

    private:
        Co2Monitor();

        void getCo2ConfigFromMsg(co2Message::Co2Message& cfgMsg);
        void getFanConfigFromMsg(co2Message::Co2Message& cfgMsg);
        void listener();

        void publishCo2State();

        void startFanManOnTimer();
        void stopFanManOnTimer();
        void updateFanState();
        void updateFanState(Co2Display::FanAutoManStates newFanAutoManState);

        void fanControl();
        void readCo2Sensor();

        void init();

        zmq::context_t& ctx_;
        zmq::socket_t mainSocket_;
        zmq::socket_t subSocket_;

        CO2::ThreadFSM* threadState_;

        std::string co2Port_;
        Co2Sensor* co2Sensor_;

        int temperature_;
        int relHumidity_;
        int filterRelHumidity_;
        int co2_;
        int filterCo2_;
        std::atomic<int> relHumidityThreshold_;
        std::atomic<int> co2Threshold_;
        time_t fanOnOverrideTime_;
        bool fanStateOn_;
        std::atomic<Co2Display::FanAutoManStates> fanAutoManState_;
        std::atomic<time_t> fanManOnEndTime_;

        bool hasCo2Config_;
        bool hasFanConfig_;
        const uint32_t kFanGpioPin_;

        time_t kPublishInterval_;
        time_t timeLastPublish_;

        int consecutiveCo2SensorHwErrorCount_;     // used to trigger restart if hardware acting up
        const int kHwErrorThreshold_; // the number of consecutive h/w errors to trigger restart

    protected:
};


#endif /* CO2MONITOR_H */
