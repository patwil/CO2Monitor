/*
 * co2TouchScreen.cpp
 *
 * Created on: 2016-11-13
 *     Author: patw
 */

//#include <iostream>
//#include <sys/select.h>
//#include <sys/time.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <fcntl.h>
//#include <stropts.h>
//#include <linux/version.h>
#include <linux/input.h>
//#include <unistd.h>
//#include <error.h>
//#include <errno.h>
#include <syslog.h>
#include "co2Display.h"
//#include "co2TouchScreen.h"
#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

Co2TouchScreen::Co2TouchScreen() :
    mouseDeviceFd_(-1)
{
    shouldTerminate_.store(false, std::memory_order_relaxed);
}

Co2TouchScreen::~Co2TouchScreen()
{
    // Delete all dynamic memory.
}

const uint32_t Co2TouchScreen::kDebounceMilliSecs_ = 200;

void Co2TouchScreen::init(std::string mouseDevice)
{
    if ((mouseDeviceFd_ = open(mouseDevice.c_str(), O_RDONLY)) < 0) {
        if (errno == EACCES && getuid() != 0) {
            syslog(LOG_ERR, "Cannot access %s. Need root permission",
                    mouseDevice.c_str());
        } else {
            syslog(LOG_ERR, "Error opening input device %s.",
                    mouseDevice.c_str());
        }
        return;
    }

    int rc = ioctl(mouseDeviceFd_, EVIOCGRAB, (void*)1);
    if (rc) {
        syslog(LOG_ERR, "Unable to grab exclusive access to %s", mouseDevice.c_str());
        close(mouseDeviceFd_);
        mouseDeviceFd_ = -1;
        return;
    }

    syslog(LOG_DEBUG, "%s open fd=%d", mouseDevice.c_str(), mouseDeviceFd_);
}

void Co2TouchScreen::buttonInit()
{
    std::array<ButtonInfo, ButtonMax> buttons = {{
        {Co2Display::GPIO_Button_1, button1Action},
        {Co2Display::GPIO_Button_2, button2Action},
        {Co2Display::GPIO_Button_3, button3Action},
        {Co2Display::GPIO_Button_4, button4Action} }};

    for (const auto& button: buttons)
    {
#ifdef HAS_WIRINGPI
        int mode = INT_EDGE_FALLING;

        pinMode(button.gpioPin, INPUT);
        pullUpDnControl(button.gpioPin, PUD_UP);
        if (wiringPiISR(button.gpioPin, mode, button.action) < 0) {
            syslog(LOG_ERR, "Unable to setup ISR for GPIO pin %d: %s", button.gpioPin, strerror(errno));
            throw CO2::exceptionLevel("wiringPiISR() returned error", true);
        }
#endif
    }
}

void Co2TouchScreen::button1Action()
{
    buttonAction(Button1);
}

void Co2TouchScreen::button2Action()
{
    buttonAction(Button2);
}

void Co2TouchScreen::button3Action()
{
    buttonAction(Button3);
}

void Co2TouchScreen::button4Action()
{
    buttonAction(Button4);
}

void Co2TouchScreen::buttonAction(int button)
{
    static uint32_t prevEventTime = 0;

    uint32_t now = SDL_GetTicks();

    if (now >= (prevEventTime + kDebounceMilliSecs_)) {
        SDL_Event buttonPush;
        prevEventTime = now;
        buttonPush.type = ButtonPush;
        buttonPush.user.code = button;
        SDL_PushEvent(&buttonPush);
    }
}

uint32_t Co2TouchScreen::sendTimerEvent(uint32_t interval, void* arg)
{
    SDL_Event timerEvent;

    timerEvent.type = Timer;
    SDL_PushEvent(&timerEvent);
    return interval;
}

void Co2TouchScreen::run()
{

    uint32_t prevEventTime = 0;

    while (!shouldTerminate_.load(std::memory_order_relaxed)) {
        struct input_event ev[64];
        int i, rd;
        fd_set rfds;
        struct timeval readTimeout;

        FD_ZERO(&rfds);
        FD_SET(mouseDeviceFd_, &rfds);
        readTimeout.tv_sec = 0;
        readTimeout.tv_usec = 300000; // 300 ms

        int ready = select(mouseDeviceFd_ + 1, &rfds, NULL, NULL, &readTimeout);

        if (shouldTerminate_.load(std::memory_order_relaxed)) {
            break;
        }

        if (ready == 0) {
            // timer expired - nothing to read this time
            continue;
        } else if (ready < 0) {
            char errStr[100];
            syslog(LOG_ERR, "select returned error %d (%s)", errno, strerror_r(errno, errStr, sizeof(errStr)));
            continue;
        }

        if (!FD_ISSET(mouseDeviceFd_, &rfds)) {
            syslog(LOG_ERR, "unknown input: should be touchscreen");
            continue;
        }

        rd = read(mouseDeviceFd_, ev, sizeof(ev));

        if (rd < (int) sizeof(struct input_event)) {
            syslog(LOG_ERR, "%s: expected %d bytes, got %d\n", __FUNCTION__,  (int) sizeof(struct input_event), rd);
            break;
        }
        uint32_t now = SDL_GetTicks();

        int nEventLines = rd / sizeof(struct input_event);
        int32_t x = -1;
        int32_t y = -1;
        uint16_t eventType = ~0;
        uint16_t eventCode = ~0;

        for (i = 0; i < nEventLines; i++) {
            eventType = ev[i].type;
            eventCode = ev[i].code;

            switch (eventType) {
                case EV_ABS:
                    switch (eventCode) {
                    case ABS_X:
                    case ABS_MT_POSITION_X:
                        if (x < 0) {
                            x = ev[i].value;
                        }
                        break;

                    case ABS_Y:
                    case ABS_MT_POSITION_Y:
                        if (y < 0) {
                            y = ev[i].value;
                        }
                        break;

                    default:
                        break;
                    }
                    break;

            default:
                break;
            }
            if ( (x >= 0) && (y >= 0) ) {
                // we've got what we need
                // scale up for SDL2 screensize
                x *= 2;
                y *= 2;
                break;
            }
        }

        if ((x >= 0) && (y >= 0) && (now >= (prevEventTime + kDebounceMilliSecs_))) {
            SDL_Event uEvent;
            uEvent.type = TouchDown;
            uEvent.user.code = 0;
            uEvent.user.data1 = reinterpret_cast<void*>(x);
            uEvent.user.data2 = reinterpret_cast<void*>(y);
            SDL_PushEvent(&uEvent);

            prevEventTime = now;
        }
    } // while ()

    ioctl(mouseDeviceFd_, EVIOCGRAB, (void*)0);
    close(mouseDeviceFd_);

    syslog(LOG_DEBUG, "TouchScreen run loop end");
}

void Co2TouchScreen::stop()
{
    shouldTerminate_.store(true, std::memory_order_relaxed);
}

