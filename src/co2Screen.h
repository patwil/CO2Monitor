/*
 * co2Screen.h
 *
 * Created on: 2016-11-13
 *     Author: patw
 */

#ifndef CO2SCREEN_H
#define CO2SCREEN_H

#include <iostream>
#include <map>
#include <vector>
#include "SDL.h"
#include "SDL_thread.h"
#include <SDL_ttf.h>

#include "co2Display.h"
#include "displayElement.h"

class Co2Screen
{
    public:

        Co2Screen();
        virtual ~Co2Screen();

        void clear();

        virtual void setNeedsRedraw() { needsRedraw_ = true; }
        virtual bool needsRedraw() { return needsRedraw_; }

        virtual Co2Display::ScreenEvents getScreenEvent(SDL_Point pos);

        static uint32_t getpixel(SDL_Surface *surface, SDL_Point point);
        static void     putpixel(SDL_Surface *surface, SDL_Point point, uint32_t pixel);

    private:

        bool needsRedraw_;

        SDL_Window* window_;
        SDL_Surface* screen_;

    protected:

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        void addElement(int element,
                        SDL_Rect* position,
                        SDL_Color backgroundColour,
                        std::string bitmap,
                        SDL_Point colourKey = {1, 1});

        void addElement(int element,
                        SDL_Rect* position,
                        SDL_Color foregroundColour,
                        SDL_Color backgroundColour,
                        std::string& text,
                        Co2Display::FontSizes fontSize);

        void setElementText(int element, std::string& text);

        virtual void draw(bool refreshOnly = true);
        virtual void draw(int element, bool refreshOnly = true);
        virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

        std::map<int, DisplayElement*> displayElements_;

        std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts_;

        std::string sdlBitMapDir_;

        bool initComplete_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  StatusScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class StatusScreen: public Co2Screen
{
    public:

        typedef enum {
            FanOnImages = 5
        } MultiImageNumbers;

        typedef enum {
            FirstElement,
            TemperatureText = FirstElement,
            TemperatureValue,
            TemperatureUnitText_1,
            TemperatureUnitText_2,
            RelHumText,
            RelHumValue,
            RelHumUnitText,
            Co2Text_1,
            Co2Text_2,
            Co2Value,
            Co2UnitText,
            FanOverrideAutoText,
            FanOverrideManText,
            FanManOnCountdown,
            FanOnFirst,
            FanOnLast = FanOnFirst + FanOnImages - 1,
            FanOff,
            WiFiStateOn,
            WiFiStateOff,
            LastElement
        } Elements;

        StatusScreen();
        virtual ~StatusScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

        void setTemperature(int temperature);
        void setRelHumidity(int relHumidity);
        void setCo2(int co2);
        void setFanState(bool isOn);
        void setFanAuto(bool isAuto);
        void startFanManOnTimer(time_t duration);
        void stopFanManOnTimer();
        void setWiFiState(bool isOn);

    private:

        void updateFanManOnCountdown();

        bool temperatureChanged_;
        bool relHumChanged_;
        bool co2Changed_;
        bool fanStateOn_;
        bool fanStateChanged_;
        bool fanAuto_;
        bool fanAutoChanged_;
        time_t fanManOnEndTime_;
        bool wifiStateOn_;
        bool wifiStateChanged_;

        int fanOnImageIndex_;
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  RelHumCo2ThresholdScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class RelHumCo2ThresholdScreen: public Co2Screen
{
    public:

        typedef enum {
            FirstElement,
            TitleText = FirstElement,
            RelHumText,
            RelHumValue,
            RelHumUnitText,
            Co2Text_1,
            Co2Text_2,
            Co2Value,
            Co2UnitText,
            RelHumControlUp,
            RelHumControlDown,
            Co2ControlUp,
            Co2ControlDown,
            LastElement
        } Elements;

        RelHumCo2ThresholdScreen();
        virtual ~RelHumCo2ThresholdScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

        virtual Co2Display::ScreenEvents getScreenEvent(SDL_Point pos);

        void setRelHumThreshold(int relHumThreshold);
        void setCo2Threshold(int co2Threshold);

    private:

        bool relHumThresholdChanged_;
        bool co2ThresholdChanged_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  FanControlScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class FanControlScreen: public Co2Screen
{
    public:

        typedef enum {
            FirstElement,
            TitleText = FirstElement,
            FanOverrideManOnText,
            FanOverrideManOffText,
            FanOverrideAutoText,
            FanOnActive,
            FanOnInactive,
            FanAutoActive,
            FanAutoInactive,
            FanOffActive,
            FanOffInactive,
            LastElement
        } Elements;

        FanControlScreen();
        virtual ~FanControlScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

        virtual Co2Display::ScreenEvents getScreenEvent(SDL_Point pos);

        void setFanAuto(Co2Display::FanAutoManStates fanAutoState);

    private:

        Co2Display::FanAutoManStates fanAutoState_;
        bool fanAutoChanged_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  ShutdownRebootScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class ShutdownRebootScreen: public Co2Screen
{
    public:

        typedef enum {
            FirstElement,
            Reboot = FirstElement,
            RebootText,
            Shutdown,
            ShutdownText,
            LastElement
        } Elements;

        ShutdownRebootScreen();
        virtual ~ShutdownRebootScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

        virtual Co2Display::ScreenEvents getScreenEvent(SDL_Point pos);

    private:

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  ConfirmCancelScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class ConfirmCancelScreen: public Co2Screen
{
    public:

        typedef enum {
            FirstElement,
            RebootText = FirstElement,
            ShutdownText,
            Confirm,
            Cancel,
            LastElement
        } Elements;

        ConfirmCancelScreen();
        virtual ~ConfirmCancelScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        void setConfirmAction(Co2Display::ScreenEvents confirmAction);
        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

        virtual Co2Display::ScreenEvents getScreenEvent(SDL_Point pos);

    private:
        Co2Display::ScreenEvents confirmAction_;

};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  BlankScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class BlankScreen: public Co2Screen
{
    public:

        typedef enum {
            FirstElement,
            LastElement
        } Elements;

        BlankScreen();
        virtual ~BlankScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

    private:
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  SplashScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

class SplashScreen: public Co2Screen
{
    public:

        typedef enum {
            FirstElement,
            Splash = FirstElement,
            LastElement
        } Elements;

        SplashScreen();
        virtual ~SplashScreen();

        virtual void init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts);

        virtual void draw(bool refreshOnly = true);
        //virtual void draw(int element, bool refreshOnly = true);
        //virtual void draw(std::vector<int>& elements, bool clearScreen = false,  bool refreshOnly = true);

    private:
};

#endif /* CO2SCREEN_H */
