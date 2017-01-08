/*
 * co2Monitor.cpp
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#include "co2Monitor.h"
#include <wiringPi.h>
#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>


Co2Monitor::Co2Monitor(zmq::context_t& ctx, int sockType) :
    ctx_(ctx),
    mainSocket_(ctx, sockType),
    subSocket_(ctx, ZMQ_SUB),
    temperature_(0),
    relHumidity_(0),
    filterRelHumidity_(-1),
    co2_(0),
    filterCo2_(-1),
    fanOnOverrideTime_(0),
    fanStateOn_(false),
    fanManOnEndTime_(0),
    hasCo2Config_(false),
    hasFanConfig_(false),
    kFanGpioPin_(17),
    kPublishInterval_(10)  // seconds
{
    threadState_ = new CO2::ThreadFSM("Co2Monitor", &mainSocket_);

    relHumidityThreshold_.store(0, std::memory_order_relaxed);
    co2Threshold_.store(0, std::memory_order_relaxed);
    fanAutoManState_.store(Co2Display::Auto, std::memory_order_relaxed);

    timeLastPublish_ = time(0);
}


Co2Monitor::~Co2Monitor()
{
    // Delete all dynamic memory.
}

std::atomic<bool> Co2Monitor::shouldTerminate_;

void Co2Monitor::getCo2ConfigFromMsg(co2Message::Co2Message& cfgMsg)
{
    DBG_TRACE();

    int rc;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    if (cfgMsg.has_co2config()) {
        const co2Message::Co2Config& co2Cfg = cfgMsg.co2config();

        if (myThreadState == co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {

            if (co2Cfg.has_co2port()) {
                co2Port_ = std::string(co2Cfg.co2port());
            } else {
                throw CO2::exceptionLevel("missing co2 port", true);
            }

            hasCo2Config_ = true;
            if (hasFanConfig_) {
                threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
            }
            syslog(LOG_DEBUG, "Co2Monitor co2 config: Co2 port=\"%s\"", co2Port_.c_str());
        }

    } else {
        syslog(LOG_ERR, "missing Co2Monitor co2 config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }
}

void Co2Monitor::getFanConfigFromMsg(co2Message::Co2Message& cfgMsg)
{
    DBG_TRACE();

    int rc;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    if (cfgMsg.has_fanconfig()) {
        const co2Message::FanConfig& fanCfg = cfgMsg.fanconfig();

        if (myThreadState == co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {

            if (fanCfg.has_fanonoverridetime()) {
                fanOnOverrideTime_ = fanCfg.fanonoverridetime();
            } else {
                throw CO2::exceptionLevel("missing fan on override time", true);
            }

            if (fanCfg.has_relhumfanonthreshold()) {
                relHumidityThreshold_.store(fanCfg.relhumfanonthreshold() * 100, std::memory_order_relaxed);
            } else {
                throw CO2::exceptionLevel("missing rel humidity threshold", true);
            }

            if (fanCfg.has_co2fanonthreshold()) {
                co2Threshold_.store(fanCfg.co2fanonthreshold(), std::memory_order_relaxed);
            } else {
                throw CO2::exceptionLevel("missing CO2 threshold", true);
            }

            hasFanConfig_ = true;
            if (hasCo2Config_) {
                threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
            }
            syslog(LOG_DEBUG,
                   "Co2Monitor fan config: FanOnOverrideTIme=%lu minutes  RelHumThreshold=%u%%  CO2Threshold=%uppm",
                   fanOnOverrideTime_, relHumidityThreshold_.load(std::memory_order_relaxed),
                   co2Threshold_.load(std::memory_order_relaxed));
        } else {
            if (fanCfg.has_fanonoverridetime()) {
                fanOnOverrideTime_ = fanCfg.fanonoverridetime();
            }

            if (fanCfg.has_relhumfanonthreshold()) {
                relHumidityThreshold_.store(fanCfg.relhumfanonthreshold(), std::memory_order_relaxed);
            }

            if (fanCfg.has_co2fanonthreshold()) {
                co2Threshold_.store(fanCfg.co2fanonthreshold(), std::memory_order_relaxed);
            }

            const char* fanOverrideStr = "";
            if (fanCfg.has_fanoverride()) {
                Co2Display::FanAutoManStates newFanAutoManState = fanAutoManState_.load(std::memory_order_relaxed);
                switch (fanCfg.fanoverride()) {
                case co2Message::FanConfig_FanOverride_AUTO:
                    fanOverrideStr = "Auto";
                    newFanAutoManState = Co2Display::Auto;
                    break;

                case co2Message::FanConfig_FanOverride_MANUAL_OFF:
                    fanOverrideStr = "Manual Off";
                    newFanAutoManState = Co2Display::ManOff;
                    break;

                case co2Message::FanConfig_FanOverride_MANUAL_ON:
                    fanOverrideStr = "Manual On";
                    newFanAutoManState = Co2Display::ManOn;
                    break;

                default:
                    syslog(LOG_ERR, "unknown fan override setting (%d)", static_cast<int>(fanCfg.fanoverride()));
                    break;
                }

                if (newFanAutoManState != fanAutoManState_.load(std::memory_order_relaxed)) {
                    updateFanState(newFanAutoManState);
                }
            }

            syslog(LOG_DEBUG,
                   "Co2Monitor fan config: FanOnOverrideTIme=%lu minutes  RelHumThreshold=%u%%"
                   "  CO2Threshold=%uppm  Fan Override is: %s",
                   fanOnOverrideTime_, relHumidityThreshold_.load(std::memory_order_relaxed),
                   co2Threshold_.load(std::memory_order_relaxed), fanOverrideStr);
        }

    } else {
        syslog(LOG_ERR, "missing Co2Monitor fan config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }
}

void Co2Monitor::listener()
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
                syslog(LOG_DEBUG, "co2 monitor thread rx msg (type=%d)", co2Msg.messagetype());

                switch (co2Msg.messagetype()) {
                case co2Message::Co2Message_Co2MessageType_CO2_CFG:
                    getCo2ConfigFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_FAN_CFG:
                    getFanConfigFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_TERMINATE:
                    threadState_->stateEvent(CO2::ThreadFSM::Terminate);
                    shouldTerminate_.store(true, std::memory_order_relaxed);
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
void Co2Monitor::publishCo2State()
{
    time_t timeNow = time(0);

    // Don't publish more then once every 10 seconds
    //
    if ((timeNow - timeLastPublish_) < kPublishInterval_) {
        return;
    }

    DBG_TRACE();

    co2Message::Co2Message co2Msg;

    co2Message::Co2State* co2State = co2Msg.mutable_co2state();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_CO2_STATE);

    co2State->set_temperature(temperature_);
    co2State->set_relhumidity(relHumidity_);
    co2State->set_co2(co2_);

    Co2Display::FanAutoManStates fanAutoManState = fanAutoManState_.load(std::memory_order_relaxed);
    co2Message::Co2State_FanStates fanState = co2Message::Co2State_FanStates_AUTO_OFF;

    if (fanStateOn_) {
        if (fanAutoManState == Co2Display::Auto) {
            fanState = co2Message::Co2State_FanStates_AUTO_ON;
        } else {
            fanState = co2Message::Co2State_FanStates_MANUAL_ON;
        }
    } else if (fanAutoManState != Co2Display::Auto) {
        fanState = co2Message::Co2State_FanStates_MANUAL_OFF;
    }

    co2State->set_fanstate(fanState);

    co2Message::Co2State_Timestamp* timeStamp = co2State->mutable_timestamp();
    timeStamp->set_seconds(static_cast<int>(timeNow));

    std::string co2StateStr;
    co2Msg.SerializeToString(&co2StateStr);

    zmq::message_t co2StateMsg(co2StateStr.size());

    memcpy(co2StateMsg.data(), co2StateStr.c_str(), co2StateStr.size());
    mainSocket_.send(co2StateMsg);

    timeLastPublish_ = timeNow;
}

void Co2Monitor::startFanManOnTimer()
{
    // fanOnOverrideTime is in minutes, so convert to seconds
    fanManOnEndTime_.store(time(0) + (fanOnOverrideTime_ * 60), std::memory_order_relaxed);
}

void Co2Monitor::stopFanManOnTimer()
{
    if (fanManOnEndTime_.load(std::memory_order_relaxed)) {
        fanManOnEndTime_.store(0, std::memory_order_relaxed);
    }
}

void Co2Monitor::updateFanState()
{
    if (fanAutoManState_.load(std::memory_order_relaxed) == Co2Display::ManOn) {

        time_t endTime = fanManOnEndTime_.load(std::memory_order_relaxed);
        time_t timeNow = time(0);

        if (endTime < timeNow) {
            // timer has expired, so Auto/Man state reverts to Auto
            fanManOnEndTime_.store(0, std::memory_order_relaxed);
            stopFanManOnTimer();
            fanAutoManState_.store(Co2Display::Auto, std::memory_order_relaxed);
            syslog(LOG_DEBUG, "fan override timer expired - state now Auto");

            // force early publish
            timeLastPublish_ -= kPublishInterval_;
        }
    }

    fanControl();
}

void Co2Monitor::updateFanState(Co2Display::FanAutoManStates newFanAutoManState)
{

    Co2Display::FanAutoManStates oldFanAutoManState = fanAutoManState_.load(std::memory_order_relaxed);

    if (oldFanAutoManState != newFanAutoManState) {
        const char* fanStateStr = "";
        switch (newFanAutoManState) {
        case Co2Display::Auto:
            if (oldFanAutoManState == Co2Display::ManOn) {
                stopFanManOnTimer();
            }
            fanAutoManState_.store(newFanAutoManState, std::memory_order_relaxed);
            fanStateStr = "Auto";
            break;

        case Co2Display::ManOff:
            if (oldFanAutoManState == Co2Display::ManOn) {
                stopFanManOnTimer();
            }
            fanAutoManState_.store(newFanAutoManState, std::memory_order_relaxed);
            fanStateStr = "Manual Off";
            break;

        case Co2Display::ManOn:
            startFanManOnTimer();
            fanAutoManState_.store(newFanAutoManState, std::memory_order_relaxed);
            fanStateStr = "Manual On";
            break;

        default:
            break;
        }

        fanControl();

        syslog(LOG_DEBUG, "new fan state is: %s (%s)", fanStateStr, fanStateOn_ ? "on" : "off");

        // force early publish
        timeLastPublish_ -= kPublishInterval_;

    } else {
        updateFanState();
    }
}

void Co2Monitor::fanControl()
{
    bool newFanStateOn = fanStateOn_;

    do { // once
        if (fanAutoManState_.load(std::memory_order_relaxed) == Co2Display::ManOff) {
            if (fanStateOn_) {
                // turn fan off
                newFanStateOn = false;
                syslog(LOG_DEBUG, "%s - fan on -> off (man)", __FUNCTION__);
            }
            break;
        }

        if (fanAutoManState_.load(std::memory_order_relaxed) == Co2Display::ManOn) {
            if (!fanStateOn_) {
                // turn fan on
                newFanStateOn = true;
                syslog(LOG_DEBUG, "%s - fan off -> on (man)", __FUNCTION__);
            }
            break;
        }

        // auto
        if ((filterRelHumidity_ > relHumidityThreshold_.load(std::memory_order_relaxed)) ||
            (filterCo2_ > co2Threshold_.load(std::memory_order_relaxed)) ) {
            if (!fanStateOn_) {
                // turn fan on
                newFanStateOn = true;
                syslog(LOG_DEBUG, "%s - fan off -> on (auto)", __FUNCTION__);
            }
        } else {
            if (fanStateOn_) {
                // turn fan off
                newFanStateOn = false;
                syslog(LOG_DEBUG, "%s - fan on -> off (auto)", __FUNCTION__);
            }
        }

    } while (false);

    if (newFanStateOn != fanStateOn_) {
        digitalWrite(kFanGpioPin_, newFanStateOn ? 1 : 0);
        syslog(LOG_DEBUG, "turned fan %s", newFanStateOn ? "on" : "off");
        fanStateOn_ = newFanStateOn;
    }
}

void Co2Monitor::readCo2Sensor()
{
    int returnVal;

    returnVal = co2Sensor_->readTemperature();
    if (returnVal >= 0) {
        temperature_ = returnVal;
    } else {
        syslog(LOG_ERR, "co2Sensor->readTemperature() returned error (%d)", returnVal);
    }

    returnVal = co2Sensor_->readRelHumidity();
    if (returnVal >= 0) {
        relHumidity_ = returnVal;
    } else {
        syslog(LOG_ERR, "co2Sensor->readRelHumidity() returned error (%d)", returnVal);
    }

    returnVal = co2Sensor_->readCo2ppm();
    if (returnVal >= 0) {
        co2_ = returnVal;
    } else {
        syslog(LOG_ERR, "co2Sensor->readCo2ppm() returned error (%d)", returnVal);
    }
}

void Co2Monitor::init()
{
    DBG_TRACE();
#ifndef __RPI3__
    co2Sensor_ = new Co2Sensor(co2Port_);
    if (co2Sensor_) {
        co2Sensor_->init();
    } else {
        throw CO2::exceptionLevel("Unable to initialise CO2 sensor", true);
    }
#endif
    // setup GPIO pin to control fan
    pinMode(kFanGpioPin_, OUTPUT);
    digitalWrite(kFanGpioPin_, 1);
}

void Co2Monitor::run()
{
    DBG_TRACE_MSG("Start of Co2Monitor::run");

    std::thread* touchScreenThread;
    const char* threadName;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    /**************************************************************************/
    /*                                                                        */
    /* Start listener thread and await config                                 */
    /*                                                                        */
    /**************************************************************************/
    std::thread* listenerThread = new std::thread(std::bind(&Co2Monitor::listener, this));

    // mainSocket is used to send status to main thread
    mainSocket_.connect(CO2::co2MonEndpoint);

    threadState_->stateEvent(CO2::ThreadFSM::ReadyForConfig);
    if (threadState_->stateChanged()) {
        myThreadState = threadState_->state();
    }

    // We'll continue after receiving our configuration
    while (!threadState_->stateChanged()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    myThreadState = threadState_->state();
    syslog(LOG_DEBUG, "Co2Monitor state=%s", CO2::stateStr(myThreadState));

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STARTED) {

        /**************************************************************************/
        /*                                                                        */
        /* Initialise  (if config received OK)           */
        /*                                                                        */
        /**************************************************************************/
        try {
            init();
        } catch (CO2::exceptionLevel& el) {
            if (el.isFatal()) {
                syslog(LOG_ERR, "Fatal exception initialising co2 monitor: %s", el.what());
                threadState_->stateEvent(CO2::ThreadFSM::InitFail);
                shouldTerminate_.store(true, std::memory_order_relaxed);
            }
            syslog(LOG_ERR, "exception initialising co2 monitor: %s", el.what());
        } catch (...) {
            syslog(LOG_ERR, "Exception initialising co2 monitor");
            threadState_->stateEvent(CO2::ThreadFSM::InitFail);
            shouldTerminate_.store(true, std::memory_order_relaxed);
        }

    } else {
        syslog(LOG_ERR, "Display thread failed to get config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }


    myThreadState = threadState_->state();

    // we are now ready to roll
    threadState_->stateEvent(CO2::ThreadFSM::InitOk);
    myThreadState = threadState_->state();

    /**************************************************************************/
    /*                                                                        */
    /* This is the main run loop.                                             */
    /*                                                                        */
    /**************************************************************************/
    try {
#ifdef __RPI3__
        int i = 0;
#endif
        time_t publishIntervalCounter = 0;
        while (!shouldTerminate_.load(std::memory_order_relaxed))
        {
            if (++publishIntervalCounter >= kPublishInterval_) {
#ifdef __RPI3__
                temperature_ = 1234 + (100 * (i % 18)) + (i % 23);
                relHumidity_ = 3456 + (100 * (i % 27)) + (i % 19);
                co2_ = 250 + (i % 450);
                i++;
#else

                readCo2Sensor();
#endif
                if (filterRelHumidity_ >= 0) {
                    filterRelHumidity_ = ((relHumidity_ * 20) + (filterRelHumidity_ * 80)) / 100;
                } else {
                    filterRelHumidity_ = relHumidity_;
                }

                if (filterCo2_ >= 0) {
                    filterCo2_ = ((co2_ * 20) + (filterCo2_ * 80)) / 100;
                } else {
                    filterCo2_ = co2_;
                }

                updateFanState();

                publishIntervalCounter = 0;
            }

            publishCo2State();

            std::this_thread::sleep_for(std::chrono::seconds(1));
        } // end main run loop

    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_ERR, "Fatal exception in Co2Monitor run loop: %s", el.what());
            throw;
        }
        syslog(LOG_ERR, "exception starting in Co2Monitor run loop: %s", el.what());
    } catch (...) {
        syslog(LOG_ERR, "Exception in Co2Monitor run loop");
        throw;
    }

    /**************************************************************************/
    /*                                                                        */
    /* end of main run loop.                                                  */
    /*                                                                        */
    /**************************************************************************/
    DBG_TRACE_MSG("end of Co2Monitor::run loop");

    // remember to turn fan off
    digitalWrite(kFanGpioPin_, 0);

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STOPPING) {
        threadState_->stateEvent(CO2::ThreadFSM::Timeout);
    }
}



