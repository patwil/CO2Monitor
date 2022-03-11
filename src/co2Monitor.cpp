/*
 * co2Monitor.cpp
 *
 * Created on: 2016-02-13
 *     Author: patw
 */

#include <fstream>
#include <filesystem>
#include <thread>          // std::thread
#include <fcntl.h>
#include <syslog.h>
#include <fmt/core.h>
#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

#include "co2Monitor.h"
#include "co2SensorK30.h"
#include "co2SensorSCD30.h"
#include "co2SensorSim.h"


namespace fs = std::filesystem;

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
    kFanGpioPin_(Co2Display::GPIO_FanControl),
    kPublishInterval_(10),  // seconds
    consecutiveCo2SensorHwErrorCount_(0),
    kHwErrorThreshold_(3)
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

std::mutex Co2Monitor::fanControlMutex_;
std::atomic<bool> Co2Monitor::shouldTerminate_;

void Co2Monitor::getCo2ConfigFromMsg(co2Message::Co2Message& cfgMsg)
{
    DBG_TRACE();

    int rc;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    if (cfgMsg.has_co2config()) {
        const co2Message::Co2Config& co2Cfg = cfgMsg.co2config();

        if (myThreadState == co2Message::ThreadState_ThreadStates_AWAITING_CONFIG) {

            if (co2Cfg.has_sensortype()) {
                sensorType_ = std::string(co2Cfg.sensortype());
            } else {
                throw CO2::exceptionLevel("missing sensor type", true);
            }

            // We'll check sensor port later when instantiating sensor.
            if (co2Cfg.has_sensorport()) {
                sensorPort_ = std::string(co2Cfg.sensorport());
            }

            if (co2Cfg.has_co2monlogbasedir()) {
                co2LogBaseDirStr_ = std::string(co2Cfg.co2monlogbasedir());
            } else {
                throw CO2::exceptionLevel("missing CO2 log base dir", true);
            }

            hasCo2Config_ = true;
            if (hasFanConfig_) {
                // We have received both sets of config, so
                // every little thing is gonna be all right.
                threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
            }
            syslog(LOG_DEBUG, "Co2Monitor co2 config: CO2 Sensor=\"%s\"", sensorType_.c_str());
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

            if (fanCfg.has_fanoverride()) {
                updateFanState(fanCfg.fanoverride());
            }

            hasFanConfig_ = true;
            if (hasCo2Config_) {
                // We have received both sets of config, so
                // every little thing is gonna be all right.
                threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
            }
            syslog(LOG_DEBUG,
                   "Co2Monitor fan config: FanOnOverrideTIme=%lu minutes  RelHumThreshold=%u%%  CO2Threshold=%uppm",
                   fanOnOverrideTime_, relHumidityThreshold_.load(std::memory_order_relaxed)/100,
                   co2Threshold_.load(std::memory_order_relaxed));
        } else {
            if (fanCfg.has_fanonoverridetime()) {
                fanOnOverrideTime_ = fanCfg.fanonoverridetime();
            }

            if (fanCfg.has_relhumfanonthreshold()) {
                relHumidityThreshold_.store(fanCfg.relhumfanonthreshold() * 100, std::memory_order_relaxed);
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
                DBG_MSG(LOG_DEBUG, "co2 monitor thread rx msg (type=%d)", co2Msg.messagetype());

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
    mainSocket_.send(co2StateMsg, zmq::send_flags::none);

    // Store readings in co2LogBaseDirStr_/YYYY/MM/DD (Base dir is in config file.)
    //
    // If date changed since last time we need to create new file and,
    // if month changed, create new directory.
    //
    do { // once
        if (filterRelHumidity_ > 0) {
            struct tm tmThen;
            struct tm tmNow;
            std::string filePathStr;
            int mode = R_OK|W_OK|X_OK;
            int dirMode = 0755;


            if ( timeLastPublish_ && timeNow &&
                 localtime_r(&timeLastPublish_, &tmThen) &&
                 localtime_r(&timeNow, &tmNow) ) {

                filePathStr = fmt::format("{}/{}/{}/{}", 
                                          co2LogBaseDirStr_,
                                          CO2::zeroPadNumber(2, tmNow.tm_year + 1900),
                                          CO2::zeroPadNumber(2, tmNow.tm_mon+1),
                                          CO2::zeroPadNumber(2, tmNow.tm_mday));
                // Create parent directory if necessary
                fs::path filePath(filePathStr);
                if (!fs::exists(filePath.parent_path())) {
                    if (!fs::create_directories(filePath.parent_path())) {
                        syslog(LOG_ERR, "Unable to create directory \"%s\"", filePath.parent_path().c_str());
                        break;
                    }
                }
            }
            std::ofstream co2Log;
            co2Log.open(filePathStr, std::ios::out | std::ios::app);
            if (co2Log.is_open()) {
                co2Log << temperature_ << ","
                    << relHumidity_ << ","
                    << co2_ << ","
                    << (fanStateOn_ ? "on" : "off") << ","
                    << (fanAutoManState == Co2Display::Auto ? "auto" : "man") << ","
                    << filterRelHumidity_ << ","
                    << filterCo2_ << ","
                    << timeNow << std::endl << std::flush;
                co2Log.close();
            }
        }
    } while (false);

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

        bool oldFanStateOn = fanStateOn_;
        fanControl();

        DBG_MSG(LOG_DEBUG, "new fan state is: %s (%s => %s)", fanStateStr, oldFanStateOn ? "on" : "off", fanStateOn_ ? "on" : "off");

        // force early publish
        timeLastPublish_ -= kPublishInterval_;

    } else {
        updateFanState();
    }
}

void Co2Monitor::updateFanState(co2Message::FanConfig_FanOverride fanOverride)
{
    Co2Display::FanAutoManStates fanAutoManState;

    switch (fanOverride) {
    case co2Message::FanConfig_FanOverride_AUTO:
        fanAutoManState = Co2Display::Auto;
        break;
    case co2Message::FanConfig_FanOverride_MANUAL_OFF:
        fanAutoManState = Co2Display::ManOff;
        break;
    case co2Message::FanConfig_FanOverride_MANUAL_ON:
        fanAutoManState = Co2Display::ManOn;
        break;
    default:
        throw CO2::exceptionLevel(fmt::format("{}: unknown fan override ({})", __FUNCTION__, static_cast<int>(fanOverride)), false);
    }

    updateFanState(fanAutoManState);
}

void Co2Monitor::fanControl()
{
    std::lock_guard<std::mutex> lock(Co2Monitor::fanControlMutex_);

    bool newFanStateOn = fanStateOn_;

    do { // once
        Co2Display::FanAutoManStates fanAutoManState = fanAutoManState_.load(std::memory_order_relaxed);

        if (fanAutoManState == Co2Display::ManOff) {
            if (fanStateOn_) {
                // turn fan off
                newFanStateOn = false;
            }
            break;
        } else if (fanAutoManState == Co2Display::ManOn) {
            if (!fanStateOn_) {
                // turn fan on
                newFanStateOn = true;
            }
            break;
        } else if (fanAutoManState == Co2Display::Auto) {
            if ((filterRelHumidity_ > relHumidityThreshold_.load(std::memory_order_relaxed)) ||
                (filterCo2_ > co2Threshold_.load(std::memory_order_relaxed)) ) {
                if (!fanStateOn_) {
                    // turn fan on
                    newFanStateOn = true;
                }
            } else {
                if (fanStateOn_) {
                    // turn fan off
                    newFanStateOn = false;
                }
            }
        }
    } while (false);

    if (newFanStateOn != fanStateOn_) {
#ifdef HAS_WIRINGPI
        digitalWrite(kFanGpioPin_, newFanStateOn ? 1 : 0);
#endif
        fanStateOn_ = newFanStateOn;
    }
}

void Co2Monitor::initCo2Sensor()
{
    int permittedInitFails = 3;
    while (true) {
        try {
            co2Sensor_->init();
            return;
        } catch (CO2::exceptionLevel& el) {
            if ( (--permittedInitFails <= 0) || el.isFatal() ) {
                throw;
            }
        } catch (...) {
            throw;
        }
    }
}

void Co2Monitor::readCo2Sensor()
{
    int co2ppm;
    int t;
    int rh;

    try {
        co2Sensor_->readMeasurements(co2ppm, t, rh);
        // ignore readings if co2ppm is 0 as it
        // probably means that sensor is not ready
        if (co2ppm) {
            consecutiveCo2SensorHwErrorCount_ = 0;
            co2_ = co2ppm;
            temperature_ = t;
            relHumidity_ = rh;
        }
    } catch (CO2::exceptionLevel& el) {
        consecutiveCo2SensorHwErrorCount_++;
        syslog(LOG_DEBUG, "%s: consecutive error count = %d", el.what(), consecutiveCo2SensorHwErrorCount_);
        if (el.isFatal()) {
            throw;
        }
        // non fatal sensor errors might be cured
        // with re-initialisation.
        initCo2Sensor();
    } catch (...) {
        throw;
    }
}

void Co2Monitor::init()
{
    DBG_TRACE();

    if (sensorType_ == "K30") {
        if (!sensorPort_.empty()) {
            co2Sensor_ = new Co2SensorK30(sensorPort_);
        } else {
            throw CO2::exceptionLevel("No sensor port configured for K30 sensor", true);
        }
    } else if (sensorType_ == "SCD30") {
        if (!sensorPort_.empty()) {
            if (sensorPort_ == "I2C") {
#ifdef HAS_I2C
                // find the I2C bus to which the device is attached
                int i2cBus = Co2SensorSCD30::findI2cBus();
                if (i2cBus >= 0) {
                    co2Sensor_ = new Co2SensorSCD30(i2cBus);
                } else {
                    throw CO2::exceptionLevel("SCD30 sensor not found on any I2C bus", true);
                }
#else
                throw CO2::exceptionLevel("No I2C support for SCD30 sensor", true);
#endif /* HAS_I2C */
            } else {
                std::string errStr = "Unsupported port (" + sensorPort_ + ") for SCD30 sensor";
                throw CO2::exceptionLevel(errStr, true);
            }
        } else {
            throw CO2::exceptionLevel("No sensor port configured for SCD30 sensor", true);
            co2Sensor_ = new Co2SensorSCD30(sensorPort_);
        }
    } else if (sensorType_ == "sim") {
        co2Sensor_ = new Co2SensorSim();
    }

    if (co2Sensor_) {
        initCo2Sensor();
    } else {
        throw CO2::exceptionLevel("Unable to initialise CO2 sensor", true);
    }
}

void Co2Monitor::run()
{
    DBG_TRACE_MSG("Start of Co2Monitor::run");

    std::thread* touchScreenThread;
    const char* threadName;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

#ifdef HAS_WIRINGPI
    // setup GPIO pin to control fan
    pinMode(kFanGpioPin_, OUTPUT);
    digitalWrite(kFanGpioPin_, 0);
#endif

    /**************************************************************************/
    /*                                                                        */
    /* Start listener thread and await config                                 */
    /*                                                                        */
    /**************************************************************************/
    std::thread* listenerThread = new std::thread(&Co2Monitor::listener, this);

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
        /* Initialise  (if config received OK)                                    */
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
        time_t publishIntervalCounter = 0;
        while (!shouldTerminate_.load(std::memory_order_relaxed))
        {
            if (++publishIntervalCounter >= kPublishInterval_) {

                readCo2Sensor();
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

            if (consecutiveCo2SensorHwErrorCount_ > kHwErrorThreshold_) {
                // we need to try restarting to try to fix hardware error
                threadState_->stateEvent(CO2::ThreadFSM::HardwareFail);
                myThreadState = threadState_->state();
                continue;
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
#ifdef HAS_WIRINGPI
    digitalWrite(kFanGpioPin_, 0);
#endif

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STOPPING) {
        threadState_->stateEvent(CO2::ThreadFSM::Timeout);
    }
}



