/*
 * co2Display.cpp
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#include <stdlib.h>
#include <syslog.h>
#include <exception>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

#include <linux/version.h>
#include <linux/input.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <signal.h>

#include <stropts.h>
#include <string.h>
#include <time.h>
#include "co2TouchScreen.h"
#include "co2Screen.h"
#include "screenBacklight.h"
#include "co2Display.h"

Co2Display::Co2Display(zmq::context_t& ctx, int sockType) :
    ctx_(ctx),
    mainSocket_(ctx, sockType),
    subSocket_(ctx, ZMQ_SUB),
    statusScreen_(nullptr),
    relHumCo2ThresholdScreen_(nullptr),
    fanControlScreen_(nullptr),
    shutdownRestartScreen_(nullptr),
    confirmCancelScreen_(nullptr),
    blankScreen_(nullptr),
    splashScreen_(nullptr),
    currentScreen_(Splash_Screen),
    hasUIConfig_(false),
    hasFanConfig_(false),
    screenRefreshRate_(0),
    screenTimeout_(0),
    timerId_(0),
    relHumThreshold_(0),
    relHumThresholdChanged_(false),
    co2Threshold_(0),
    co2ThresholdChanged_(false),
    fanAutoManStateChanged_(false),
    timeLastUiPublish_(0),
    kPublishInterval_(2)  // seconds
{
    threadState_ = new CO2::ThreadFSM("Co2Display", &mainSocket_);

    temperature_.store(0, std::memory_order_relaxed);
    temperatureChanged_.store(false, std::memory_order_relaxed);
    relHumidity_.store(0, std::memory_order_relaxed);
    relHumChanged_.store(false, std::memory_order_relaxed);
    co2_.store(0, std::memory_order_relaxed);
    co2Changed_.store(false, std::memory_order_relaxed);
    fanStateOn_.store(false, std::memory_order_relaxed);
    fanStateChanged_.store(false, std::memory_order_relaxed);
    fanAutoManStateChangeReq_.store(Auto, std::memory_order_relaxed);
    fanAutoManState_.store(Auto, std::memory_order_relaxed);
    wifiStateOn_.store(false, std::memory_order_relaxed);
    wifiStateChanged_.store(false, std::memory_order_relaxed);

    fontName_ = std::string("FreeSans.ttf");

    screenRefreshRate_ = 15;
    screenTimeout_ = 30;

    screenSize_ = { 0, 0 };
    bitDepth_ = 0;

    statusScreen_ = new StatusScreen;
    relHumCo2ThresholdScreen_ = new RelHumCo2ThresholdScreen;
    fanControlScreen_ = new FanControlScreen;
    shutdownRestartScreen_ = new ShutdownRebootScreen;
    confirmCancelScreen_ = new ConfirmCancelScreen;
    blankScreen_ = new BlankScreen;
    splashScreen_ = new SplashScreen;
    screens_ = {
        statusScreen_,
        relHumCo2ThresholdScreen_,
        fanControlScreen_,
        shutdownRestartScreen_,
        confirmCancelScreen_,
        confirmCancelScreen_,
        blankScreen_,
        splashScreen_
    };

    touchScreen_ = new Co2TouchScreen;
    backlight_ = new ScreenBacklight;

    // set up signal handler. We only have one
    // to handle all trapped signals
    //
    struct sigaction action;
    struct sigaction oldAction;
    //
    action.sa_handler = Co2Display::sigHandler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGHUP, &action, &oldAction);
    if (oldAction.sa_handler) {
        syslog(LOG_INFO, "Replacing signal handler for SIG_HUP");
    }
    //sigaction(SIGINT, &action, 0);
    //sigaction(SIGQUIT, &action, 0);
    //sigaction(SIGTERM, &action, 0);

    disableSDLCleanUp_ = true;
    shouldTerminate_.store(false, std::memory_order_relaxed);
}

Co2Display::~Co2Display()
{
    // Delete all dynamic memory.
    delete touchScreen_;
    delete backlight_;

    delete statusScreen_;
    delete relHumCo2ThresholdScreen_;
    delete fanControlScreen_;
    delete shutdownRestartScreen_;
    delete confirmCancelScreen_;
    delete blankScreen_;

    delete threadState_;
}

bool Co2Display::disableSDLCleanUp_;
std::atomic<bool> Co2Display::shouldTerminate_;

void Co2Display::init()
{
    DBG_TRACE_MSG("Start of Co2Display::init");

    if (!hasUIConfig_) {
        throw CO2::exceptionLevel("called init() before receiving UI config", true);
    }

    if (!hasFanConfig_) {
        throw CO2::exceptionLevel("called init() before receiving fan config", true);
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    // Initialize SDL_ttf library
    if (TTF_Init())
    {
        SDL_Quit();
        throw CO2::exceptionLevel("failed to init ttf");
    }

    if (atexit(Co2Display::sdlCleanUp)) {
        throw CO2::exceptionLevel("unable to set exit function", true);
    }
    Co2Display::disableSDLCleanUp_ = false;

    std::string fontFile = sdlTTFDir_ + std::string("/") + fontName_;
    fonts_[Small].size = 24; // point
    fonts_[Medium].size = 30; // point
    fonts_[Large].size = 40; // point

    for (auto& font: fonts_) {
        font.font = TTF_OpenFont(fontFile.c_str(), font.size);
        if (!font.font) {
            syslog(LOG_ERR, "TTF_OpenFont() Failed \"%s\" (fontSize=%d): %s", fontFile.c_str(), font.size, TTF_GetError());
            throw CO2::exceptionLevel("TTF_OpenFont error", true);
        }
    }

    SDL_ShowCursor(SDL_DISABLE);

    screen_ = SDL_SetVideoMode(screenSize_.x, screenSize_.y, bitDepth_, 0);

    std::string sdlBitMapDir = sdlBMPDir_ + "/";
    statusScreen_->init(screen_, sdlBitMapDir, &fonts_);
    relHumCo2ThresholdScreen_->init(screen_, sdlBitMapDir, &fonts_);
    fanControlScreen_->init(screen_, sdlBitMapDir, &fonts_);
    shutdownRestartScreen_->init(screen_, sdlBitMapDir, &fonts_);
    confirmCancelScreen_->init(screen_, sdlBitMapDir, &fonts_);
    blankScreen_->init(screen_, sdlBitMapDir, &fonts_);
    splashScreen_->init(screen_, sdlBitMapDir, &fonts_);

    currentScreen_ =  Splash_Screen;

    touchScreen_->init(mouseDev_);
    touchScreen_->buttonInit();
    backlight_->init(screenTimeout_);

    fanAutoManState_.store(Auto, std::memory_order_relaxed);
    wifiStateOn_.store(false, std::memory_order_relaxed);

    statusScreen_->setTemperature(temperature_.load(std::memory_order_relaxed));
    statusScreen_->setRelHumidity(relHumidity_.load(std::memory_order_relaxed));
    statusScreen_->setCo2(co2_.load(std::memory_order_relaxed));
    statusScreen_->setFanState(fanStateOn_.load(std::memory_order_relaxed));
    statusScreen_->setFanAuto(fanAutoManState_.load(std::memory_order_relaxed));
    statusScreen_->setWiFiState(wifiStateOn_.load(std::memory_order_relaxed));

    relHumCo2ThresholdScreen_->setRelHumThreshold(relHumThreshold_);
    relHumCo2ThresholdScreen_->setCo2Threshold(co2Threshold_);

    fanControlScreen_->setFanAuto(fanAutoManState_.load(std::memory_order_relaxed));
}

void Co2Display::setScreenSize(std::string fbFilename)
{
    struct fb_var_screeninfo fbVarScreenInfo;
    int fd;

    if ((fd = open(fbFilename.c_str(), O_RDONLY)) == -1) {
        syslog(LOG_ERR, "open %s: %s\n", fbFilename.c_str(), strerror(errno));
        return;
    }

    if (!ioctl(fd, FBIOGET_VSCREENINFO, &fbVarScreenInfo)) {
        screenSize_.x = fbVarScreenInfo.xres;
        screenSize_.y = fbVarScreenInfo.yres;
        bitDepth_ = fbVarScreenInfo.bits_per_pixel;
        syslog(LOG_DEBUG, "Screen: width=%d  height=%d  depth=%d  ", screenSize_.x, screenSize_.y, bitDepth_);
    } else {
        syslog(LOG_ERR, "ioctl %s: %s\n", fbFilename.c_str(), strerror(errno));
        close(fd);
        return;
    }

    close(fd);
}

void Co2Display::screenFSM(Co2Display::ScreenEvents event)
{
    ScreenNames newScreen = currentScreen_;

    switch (currentScreen_) {
    case Status_Screen:
        switch (event) {
        case ButtonPush_1:
            break;
        case ButtonPush_2:
            newScreen = RelHumCo2Threshold_Screen;
            break;
        case ButtonPush_3:
            newScreen = FanControl_Screen;
            break;
        case ButtonPush_4:
            newScreen = ShutdownReboot_Screen;
            break;
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            break;
        default:
            break;
        }
        break;

    case RelHumCo2Threshold_Screen:
        switch (event) {
        case ButtonPush_1:
            newScreen = Status_Screen;
            break;
        case ButtonPush_2:
            break;
        case ButtonPush_3:
            newScreen = FanControl_Screen;
            break;
        case ButtonPush_4:
            newScreen = ShutdownReboot_Screen;
            break;
        case RelHumUp:
            if (CO2::isInRange("RelHumFanOnThreshold", relHumThreshold_+1)) {
                relHumCo2ThresholdScreen_->setRelHumThreshold(++relHumThreshold_);
                relHumThresholdChanged_ = true;
            }
            break;
        case RelHumDown:
            if (CO2::isInRange("RelHumFanOnThreshold", relHumThreshold_-1)) {
                relHumCo2ThresholdScreen_->setRelHumThreshold(--relHumThreshold_);
                relHumThresholdChanged_ = true;
            }
            break;
        case Co2Up:
            if (CO2::isInRange("CO2FanOnThreshold", co2Threshold_+1)) {
                relHumCo2ThresholdScreen_->setCo2Threshold(++co2Threshold_);
                co2ThresholdChanged_ = true;
            }
            break;
        case Co2Down:
            if (CO2::isInRange("CO2FanOnThreshold", co2Threshold_-1)) {
                relHumCo2ThresholdScreen_->setCo2Threshold(--co2Threshold_);
                co2ThresholdChanged_ = true;
            }
            break;
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    case FanControl_Screen:
        switch (event) {
        case ButtonPush_1:
            newScreen = Status_Screen;
            break;
        case ButtonPush_2:
            newScreen = RelHumCo2Threshold_Screen;
            break;
        case ButtonPush_3:
            break;
        case ButtonPush_4:
            newScreen = ShutdownReboot_Screen;
            break;
        case FanOn:
            fanAutoManStateChangeReq_.store(ManOn, std::memory_order_relaxed);
            fanControlScreen_->setFanAuto(fanAutoManStateChangeReq_.load(std::memory_order_relaxed));
            fanAutoManStateChanged_ = true;
            break;
        case FanOff:
            fanAutoManStateChangeReq_.store(ManOff, std::memory_order_relaxed);
            fanControlScreen_->setFanAuto(fanAutoManStateChangeReq_.load(std::memory_order_relaxed));
            fanAutoManStateChanged_ = true;
            break;
        case FanAuto:
            fanAutoManStateChangeReq_.store(Auto, std::memory_order_relaxed);
            fanControlScreen_->setFanAuto(fanAutoManStateChangeReq_.load(std::memory_order_relaxed));
            fanAutoManStateChanged_ = true;
            break;
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    case ShutdownReboot_Screen:
        switch (event) {
        case ButtonPush_1:
            newScreen = Status_Screen;
            break;
        case ButtonPush_2:
            newScreen = RelHumCo2Threshold_Screen;
            break;
        case ButtonPush_3:
            newScreen = FanControl_Screen;
            break;
        case ButtonPush_4:
            break;
        case Reboot:
            newScreen = ConfirmCancelReboot_Screen;
            confirmCancelScreen_->setConfirmAction(event);
            break;
        case Shutdown:
            newScreen = ConfirmCancelShutdown_Screen;
            confirmCancelScreen_->setConfirmAction(event);
            break;
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    case ConfirmCancelReboot_Screen:
        switch (event) {
        case ButtonPush_1:
            newScreen = Status_Screen;
            break;
        case ButtonPush_2:
            newScreen = RelHumCo2Threshold_Screen;
            break;
        case ButtonPush_3:
            newScreen = FanControl_Screen;
            break;
        case ButtonPush_4:
        case Cancel:
            newScreen = ShutdownReboot_Screen;
            break;
        case Confirm:
            newScreen = Blank_Screen;
            syslog(LOG_DEBUG, "Restart confirmed");
            sendShutdownMsg(true);
            break;
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    case ConfirmCancelShutdown_Screen:
        switch (event) {
        case ButtonPush_1:
            newScreen = Status_Screen;
            break;
        case ButtonPush_2:
            newScreen = RelHumCo2Threshold_Screen;
            break;
        case ButtonPush_3:
            newScreen = FanControl_Screen;
            break;
        case ButtonPush_4:
        case Cancel:
            newScreen = ShutdownReboot_Screen;
            break;
        case Confirm:
            newScreen = Blank_Screen;
            syslog(LOG_DEBUG, "Shutdown confirmed");
            sendShutdownMsg(false);
            break;
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    case Blank_Screen:
        switch (event) {
        case ScreenBacklightOff:
            break;
        case ScreenBacklightOn:
            newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    case Splash_Screen:
        switch (event) {
        case ScreenBacklightOff:
            newScreen = Blank_Screen;
            break;
        case ScreenBacklightOn:
            // newScreen = Status_Screen;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    if (newScreen != currentScreen_) {
        currentScreen_ = newScreen;
        DBG_MSG(LOG_DEBUG, "new screen = %d", static_cast<int>(newScreen));
        screens_[currentScreen_]->setNeedsRedraw();
        drawScreen(false);
    }
}

void Co2Display::sdlCleanUp()
{
    if (!Co2Display::disableSDLCleanUp_) {
        TTF_Quit();
        SDL_Quit();
    }
}

void Co2Display::drawScreen(bool refreshOnly)
{
    switch (currentScreen_) {
    case Status_Screen:
        statusScreen_->draw(refreshOnly);
        break;

    case RelHumCo2Threshold_Screen:
        relHumCo2ThresholdScreen_->draw(refreshOnly);
        break;

    case FanControl_Screen:
        fanControlScreen_->draw(refreshOnly);
        break;

    case ShutdownReboot_Screen:
        shutdownRestartScreen_->draw(refreshOnly);
        break;

    case ConfirmCancelReboot_Screen:
    case ConfirmCancelShutdown_Screen:
        confirmCancelScreen_->draw(refreshOnly);
        break;

    case Blank_Screen:
        blankScreen_->draw(refreshOnly);
        break;

    case Splash_Screen:
        splashScreen_->draw(refreshOnly);
        break;

    default:
        break;
    }
}

Co2Display::ScreenEvents Co2Display::getScreenEvent(SDL_Point pos)
{
    ScreenEvents event = None;

    switch (currentScreen_) {
    case Status_Screen:
        event = statusScreen_->getScreenEvent(pos);
        break;

    case RelHumCo2Threshold_Screen:
        event = relHumCo2ThresholdScreen_->getScreenEvent(pos);
        break;

    case FanControl_Screen:
        event = fanControlScreen_->getScreenEvent(pos);
        break;

    case ShutdownReboot_Screen:
        event = shutdownRestartScreen_->getScreenEvent(pos);
        break;

    case ConfirmCancelReboot_Screen:
    case ConfirmCancelShutdown_Screen:
        event = confirmCancelScreen_->getScreenEvent(pos);
        break;

    case Blank_Screen:
        event = blankScreen_->getScreenEvent(pos);
        break;

    case Splash_Screen:
        event = splashScreen_->getScreenEvent(pos);
        break;

    default:
        break;
    }

    return event;
}

void Co2Display::getUIConfigFromMsg(co2Message::Co2Message& cfgMsg)
{
    DBG_TRACE();

    int rc;

    if (cfgMsg.has_uiconfig()) {
        const co2Message::UIConfig& uiCfg = cfgMsg.uiconfig();

        if (uiCfg.has_fbdev()) {
            std::string fbDev = uiCfg.fbdev();
            rc = setenv("SDL_FBDEV", fbDev.c_str(), 0);

            setScreenSize(fbDev);

            if ( !bitDepth_ || !screenSize_.x || !screenSize_.y){
                syslog(LOG_ERR, "Error setting screen size/depth from %s: bits-per-pixel=%d  width=%d  height=%d",
                       fbDev.c_str(), bitDepth_, screenSize_.x, screenSize_.y);
                throw CO2::exceptionLevel("unable to read screen size and/or depth", true);
            }

            if (rc) {
                syslog(LOG_ERR, "setenv(\"SDL_FBDEV\") returned error (%d)\n", rc);
                throw CO2::exceptionLevel("setenv SDL_FBDEV error", true);
            }
        } else {
            throw CO2::exceptionLevel("missing framebuffer device", true);
        }

        if (uiCfg.has_mousedev()) {
            mouseDev_ = uiCfg.mousedev();
            rc = setenv("SDL_MOUSEDEV", mouseDev_.c_str(), 0);

            if (rc) {
                syslog(LOG_ERR, "setenv(\"SDL_MOUSEDEV\") returned error (%d)\n", rc);
                throw CO2::exceptionLevel("setenv SDL_MOUSEDEV error", true);
            }
        } else {
            throw CO2::exceptionLevel("missing mouse device", true);
        }

        if (uiCfg.has_mousedrv()) {
            //std::string mouseDrv = uiCfg.mousedrv();
            rc = setenv("SDL_MOUSEDRV", uiCfg.mousedrv().c_str(), 0);

            if (rc) {
                syslog(LOG_ERR, "setenv(\"SDL_MOUSEDRV\") returned error (%d)\n", rc);
            }
        } else {
            throw CO2::exceptionLevel("missing mouse driver", true);
        }

        if (uiCfg.has_mouserelative()) {
            //std::string mouseRel = uiCfg.mouserelative();
            rc = setenv("SDL_MOUSE_RELATIVE", uiCfg.mouserelative().c_str(), 0);

            if (rc) {
                syslog(LOG_ERR, "setenv(\"SDL_MOUSE_RELATIVE\") returned error (%d)\n", rc);
            }
        } else {
            throw CO2::exceptionLevel("missing mouse relative", true);
        }

        if (uiCfg.has_videodriver()) {
            //std::string videoDrv = uiCfg.videodriver();
            rc = setenv("SDL_VIDEODRIVER", uiCfg.videodriver().c_str(), 0);

            if (rc) {
                syslog(LOG_ERR, "setenv(\"SDL_VIDEODRIVER\") returned error (%d)\n", rc);
                throw CO2::exceptionLevel("setenv SDL_VIDEODRIVER error", true);
            }
        } else {
            throw CO2::exceptionLevel("missing video driver", true);
        }

        if (uiCfg.has_ttfdir()) {
            sdlTTFDir_ = uiCfg.ttfdir();
        } else {
            throw CO2::exceptionLevel("missing TTF dir", true);
        }

        if (uiCfg.has_bitmapdir()) {
            sdlBMPDir_ = uiCfg.bitmapdir();
        } else {
            throw CO2::exceptionLevel("missing BMP dir", true);
        }

        if (uiCfg.has_screenrefreshrate()) {
            screenRefreshRate_ = uiCfg.screenrefreshrate();
        } else {
            throw CO2::exceptionLevel("missing screen refresh rate", true);
        }

        if (uiCfg.has_screentimeout()) {
            screenTimeout_ = uiCfg.screentimeout();
        } else {
            throw CO2::exceptionLevel("missing screen timeout", true);
        }

        hasUIConfig_ = true;
        if (hasFanConfig_) {
            threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
        }
        syslog(LOG_DEBUG, "Display config: SDL_FBDEV=\"%s\"  SDL_MOUSEDEV=\"%s\"  "
                          "SDL_MOUSEDRV=\"%s\"  SDL_MOUSE_RELATIVE=\"%s\" "
                          "SDL_VIDEODRIVER=\"%s\"  TTF Dir=\"%s\"  BMP Dir=\"%s\" "
                          "Screen Refresh Rate=%u fps  Screen Timeout=%us",
                          uiCfg.fbdev().c_str(), uiCfg.mousedev().c_str(),
                          uiCfg.mousedrv().c_str(), uiCfg.mouserelative().c_str(),
                          uiCfg.videodriver().c_str(), sdlTTFDir_.c_str(), sdlBMPDir_.c_str(),
                          screenRefreshRate_, screenTimeout_);

    } else {
        syslog(LOG_ERR, "missing Display uiConfig");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }
}

void Co2Display::getFanConfigFromMsg(co2Message::Co2Message& cfgMsg)
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
                relHumThreshold_ = fanCfg.relhumfanonthreshold();
            } else {
                throw CO2::exceptionLevel("missing rel humidity threshold", true);
            }

            if (fanCfg.has_co2fanonthreshold()) {
                co2Threshold_ = fanCfg.co2fanonthreshold();
            } else {
                throw CO2::exceptionLevel("missing CO2 threshold", true);
            }

            hasFanConfig_ = true;
            if (hasUIConfig_) {
                threadState_->stateEvent(CO2::ThreadFSM::ConfigOk);
            }
            syslog(LOG_DEBUG, "Display fan config: FanOnOverrideTIme=%lu minutes  RelHumThreshold=%u%%  CO2Threshold=%uppm",
                              fanOnOverrideTime_, relHumThreshold_, co2Threshold_);
        }

    } else {
        syslog(LOG_ERR, "missing Display fan config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }
}

void Co2Display::getCo2StateFromMsg(co2Message::Co2Message& co2Msg)
{
    DBG_TRACE();

    int rc;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    if (co2Msg.has_co2state()) {
        const co2Message::Co2State& co2State = co2Msg.co2state();

        if (myThreadState == co2Message::ThreadState_ThreadStates_RUNNING) {

            if (co2State.has_temperature()) {
                temperature_.store(co2State.temperature(), std::memory_order_relaxed);
                statusScreen_->setTemperature(temperature_.load(std::memory_order_relaxed));
                DBG_MSG(LOG_DEBUG, "temperature now: %d", temperature_.load(std::memory_order_relaxed));
            }

            if (co2State.has_relhumidity()) {
                relHumidity_.store(co2State.relhumidity(), std::memory_order_relaxed);
                statusScreen_->setRelHumidity(relHumidity_.load(std::memory_order_relaxed));
                DBG_MSG(LOG_DEBUG, "RH now: %d", relHumidity_.load(std::memory_order_relaxed));
            }

            if (co2State.has_co2()) {
                co2_.store(co2State.co2(), std::memory_order_relaxed);
                statusScreen_->setCo2(co2_.load(std::memory_order_relaxed));
                DBG_MSG(LOG_DEBUG, "co2 now: %d", co2_.load(std::memory_order_relaxed));
            }

            if (co2State.has_fanstate()) {
                bool fanOn = false;
                FanAutoManStates fanAutoManState = Auto;
                const char* fanStateStr = "";
                switch (co2State.fanstate()) {
                case co2Message::Co2State_FanStates_AUTO_OFF:
                    fanStateStr = "Auto-Off";
                    break;

                case co2Message::Co2State_FanStates_AUTO_ON:
                    fanOn = true;
                    fanStateStr = "Auto-On";
                    break;

                case co2Message::Co2State_FanStates_MANUAL_OFF:
                    fanAutoManState = ManOff;
                    fanStateStr = "Man-Off";
                    break;

                case co2Message::Co2State_FanStates_MANUAL_ON:
                    fanAutoManState = ManOn;
                    fanOn = true;
                    fanStateStr = "Man-On";
                    break;

                default:
                    syslog(LOG_ERR, "Co2State - unknown fan state:%d", co2State.fanstate());
                    break;
                }

                if (fanOn != fanStateOn_.load(std::memory_order_relaxed)) {
                    statusScreen_->setFanState(fanOn);
                    fanStateOn_.store(fanOn, std::memory_order_relaxed);
                    DBG_MSG(LOG_DEBUG, "fan now: %s", (fanOn) ? "On" : "Off");
                }

                if (fanAutoManState != fanAutoManState_.load(std::memory_order_relaxed)) {
                    fanControlScreen_->setFanAuto(fanAutoManState);
                    statusScreen_->setFanAuto(fanAutoManState == Auto);

                    // stop timer if no longer manual on, but
                    // start timer if new state is manual on
                    //
                    if (fanAutoManState_.load(std::memory_order_relaxed) == ManOn) {
                        statusScreen_->stopFanManOnTimer();
                    } else if (fanAutoManState == ManOn) {
                        // fanOnOverrideTime is in minutes, so convert to seconds
                        statusScreen_->startFanManOnTimer(fanOnOverrideTime_ * 60);
                    }
                    fanAutoManState_.store(fanAutoManState, std::memory_order_relaxed);

                    if (currentScreen_ == Status_Screen) {
                        drawScreen(false);
                    }
                    DBG_MSG(LOG_DEBUG, "fan now: %s", (fanAutoManState_.load(std::memory_order_relaxed) == Auto) ? "Auto" : "Man");
                }
            }

        }

    } else {
        syslog(LOG_ERR, "missing Co2State");
    }
}

void Co2Display::getNetStateFromMsg(co2Message::Co2Message& co2Msg)
{
    DBG_TRACE();

    int rc;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    if (co2Msg.has_netstate()) {
        const co2Message::NetState& netState = co2Msg.netstate();

        if (myThreadState == co2Message::ThreadState_ThreadStates_RUNNING) {

            if (netState.has_netstate()) {
                bool netUp = false;

                switch (netState.netstate()) {
                case co2Message::NetState_NetStates_START:
                case co2Message::NetState_NetStates_DOWN:
                case co2Message::NetState_NetStates_MISSING:
                case co2Message::NetState_NetStates_NO_NET_INTERFACE:
                    netUp = false;
                    break;

                case co2Message::NetState_NetStates_UP:
                    netUp = true;
                    break;

                default:
                    syslog(LOG_ERR, "unknown NetState");
                    break;
                }

                wifiStateOn_.store(netUp, std::memory_order_relaxed);
                statusScreen_->setWiFiState(wifiStateOn_.load(std::memory_order_relaxed));
                DBG_MSG(LOG_DEBUG, "Net state is: %s", (netUp) ? "Up" : "Down");

            } else {
                throw CO2::exceptionLevel("missing netstate", true);
            }
        }

    } else {
        syslog(LOG_ERR, "missing NetState");
    }
}

void Co2Display::listener()
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
                DBG_MSG(LOG_DEBUG, "Display thread rx msg (type=%d)", co2Msg.messagetype());

                switch (co2Msg.messagetype()) {
                case co2Message::Co2Message_Co2MessageType_UI_CFG:
                    getUIConfigFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_FAN_CFG:
                    getFanConfigFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_CO2_STATE:
                    getCo2StateFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_NET_STATE:
                    getNetStateFromMsg(co2Msg);
                    break;

                case co2Message::Co2Message_Co2MessageType_TERMINATE:
                    // send event to wake up ruun loop if necessary
                    SDL_Event uEvent;
                    uEvent.type = SDL_QUIT;
                    SDL_PushEvent(&uEvent);

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
        }
    }
}
void Co2Display::publishUiChanges()
{
    if (!(relHumThresholdChanged_ || co2ThresholdChanged_ || fanAutoManStateChanged_)) {
        return;
    }

    time_t timeNow = time(0);

    // Don't publish more then once every kPublishInterval_ seconds
    //
    if ((timeNow - timeLastUiPublish_) < kPublishInterval_) {
        return;
    }

    DBG_TRACE();

    co2Message::Co2Message co2Msg;

    co2Message::FanConfig* fanCfg = co2Msg.mutable_fanconfig();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_FAN_CFG);

    if (relHumThresholdChanged_) {
        fanCfg->set_relhumfanonthreshold(relHumThreshold_);
    }

    if (co2ThresholdChanged_) {
        fanCfg->set_co2fanonthreshold(co2Threshold_);
    }

    if (fanAutoManStateChanged_) {
        const char* fanAutoManStateStr;
        bool fanAutoManStateOk = true;
        switch (fanAutoManStateChangeReq_.load(std::memory_order_relaxed)) {
        case ManOn:
            fanAutoManStateStr = "On";
            fanCfg->set_fanoverride(co2Message::FanConfig_FanOverride_MANUAL_ON);
            break;
        case ManOff:
            fanAutoManStateStr = "Off";
            fanCfg->set_fanoverride(co2Message::FanConfig_FanOverride_MANUAL_OFF);
            break;
        case Auto:
            fanAutoManStateStr = "Auto";
            fanCfg->set_fanoverride(co2Message::FanConfig_FanOverride_AUTO);
            break;
        default:
            fanAutoManStateOk = false;
            syslog(LOG_ERR, "Unknown fanAutoManState_:%d", static_cast<int>(fanAutoManStateChangeReq_.load(std::memory_order_relaxed)));
            break;
        }
        if (fanAutoManStateOk) {
            DBG_MSG(LOG_DEBUG, "Fan now: %s", fanAutoManStateStr);
        }

    }

    std::string cfgStr;
    co2Msg.SerializeToString(&cfgStr);

    zmq::message_t configMsg(cfgStr.size());

    memcpy(configMsg.data(), cfgStr.c_str(), cfgStr.size());
    mainSocket_.send(configMsg);

    DBG_MSG(LOG_DEBUG, "sent Fan config");

    relHumThresholdChanged_ = false;
    co2ThresholdChanged_ = false;
    fanAutoManStateChanged_ = false;

    timeLastUiPublish_ = timeNow;
}

void Co2Display::sendShutdownMsg(bool reboot)
{
    DBG_TRACE();

    std::string shutdownStr;
    co2Message::Co2Message co2Msg;
    co2Message::RestartMsg* restartMsg = co2Msg.mutable_restartmsg();

    co2Msg.set_messagetype(co2Message::Co2Message_Co2MessageType_RESTART);

    restartMsg->set_restarttype(reboot ? co2Message::RestartMsg_RestartType_REBOOT : co2Message::RestartMsg_RestartType_SHUTDOWN);

    co2Msg.SerializeToString(&shutdownStr);

    zmq::message_t shutdownMsg(shutdownStr.size());

    memcpy (shutdownMsg.data(), shutdownStr.c_str(), shutdownStr.size());
    mainSocket_.send(shutdownMsg);
    syslog(LOG_DEBUG, "Sent %s message", reboot ? "restart" : "shutdown");
}

void Co2Display::run()
{

    DBG_TRACE_MSG("Start of Co2Display::run");

    std::thread* touchScreenThread;
    const char* threadName;

    co2Message::ThreadState_ThreadStates myThreadState = threadState_->state();

    /**************************************************************************/
    /*                                                                        */
    /* Start listener thread and await config                                 */
    /*                                                                        */
    /**************************************************************************/
    std::thread* listenerThread = new std::thread(std::bind(&Co2Display::listener, this));

    // mainSocket is used to send status to main thread
    mainSocket_.connect(CO2::uiEndpoint);

    threadState_->stateEvent(CO2::ThreadFSM::ReadyForConfig);
    if (threadState_->stateChanged()) {
        myThreadState = threadState_->state();
    }

    // We'll continue after receiving our configuration
    while (!threadState_->stateChanged()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    myThreadState = threadState_->state();
    DBG_MSG(LOG_DEBUG, "display state=%s", CO2::stateStr(myThreadState));

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STARTED) {

        init(); // SDL *must* be initialised in the same thread as we call SDL_WaitEvent()
        drawScreen(false);

    } else {
        syslog(LOG_ERR, "Display thread failed to get config");
        threadState_->stateEvent(CO2::ThreadFSM::ConfigError);
    }

    /**************************************************************************/
    /*                                                                        */
    /* Initialise touchscreen and backlight (if config received OK)           */
    /*                                                                        */
    /**************************************************************************/

    SDL_Event event;

    ScreenBacklight::LightLevel backlightLevel;
    uint32_t timerDelay;

    myThreadState = threadState_->state();

    try {
        threadName = "TouchScreen";
        if (touchScreen_) {
            touchScreenThread = new std::thread(std::bind(&Co2TouchScreen::run, touchScreen_));
        } else {
            throw CO2::exceptionLevel("failed to initialise touchScreen_", true);
        }
    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_ERR, "Fatal exception starting thread %s: %s", threadName, el.what());
            shouldTerminate_.store(true, std::memory_order_relaxed);
        }
        syslog(LOG_ERR, "exception starting thread %s: %s", threadName, el.what());
    } catch (...) {
        syslog(LOG_ERR, "Exception starting thread %s", threadName);
        shouldTerminate_.store(true, std::memory_order_relaxed);
    }

    backlightLevel = backlight_->brightness();

    timerDelay = 1000 / screenRefreshRate_;

    // we are now ready to roll
    threadState_->stateEvent(CO2::ThreadFSM::InitOk);
    myThreadState = threadState_->state();

    // leave splash screen up for a bit longer
    std::this_thread::sleep_for(std::chrono::seconds(2));

    currentScreen_ = Status_Screen;
    drawScreen(false);

    timerId_ = SDL_AddTimer(timerDelay, Co2TouchScreen::sendTimerEvent, 0);
    if (!timerId_) {
        syslog(LOG_ERR, "Unable to add timer (%s)", SDL_GetError());
        return;
    }

    /**************************************************************************/
    /*                                                                        */
    /* This is the main run loop.                                             */
    /*                                                                        */
    /**************************************************************************/

    try {
        while (!shouldTerminate_.load(std::memory_order_relaxed))
        {
            uint8_t mouseButton = 0;
            int x = 0;
            int y = 0;
            bool doScreenRefresh = false;
            ScreenEvents screenEvent = None;

            if (SDL_WaitEvent(&event)) {
                // an event was found

                switch (event.type) {

                case SDL_QUIT:
                    syslog(LOG_INFO, "SDL_QUIT event");
                    shouldTerminate_.store(true, std::memory_order_relaxed);

                    break;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_q:
                            shouldTerminate_.store(true, std::memory_order_relaxed);
                            break;
                        default:
                            break;
                    }
                    break;

                case SDL_KEYUP:
                    break;

                case SDL_MOUSEMOTION:
                    mouseButton = SDL_GetMouseState(&x, &y);
                    DBG_MSG(LOG_DEBUG, "MOUSEMOTION  button=%#x x=%d y=%d", mouseButton & 0xff, x, y);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    mouseButton = SDL_GetMouseState(&x, &y);
                    DBG_MSG(LOG_DEBUG, "MOUSEBUTTONDOWN  button=%#x x=%d y=%d", mouseButton & 0xff, x, y);
                    break;

                case SDL_MOUSEBUTTONUP:
                    mouseButton = SDL_GetMouseState(&x, &y);
                    DBG_MSG(LOG_DEBUG, "MOUSEBUTTONUP button=%#x x=%d y=%d", mouseButton & 0xff, x, y);
                    break;

                case Co2TouchScreen::TouchDown:
                    x = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data1));
                    y = static_cast<int>(reinterpret_cast<intptr_t>(event.user.data2));
                    DBG_MSG(LOG_DEBUG, "TouchDown x=%d  y=%d", x, y);
                    break;

                case Co2TouchScreen::TouchUp:
                    DBG_MSG(LOG_DEBUG, "TouchUp %#x", event.user.code);
                    break;

                case Co2TouchScreen::ButtonPush:
                    DBG_MSG(LOG_DEBUG, "ButtonPush %#x", event.user.code);
                    switch (event.user.code) {

                    case Co2TouchScreen::Button1:
                        screenEvent = ButtonPush_1;
                        break;

                    case Co2TouchScreen::Button2:
                        screenEvent = ButtonPush_2;
                        break;

                    case Co2TouchScreen::Button3:
                        screenEvent = ButtonPush_3;
                        break;

                    case Co2TouchScreen::Button4:
                        screenEvent = ButtonPush_4;
                        break;

                    default:
                        break;
                    }
                    break;

                case Co2TouchScreen::Timer:
                    doScreenRefresh = true;
                    break;

                case Co2TouchScreen::Signal:
                    doScreenRefresh = true;
                    switch (event.user.code) {
                    case SIGHUP:
                        syslog(LOG_DEBUG, "SIGHUP");
                        break;
                    case SIGINT:
                        syslog(LOG_DEBUG, "SIGINT");
                        shouldTerminate_.store(true, std::memory_order_relaxed);
                        break;
                    case SIGQUIT:
                        syslog(LOG_DEBUG, "SIGQUIT");
                        shouldTerminate_.store(true, std::memory_order_relaxed);
                        break;
                    case SIGTERM:
                        syslog(LOG_DEBUG, "SIGTERM");
                        shouldTerminate_.store(true, std::memory_order_relaxed);
                        break;
                    default:
                        syslog(LOG_DEBUG, "unknown signal");
                        break;
                    }
                    break;

                default:
                    syslog(LOG_INFO, "UNKNOWN EVENT (%d)", event.type);
                    break;
                }

                if (doScreenRefresh) {
                    backlightLevel = backlight_->setBrightness();
                    if ((backlightLevel == ScreenBacklight::Off) && timerId_) {

                        // cancel screen refresh timer until we get another event
                        if (!SDL_RemoveTimer(timerId_)) {
                            syslog(LOG_ERR, "Unable to cancel timer (%s)", SDL_GetError());
                        }
                        timerId_ = 0;

                        screenFSM(ScreenBacklightOff);
                        DBG_MSG(LOG_DEBUG, "ScreenBacklight On -> Off");

                    } else {
                        // refresh current screen
                        drawScreen(true);
                    }

                } else if (!shouldTerminate_.load(std::memory_order_relaxed)) {
                    // we got an input event

                    // when the screen is dimming or dark we use
                    // the input event to "wake" it up, rather
                    // than initiating a change.
                    //
                    backlightLevel = backlight_->brightness();

                    // use input event to restart screen backlight timer...
                    backlight_->inputEvent();

                    if (backlightLevel != ScreenBacklight::On) {

                        if (backlightLevel == ScreenBacklight::Off) {
                            screenFSM(ScreenBacklightOn);
                            DBG_MSG(LOG_DEBUG, "ScreenBacklight Off -> On");
                        }

                        if (backlightLevel == ScreenBacklight::Dimming) {
                            DBG_MSG(LOG_DEBUG, "ScreenBacklight Dimming -> On");
                        }

                        backlightLevel = backlight_->brightness();

                        // ...and redraw the screen
                        doScreenRefresh = true;
                        screenEvent = None;
                        drawScreen(false);
                    } else {
                        // see if our input event does something...

                        if (x && y) {
                            SDL_Point screenPos = { int16_t(x & 0xffff), int16_t(y & 0xffff) };
                            screenEvent = getScreenEvent(screenPos);
                        }

                        if (screenEvent != None) {
                            screenFSM(screenEvent);
                        } else {
                            drawScreen(true);
                        }
                    }

                    if (!timerId_) {
                        timerId_ = SDL_AddTimer(timerDelay, Co2TouchScreen::sendTimerEvent, 0);
                        if (!timerId_) {
                            syslog(LOG_ERR, "Unable to add timer (%s)", SDL_GetError());
                            shouldTerminate_.store(true, std::memory_order_relaxed);
                            break;
                        }
                    }

                }

                // check and see if there are any unpublished changes
                publishUiChanges();

            } else {
                if ( (threadState_->state() == co2Message::ThreadState_ThreadStates_RUNNING) &&
                     !shouldTerminate_.load(std::memory_order_relaxed) ) {
                    syslog(LOG_ERR, "SDL_WaitEvent returned error: %s", SDL_GetError());
                } else {
                    shouldTerminate_.store(true, std::memory_order_relaxed);
                }
                break;
            }
        } // end main run loop

    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_ERR, "Fatal exception in Co2Display run loop: %s", el.what());
            throw;
        }
        syslog(LOG_ERR, "exception starting in Co2Display run loop: %s", el.what());
    } catch (...) {
        syslog(LOG_ERR, "Exception in Co2Display run loop");
        throw;
    }

    /**************************************************************************/
    /*                                                                        */
    /* end of main run loop.                                                  */
    /*                                                                        */
    /**************************************************************************/
    DBG_TRACE_MSG("end of Co2Display::run loop");
    touchScreen_->stop();
    SDL_Delay(500);
    screenFSM(ScreenBacklightOff);

    Co2Display::disableSDLCleanUp_ = true;
    TTF_Quit();
    SDL_Quit();

    if (threadState_->state() == co2Message::ThreadState_ThreadStates_STOPPING) {
        threadState_->stateEvent(CO2::ThreadFSM::Timeout);
    }
}

void Co2Display::sigHandler(int sig)
{
    SDL_Event uEvent;

    switch (sig) {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        // for now we'll make no difference
        // between various signals.
        //
        uEvent.type = Co2TouchScreen::Signal;
        uEvent.user.code = sig;
        uEvent.user.data1 = 0;
        uEvent.user.data2 = 0;
        SDL_PushEvent(&uEvent);
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


