/*
 * screenBacklight.cpp
 *
 * Created on: 2016-11-17
 *     Author: patw
 */

//#include <syslog.h>
//#include <exception>
#include "co2Display.h"

//#include "screenBacklight.h"

ScreenBacklight::ScreenBacklight() :
    kDimTimeMs_(10*1000),
    kBacklightFull_(1023),
    idleTimeoutMs_(0),
    timeLastInputEvent_(0),
    kBacklightGpioPin_(Co2Display::GPIO_Backlight),
    backlightGpioPinSetting_(kBacklightFull_)
{
}

ScreenBacklight::~ScreenBacklight()
{
    setBrightness(Off);
}

void ScreenBacklight::init(int idleTimeout)
{
    idleTimeoutMs_ = idleTimeout * 1000;

#ifdef HAS_WIRINGPI
    // setup GPIO pin to control backlight
    pinMode(kBacklightGpioPin_, PWM_OUTPUT);
    pwmWrite(kBacklightGpioPin_, backlightGpioPinSetting_);
#endif

    timeLastInputEvent_ = SDL_GetTicks();
}

void ScreenBacklight::inputEvent()
{
    timeLastInputEvent_ = SDL_GetTicks();
}

ScreenBacklight::LightLevel ScreenBacklight::setBrightness()
{
    uint32_t timeSinceLastInputEvent_ = SDL_GetTicks() - timeLastInputEvent_;

    if (timeSinceLastInputEvent_ <= idleTimeoutMs_) {

        // we haven't been idle long enough to start dimming
        backlightGpioPinSetting_ = kBacklightFull_;

    } else {

        // how long since we exceeded the timeout?
        uint32_t dimTime = timeSinceLastInputEvent_ - idleTimeoutMs_;

        if (dimTime >= kDimTimeMs_) {

            // past timeout + time to dim backlight, so turn it off
            backlightGpioPinSetting_ = 0;

        } else {

            // we're somewhere between fully on and off
            backlightGpioPinSetting_ = kBacklightFull_ * (kDimTimeMs_ - dimTime) / kDimTimeMs_;

        }
    }

#ifdef HAS_WIRINGPI
    pwmWrite(kBacklightGpioPin_, backlightGpioPinSetting_);
#endif

    return brightness();
}

void ScreenBacklight::setBrightness(ScreenBacklight::LightLevel brightness)
{
    switch (brightness) {
    case On:
        backlightGpioPinSetting_ = kBacklightFull_;
        break;
    case Off:
        backlightGpioPinSetting_ = 0;
        break;
    default:
        break;
    }

#ifdef HAS_WIRINGPI
    pwmWrite(kBacklightGpioPin_, backlightGpioPinSetting_);
#endif
}

ScreenBacklight::LightLevel ScreenBacklight::brightness() const
{
    if (backlightGpioPinSetting_ == 0) return ScreenBacklight::Off;

    if (backlightGpioPinSetting_ < kBacklightFull_) return ScreenBacklight::Dimming;

    return ScreenBacklight::On;
}



