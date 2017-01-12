/*
 * screenBacklight.h
 *
 * Created on: 2016-11-17
 *     Author: patw
 */

#ifndef SCREENBACKLIGHT_H
#define SCREENBACKLIGHT_H

#include "SDL.h"
#include "SDL_thread.h"
#include <SDL_ttf.h>
#include <time.h>

#ifdef HAS_WIRINGPI
#include <wiringPi.h>
#endif

class ScreenBacklight
{
    public:
        ScreenBacklight();

        void init(int idleTimeout);
        void inputEvent();

        typedef enum {
            On,
            Dimming,
            Off
        } LightLevel;

        LightLevel setBrightness();
        void setBrightness(LightLevel brightness);
        LightLevel brightness() const;

        ~ScreenBacklight();

    private:
        const uint32_t kDimTimeMs_; // number of milliseconds to dim screen
        const uint32_t kBacklightFull_;
        uint32_t idleTimeoutMs_;
        uint32_t timeLastInputEvent_;
        const uint32_t kBacklightGpioPin_;
        uint32_t backlightGpioPinSetting_;

        void buttonInit();

    protected:
};

#endif /* SCREENBACKLIGHT_H */
