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

#include <stropts.h>
#include <string.h>
#include <time.h>
//#include "utils.h"
//#include "co2Message.pb.h"
//#include <google/protobuf/text_format.h>
#include "co2TouchScreen.h"
#include "co2Screen.h"
#include "screenBacklight.h"
#include "co2Display.h"

#if 0
#include <stdlib.h>
#include <syslog.h>
#include "co2Display.h"
#include "utils.h"
#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

Co2Display::Co2Display(zmq::context_t& ctx, int sockType)
#endif

Co2Display::Co2Display() :
    statusScreen_(nullptr),
    relHumCo2ThresholdScreen_(nullptr),
    fanControlScreen_(nullptr),
    shutdownRestartScreen_(nullptr),
    confirmCancelScreen_(nullptr),
    blankScreen_(nullptr),
    splashScreen_(nullptr),
    currentScreen_(Splash_Screen),
    screenRefreshRate_(0),
    screenTimeout_(0),
    timerId_(0),
    relHumThreshold_(0),
    relHumThresholdChanged_(false),
    co2Threshold_(0),
    co2ThresholdChanged_(false),
    fanAutoManState_(Auto),
    fanAutoManStateChanged_(false),
    timeLastUiPublish_(0)

{
    temperature_.store(0, std::memory_order_relaxed);
    temperatureChanged_.store(false, std::memory_order_relaxed);
    relHumidity_.store(0, std::memory_order_relaxed);
    relHumChanged_.store(false, std::memory_order_relaxed);
    co2_.store(0, std::memory_order_relaxed);
    co2Changed_.store(false, std::memory_order_relaxed);
    fanStateOn_.store(false, std::memory_order_relaxed);
    fanStateChanged_.store(false, std::memory_order_relaxed);
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
    //
    action.sa_handler = Co2Display::sigHandler;
    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaction(SIGHUP, &action, 0);
    sigaction(SIGINT, &action, 0);
    sigaction(SIGQUIT, &action, 0);
    sigaction(SIGTERM, &action, 0);

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
}

std::atomic<bool> Co2Display::shouldTerminate_;

void Co2Display::init()
{
    int rc;

    sdlTTFDir_ = std::string("/usr/local/bin/sdlbmp.d/ttf");
    sdlBMPDir_ = std::string("/usr/local/bin/sdlbmp.d/bmp");

    rc = setenv("SDL_FBDEV", "/dev/fb1", 0);
    setScreenSize(std::string("/dev/fb1"));

    if ( !bitDepth_ || !screenSize_.x || !screenSize_.y){
        syslog(LOG_ERR, "Error setting screen size/depth from %s: bits-per-pixel=%d  width=%d  height=%d",
               "/dev/fb1", bitDepth_, screenSize_.x, screenSize_.y);
        throw CO2::exceptionLevel("unable to read screen size and/or depth", true);
    }

    if (rc) {
        syslog(LOG_ERR, "setenv(\"SDL_FBDEV\") returned error (%d)\n", rc);
        throw CO2::exceptionLevel("setenv SDL_FBDEV error", true);
    }

    std::string mouseDevice("/dev/input/touchscreen");
    rc = setenv("SDL_MOUSEDEV", mouseDevice.c_str(), 0);

    if (rc) {
        syslog(LOG_ERR, "setenv(\"SDL_MOUSEDEV\") returned error (%d)\n", rc);
        throw CO2::exceptionLevel("setenv SDL_MOUSEDEV error", true);
    }

    rc = setenv("SDL_MOUSEDRV", "TSLIB", 0);

    if (rc) {
        syslog(LOG_ERR, "setenv(\"SDL_MOUSEDRV\") returned error (%d)\n", rc);
    }

    rc = setenv("SDL_VIDEODRIVER", "fbcon", 0);

    if (rc) {
        syslog(LOG_ERR, "setenv(\"SDL_VIDEODRIVER\") returned error (%d)\n", rc);
        throw CO2::exceptionLevel("setenv SDL_VIDEODRIVER error", true);
    }

    rc = setenv("SDL_MOUSE_RELATIVE", "0", 0);

    if (rc) {
        syslog(LOG_ERR, "setenv(\"SDL_MOUSE_RELATIVE\") returned error (%d)\n", rc);
    }

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);

    // Initialize SDL_ttf library
    if (TTF_Init())
    {
        SDL_Quit();
        throw CO2::exceptionLevel("failed to init ttf");
    }

    if (atexit(Co2Display::cleanUp)) {
        throw CO2::exceptionLevel("unable to set exit function", true);
    }

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

    screenRefreshRate_ = 10; // fps
    screenTimeout_ = 60; // seconds

    fanOnOverrideTime_ = 120; // minutes

    SDL_ShowCursor(SDL_DISABLE);

    wiringPiSetupGpio(); // must be called once (and only once) before any GPIO related calls

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

    touchScreen_->init(mouseDevice);
    touchScreen_->buttonInit();
    backlight_->init(screenTimeout_);

    temperature_.store(12, std::memory_order_relaxed);
    relHumidity_.store(34, std::memory_order_relaxed);
    co2_.store(567, std::memory_order_relaxed);
    relHumThreshold_ = 45;
    co2Threshold_ = 670;
    fanStateOn_.store(true, std::memory_order_relaxed);
    fanAutoManState_ = Auto;
    wifiStateOn_.store(true, std::memory_order_relaxed);

    statusScreen_->setTemperature(temperature_.load(std::memory_order_relaxed));
    statusScreen_->setRelHumidity(relHumidity_.load(std::memory_order_relaxed));
    statusScreen_->setCo2(co2_.load(std::memory_order_relaxed));
    statusScreen_->setFanState(fanStateOn_.load(std::memory_order_relaxed));
    statusScreen_->setFanAuto(fanAutoManState_);
    statusScreen_->setWiFiState(wifiStateOn_.load(std::memory_order_relaxed));

    relHumCo2ThresholdScreen_->setRelHumThreshold(relHumThreshold_);
    relHumCo2ThresholdScreen_->setCo2Threshold(co2Threshold_);

    fanControlScreen_->setFanAuto(fanAutoManState_);
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
            relHumCo2ThresholdScreen_->setRelHumThreshold(++relHumThreshold_);
            relHumThresholdChanged_ = true;
            break;
        case RelHumDown:
            relHumCo2ThresholdScreen_->setRelHumThreshold(--relHumThreshold_);
            relHumThresholdChanged_ = true;
            break;
        case Co2Up:
            relHumCo2ThresholdScreen_->setCo2Threshold(++co2Threshold_);
            co2ThresholdChanged_ = true;
            break;
        case Co2Down:
            relHumCo2ThresholdScreen_->setCo2Threshold(--co2Threshold_);
            co2ThresholdChanged_ = true;
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
            fanAutoManState_ = ManOn;
            fanControlScreen_->setFanAuto(fanAutoManState_);
            fanAutoManStateChanged_ = true;
            // REMOVE WHEN IN CO2MONITOR
            fanStateOn_.store(true, std::memory_order_relaxed);
            statusScreen_->setFanState(fanStateOn_.load(std::memory_order_relaxed));
            statusScreen_->setFanAuto(fanAutoManState_ == Auto);

            // fanOnOverrideTime is in minutes, so convert to seconds
            statusScreen_->startFanManOnTimer(fanOnOverrideTime_ * 60);
            // END
            break;
        case FanOff:
            fanAutoManState_ = ManOff;
            fanControlScreen_->setFanAuto(fanAutoManState_);
            fanAutoManStateChanged_ = true;
            // REMOVE WHEN IN CO2MONITOR
            fanStateOn_.store(false, std::memory_order_relaxed);
            statusScreen_->setFanState(fanStateOn_.load(std::memory_order_relaxed));
            statusScreen_->setFanAuto(fanAutoManState_ == Auto);
            statusScreen_->stopFanManOnTimer();
            // END
            break;
        case FanAuto:
            fanAutoManState_ = Auto;
            fanControlScreen_->setFanAuto(fanAutoManState_);
            fanAutoManStateChanged_ = true;
            // REMOVE WHEN IN CO2MONITOR
            fanStateOn_.store(true, std::memory_order_relaxed);
            statusScreen_->setFanState(fanStateOn_.load(std::memory_order_relaxed));
            statusScreen_->setFanAuto(fanAutoManState_ == Auto);
            statusScreen_->stopFanManOnTimer();
            // END
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
            shouldTerminate_.store(true, std::memory_order_relaxed);
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
            shouldTerminate_.store(true, std::memory_order_relaxed);
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
        syslog(LOG_DEBUG, "new screen = %d", static_cast<int>(newScreen));
        screens_[currentScreen_]->setNeedsRedraw();
        drawScreen(false);
    }
}

void Co2Display::cleanUp()
{
    TTF_Quit();
    SDL_Quit();
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

void Co2Display::publishUiChanges()
{
    time_t timeNow = time(0);

    // Don't publish more then once every 10 seconds
    //
    if ( (timeNow - timeLastUiPublish_) < 10) {
        return;
    }

    if (!(relHumThresholdChanged_ || co2ThresholdChanged_ || fanAutoManStateChanged_)) {
        return;
    }

    if (relHumThresholdChanged_) {
        syslog(LOG_DEBUG, "RH threshold now: %d", relHumThreshold_);
    }

    if (co2ThresholdChanged_) {
        syslog(LOG_DEBUG, "CO2 threshold now: %d", co2Threshold_);
    }

    if (fanAutoManStateChanged_) {
        const char* fanAutoManStateStr;
        switch (fanAutoManState_) {
        case ManOn:
            fanAutoManStateStr = "On";
            break;
        case ManOff:
            fanAutoManStateStr = "Off";
            break;
        case Auto:
            fanAutoManStateStr = "Auto";
            break;
        default:
            fanAutoManStateStr = "";
            break;
        }
        syslog(LOG_DEBUG, "RH threshold now: %s", fanAutoManStateStr);
    }

    // publish
    // changes
    // here

    relHumThresholdChanged_ = false;
    co2ThresholdChanged_ = false;
    fanAutoManStateChanged_ = false;

    timeLastUiPublish_ = timeNow;
}

void Co2Display::run()
{

    init(); // SDL *must* be initialised in the same thread as we call SDL_WaitEvent()

    std::thread* touchScreenThread;
    const char* threadName;

    try {
        threadName = "TouchScreen";
        if (touchScreen_) {
            touchScreenThread = new std::thread(std::bind(&Co2TouchScreen::run, touchScreen_));
        } else {
            throw CO2::exceptionLevel("failed to initialise touchScreen_", true);
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

    int i = 0;
    int j = 0;
    int ii = 0;

    SDL_Event event;

    ScreenBacklight::LightLevel backlightLevel = backlight_->brightness();

    drawScreen(false);

    uint32_t timerDelay = 1000 / screenRefreshRate_;
    syslog(LOG_DEBUG, "Screen refresh rate = %dfps,  timer delay = %dms", screenRefreshRate_, timerDelay);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    currentScreen_ = Status_Screen;
    drawScreen(false);

    timerId_ = SDL_AddTimer(timerDelay, Co2TouchScreen::sendTimerEvent, 0);
    if (!timerId_) {
        syslog(LOG_ERR, "Unable to add timer (%s)", SDL_GetError());
        return;
    }

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
                    syslog(LOG_DEBUG, "SDL_QUIT event");
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
                    syslog(LOG_DEBUG, "MOUSEMOTION  button=%#x x=%d y=%d", mouseButton & 0xff, x, y);
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    mouseButton = SDL_GetMouseState(&x, &y);
                    syslog(LOG_DEBUG, "MOUSEBUTTONDOWN  button=%#x x=%d y=%d", mouseButton & 0xff, x, y);
                    break;

                case SDL_MOUSEBUTTONUP:
                    mouseButton = SDL_GetMouseState(&x, &y);
                    syslog(LOG_DEBUG, "MOUSEBUTTONUP button=%#x x=%d y=%d", mouseButton & 0xff, x, y);
                    break;

                case Co2TouchScreen::TouchDown:
                    x = int(event.user.data1);
                    y = int(event.user.data2);
                    syslog(LOG_DEBUG, "TouchDown x=%d  y=%d", x, y);
                    break;

                case Co2TouchScreen::TouchUp:
                    syslog(LOG_DEBUG, "TouchUp %#x", event.user.code);
                    break;

                case Co2TouchScreen::ButtonPush:
                    syslog(LOG_DEBUG, "ButtonPush %#x", event.user.code);
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
                        syslog(LOG_DEBUG, "ScreenBacklight On -> Off");

                        //SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
                        //SDL_UpdateRect(screen, 0, 0, 0, 0);

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
                    if (backlightLevel != ScreenBacklight::On) {
                        // use input event to wake up the screen backlight...
                        backlight_->inputEvent();

                        if (backlightLevel == ScreenBacklight::Off) {
                            screenFSM(ScreenBacklightOn);
                            syslog(LOG_DEBUG, "ScreenBacklight Off -> On");
                        }

                        if (backlightLevel == ScreenBacklight::Dimming) {
                            syslog(LOG_DEBUG, "ScreenBacklight Dimming -> On");
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
                syslog(LOG_ERR, "SDL_WaitEvent returned error: %s", SDL_GetError());
                shouldTerminate_.store(true, std::memory_order_relaxed);
                break;
            }

            //screens_[currentScreen_]->draw(doScreenRefresh);

        } // end main run loop

    } catch (CO2::exceptionLevel& el) {
        if (el.isFatal()) {
            syslog(LOG_CRIT, "Fatal exception in Co2Display run loop: %s", el.what());
            throw;
        }
        syslog(LOG_ERR, "exception starting in Co2Display run loop: %s", el.what());
    } catch (...) {
        syslog(LOG_CRIT, "Exception in Co2Display run loop");
        throw;
    }

    touchScreen_->stop();
    SDL_Delay(500);
    screenFSM(ScreenBacklightOff);

    syslog(LOG_DEBUG, "Co2Display run loop end");
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


