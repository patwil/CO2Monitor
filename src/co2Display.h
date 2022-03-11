/*
 * co2Display.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef CO2DISPLAY_H
#define CO2DISPLAY_H

//#include <exception>
//#include <time.h>

#ifdef HAS_WIRINGPI
//#include <wiringPi.h>
#endif

//#include "SDL.h"
//#include "SDL_thread.h"
//#include <SDL_ttf.h>
//#include <zmq.hpp>
//#include "co2Message.pb.h"
//#include <google/protobuf/text_format.h>
//#include "displayElement.h"
#include "co2TouchScreen.h"
#include "screenBacklight.h"
#include "utils.h"

#if defined(DEBUG) && defined(HAS_WIRINGPI)

#define GPIO_DBG 1
#define GPIO_DBG_SET(gpio_pin, val) \
    { \
        pinMode(gpio_pin, OUTPUT); \
        digitalWrite(gpio_pin, (val)); \
    }
#define GPIO_DBG_CLEAR(gpio_pin) \
    { \
        pinMode(gpio_pin, OUTPUT); \
        digitalWrite(gpio_pin, 0); \
    }
#define GPIO_DBG_FLIP(gpio_pin) \
    { \
        pinMode(gpio_pin, OUTPUT); \
        digitalWrite(gpio_pin, (digitalRead(gpio_pin)) ? 0 : 1); \
    }
#define GPIO_DBG_PWM(gpio_pin, val) \
    { \
        pinMode(gpio_pin, PWM_OUTPUT); \
        digitalWrite(gpio_pin, (val) & 0x3ff); \
    }

#else

#define GPIO_DBG_SET(gpio_pin, val)
#define GPIO_DBG_CLEAR(gpio_pin)
#define GPIO_DBG_FLIP(gpio_pin)
#define GPIO_DBG_PWM(gpio_pin, val)

#endif /* defined(DEBUG) && defined(HAS_WIRINGPI) */


// need following forward declarations for members of Co2Display
class Co2Screen;
class StatusScreen;
class RelHumCo2ThresholdScreen;
class FanControlScreen;
class ShutdownRebootScreen;
class ConfirmCancelScreen;
class BlankScreen;
class SplashScreen;

class Co2Display
{
    public:
        Co2Display(zmq::context_t& ctx, int sockType);

        ~Co2Display();

        void run();

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
            GPIO_FanControl = 5,
            GPIO_TouchScreen_1 = 24,
            GPIO_TouchScreen_2 = 25,
            GPIO_Backlight = 18,
            GPIO_Button_1 = 23,
            GPIO_Button_2 = 22,
            GPIO_Button_3 = 27,
            GPIO_Button_4 = 17,
            GPIO_Debug_0 = 21,
            GPIO_Debug_1 = 20,
            GPIO_Debug_2 = 16,
            GPIO_Debug_3 = 12,
            GPIO_Debug_4 = 26,
            GPIO_Debug_5 = 19,
            GPIO_Debug_6 = 13,
            GPIO_Debug_7 = 6,
        } GPIO_Pins;

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
        Co2Display();

        void init();
        void uninit();
        void setScreenSize(std::string fbFilename);

        void screenFSM(ScreenEvents event);

        void drawScreen(bool refreshOnly = true);
        ScreenEvents getScreenEvent(SDL_Point pos);

        void getUIConfigFromMsg(co2Message::Co2Message& cfgMsg);
        void getFanConfigFromMsg(co2Message::Co2Message& cfgMsg);
        void getCo2StateFromMsg(co2Message::Co2Message& co2Msg);
        void getNetStateFromMsg(co2Message::Co2Message& co2Msg);
        void listener();

        void publishUiChanges();

        //void sendNetState();

        void sendShutdownMsg(bool reboot);

        zmq::context_t& ctx_;
        zmq::socket_t mainSocket_;
        zmq::socket_t subSocket_;

        CO2::ThreadFSM* threadState_;

        std::string sdlTTFDir_;
        std::string sdlBMPDir_;

        std::string fontName_;

        SDL_Point screenSize_;
        int bitDepth_;

        SDL_Window* window_;
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

        bool hasUIConfig_;
        bool hasFanConfig_;

        int screenRefreshRate_;
        int screenTimeout_;
        std::string mouseDev_;

        SDL_TimerID timerId_;

        std::atomic<int> temperature_;
        std::atomic<bool> temperatureChanged_;
        std::atomic<int> relHumidity_;
        std::atomic<bool> relHumChanged_;
        std::atomic<int> co2_;
        std::atomic<bool> co2Changed_;
        int relHumThreshold_;
        bool relHumThresholdChanged_;
        const int relHumThresholdChangeDelta_ = 1;
        int co2Threshold_;
        bool co2ThresholdChanged_;
        const int co2ThresholdChangeDelta_ = 10;
        std::atomic<bool> fanStateOn_;
        std::atomic<bool> fanStateChanged_;
        std::atomic<FanAutoManStates> fanAutoManStateChangeReq_;
        std::atomic<FanAutoManStates> fanAutoManState_;
        bool fanAutoManStateChanged_;
        time_t fanOnOverrideTime_;
        std::atomic<bool> wifiStateOn_;
        std::atomic<bool> wifiStateChanged_;

        time_t timeLastUiPublish_;
        time_t kPublishInterval_;

    protected:
};

#endif /* CO2DISPLAY_H */
