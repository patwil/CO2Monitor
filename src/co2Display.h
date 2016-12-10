/*
 * co2Display.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef CO2DISPLAY_H
#define CO2DISPLAY_H

#include <exception>
#include <time.h>
#include <wiringPi.h>
#include "SDL.h"
#include "SDL_thread.h"
#include <SDL_ttf.h>
//#include <zmq.hpp>
#include "displayElement.h"
#include "co2TouchScreen.h"
#include "screenBacklight.h"

namespace CO2 {

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

}

// need following forward declarations for members of Co2Display
class Co2Screen;
class StatusScreen;
class RelHumCo2ThresholdScreen;
class FanControlScreen;
class ShutdownRebootScreen;
class ConfirmCancelScreen;
class BlankScreen;
class SplashScreen;

#if 0
#include <zmq.hpp>

class Co2Display
{
    public:
        Co2Display(zmq::context_t& ctx, int sockType);

        virtual ~Co2Display();

        void run();

    private:
        Co2Display();

    protected:
};
#endif

class Co2Display
{
    public:
        Co2Display();

        ~Co2Display();

        void run();

        static void cleanUp();
        static void sigHandler(int sig);

        static std::atomic<bool> shouldTerminate_;

        typedef enum {
            Status_Screen,
            RelHumCo2Threshold_Screen,
            FanControl_Screen,
            ShutdownReboot_Screen,
            ConfirmCancelReboot_Screen,
            ConfirmCancelShutdown_Screen,
            Blank_Screen,
            Splash_Screen,
            NumberOfScreens
        } ScreenNames;

        typedef enum {
            None,
            ButtonPush_1,
            ButtonPush_2,
            ButtonPush_3,
            ButtonPush_4,
            RelHumUp,
            RelHumDown,
            Co2Up,
            Co2Down,
            FanOn,
            FanAuto,
            FanOff,
            Reboot,
            Shutdown,
            Confirm,
            Cancel,
            ScreenBacklightOff,
            ScreenBacklightOn,
            MaxScreenEvents
        } ScreenEvents;

        typedef enum {
            ManOn,
            ManOff,
            Auto
        } FanAutoManStates;

        typedef struct {
            TTF_Font* font;
            int size;
        } FontInfo;

        typedef enum {
            Small,
            Medium,
            Large,
            NumberOfFontSizes
        } FontSizes;

        std::array<FontInfo, NumberOfFontSizes> fonts_;

    private:
        void init();
        void setScreenSize(std::string fbFilename);

        void screenFSM(ScreenEvents event);

        void drawScreen(bool refreshOnly = true);
        ScreenEvents getScreenEvent(SDL_Point pos);

        void publishUiChanges();

        std::string sdlTTFDir_;
        std::string sdlBMPDir_;

        std::string fontName_;

        SDL_Point screenSize_;
        int bitDepth_;

        SDL_Surface* screen_;
        StatusScreen* statusScreen_;
        RelHumCo2ThresholdScreen* relHumCo2ThresholdScreen_;
        FanControlScreen* fanControlScreen_;
        ShutdownRebootScreen* shutdownRestartScreen_;
        ConfirmCancelScreen* confirmCancelScreen_;
        BlankScreen* blankScreen_;
        SplashScreen* splashScreen_;

        std::array<Co2Screen*, NumberOfScreens> screens_;
        ScreenNames currentScreen_;

        Co2TouchScreen* touchScreen_;
        ScreenBacklight* backlight_;

        int screenRefreshRate_;
        int screenTimeout_;

        SDL_TimerID timerId_;

        std::atomic<int> temperature_;
        std::atomic<bool> temperatureChanged_;
        std::atomic<int> relHumidity_;
        std::atomic<bool> relHumChanged_;
        std::atomic<int> co2_;
        std::atomic<bool> co2Changed_;
        int relHumThreshold_;
        bool relHumThresholdChanged_;
        int co2Threshold_;
        bool co2ThresholdChanged_;
        std::atomic<bool> fanStateOn_;
        std::atomic<bool> fanStateChanged_;
        FanAutoManStates fanAutoManState_;
        bool fanAutoManStateChanged_;
        time_t fanOnOverrideTime_;
        std::atomic<bool> wifiStateOn_;
        std::atomic<bool> wifiStateChanged_;

        time_t timeLastUiPublish_;

    protected:
};

#endif /* CO2DISPLAY_H */
