/*
 * co2Screen.cpp
 *
 * Created on: 2016-11-13
 *     Author: patw
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include "displayElement.h"
#include "co2Screen.h"
#include "co2Display.h"

Co2Screen::Co2Screen() :
    screen_(nullptr),
    initComplete_(false)
{
}

Co2Screen::~Co2Screen()
{
    // Delete all dynamic memory.
    for (auto& e: displayElements_) {
        delete e.second;
    }
}

void Co2Screen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    if (!window) {
        throw CO2::exceptionLevel("null window arg for Co2Screen::init", true);
    }

    window_ = window;
    screen_ = SDL_GetWindowSurface(window_);
    if (!screen_) {
        throw CO2::exceptionLevel("Failed to get window surface in Co2Screen::init", true);
    }
    sdlBitMapDir_ = sdlBmpDir;

    fonts_ = fonts;
}

void Co2Screen::draw(bool refreshOnly)
{
    if (refreshOnly) {

        for (auto& e: displayElements_) {
            e.second->redraw();
        }

    } else {
        clear();

        for (auto& e: displayElements_) {
            e.second->draw(true);
        }
    }
    SDL_UpdateWindowSurface(window_);
}

void Co2Screen::draw(int element, bool refreshOnly)
{
    if (needsRedraw()) {
        refreshOnly = false;
    }

    if (refreshOnly) {
        displayElements_[element]->redraw();
    } else {
        displayElements_[element]->draw();
    }
    SDL_UpdateWindowSurface(window_);
}

void Co2Screen::draw(std::vector<int>& elements, bool clearScreen, bool refreshOnly)
{
    if (clearScreen) {
        clear();
    }

    if (needsRedraw()) {
        refreshOnly = false;
    }

    if (refreshOnly) {
        for (auto& e: elements) {
            displayElements_[e]->redraw();
        }
    } else {
        // Use this name as it has a different meaning in DisplayElement.draw().
        // We don't need to clear element before drawing because we've just cleared
        // the screen.
        bool isAlreadyClear = clearScreen;

        for (auto &e: elements) {
            displayElements_[e]->draw(isAlreadyClear);
        }
    }
    SDL_UpdateWindowSurface(window_);
    unsetNeedsRedraw();
}

void Co2Screen::clear()
{
    SDL_FillRect(screen_, NULL, SDL_MapRGB(screen_->format, 0, 0, 0));
    SDL_UpdateWindowSurface(window_);
    setNeedsRedraw();
}

void Co2Screen::addElement(int element,
                           SDL_Rect* position,
                           SDL_Color backgroundColour,
                           std::string bitmap,
                           SDL_Point colourKey)
{
    displayElements_[element] = new DisplayImage(screen_,
                                                 position,
                                                 backgroundColour,
                                                 bitmap,
                                                 colourKey);
}

void Co2Screen::addElement(int element,
                           SDL_Rect* position,
                           SDL_Color foregroundColour,
                           SDL_Color backgroundColour,
                           std::string& text,
                           Co2Display::FontSizes fontSize)
{
    displayElements_[element] = new DisplayText(screen_,
                                                position,
                                                foregroundColour,
                                                backgroundColour,
                                                text,
                                                (*fonts_)[fontSize].font);
}

void Co2Screen::setElementText(int element, std::string& text)
{
    DisplayText* textElement = dynamic_cast<DisplayText*>(displayElements_[element]);

    if (textElement) {
        textElement->setText(text);
    } else {
        throw CO2::exceptionLevel("Attempt to set text in non-text display element", true);
    }
}

Co2Display::ScreenEvents Co2Screen::getScreenEvent(SDL_Point pos)
{
    // If we're calling this base function it means that the
    // derived class doesn't support any input events.
    //
    return Co2Display::None;
}

uint32_t Co2Screen::getpixel(SDL_Surface *surface, SDL_Point point)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    uint8_t *p = (uint8_t *)surface->pixels + point.y * surface->pitch + point.x * bpp;

    switch(bpp) {
    case 1:
        return *p;
        break;

    case 2:
        return *(Uint16 *)p;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;
        break;

    case 4:
        return *(uint32_t *)p;
        break;

    default:
        return 0;       /* shouldn't happen, but avoids warnings */
    }
}

void Co2Screen::putpixel(SDL_Surface *surface, SDL_Point point, uint32_t pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    uint8_t *p = (uint8_t *)surface->pixels + point.y * surface->pitch + point.x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(uint32_t *)p = pixel;
        break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  StatusScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

StatusScreen::StatusScreen() :
    temperatureChanged_(false),
    relHumChanged_(false),
    co2Changed_(false),
    fanStateOn_(false),
    fanStateChanged_(false),
    fanAuto_(false),
    fanAutoChanged_(false),
    fanManOnEndTime_(0),
    wifiStateOn_(false),
    wifiStateChanged_(false)
{
}

StatusScreen::~StatusScreen()
{
    // Delete all dynamic memory.
}

void StatusScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    int          element;
    SDL_Color    fgColour;
    SDL_Color    bgColour;
    SDL_Rect     position;
    std::string  text;
    Co2Display::FontSizes fontSize;

    this->Co2Screen::init(window, sdlBmpDir, fonts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(TemperatureText);
    text = "Temp:";
    fgColour = {0xff, 0x24, 0x00};
    bgColour = {0, 0, 0};
    position = {0, 0, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(TemperatureValue);
    text.clear();
    text = std::to_string(0);
    position = {250, 0, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(TemperatureUnitText_1);
    text.clear();
    text = "C";
    position = {436, 0, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(TemperatureUnitText_2);
    text.clear();
    text = "o";
    position = {416, 0, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumText);
    text.clear();
    text = "Humidity:";
    fgColour = {0x33, 0x66, 0xff};
    bgColour = {0, 0, 0};
    position = {0, 180, 0, 0};
    fontSize = Co2Display::Medium;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumValue);
    text.clear();
    text = std::to_string(0);
    position = {260, 160, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumUnitText);
    text.clear();
    text = "%";
    position = {420, 174, 0, 0};
    fontSize = Co2Display::Medium;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2Text_1);
    text.clear();
    text = "CO :";
    fgColour = {0x33, 0xcc, 0x33};
    bgColour = {0, 0, 0};
    position = {0, 320, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2Text_2);
    text.clear();
    text = "2";
    fgColour = {0x33, 0xcc, 0x33};
    bgColour = {0, 0, 0};
    position = {120, 368, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2Value);
    text.clear();
    text = std::to_string(0);
    position = {200, 320, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2UnitText);
    text.clear();
    text = "ppm";
    position = {370, 360, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOverrideAutoText);
    text.clear();
    text = "Auto";
    fgColour = {0xff, 0xff, 0x00};
    bgColour = {0, 0, 0};
    position = {500, 160, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOverrideManText);
    text.clear();
    text = "Man";
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanManOnCountdown);
    text.clear();
    text = "0:00:00";
    position = {480, 220, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    ////////////////////////////////////////////////////////////////////////////////////////////////////

    int fanOnImageIndexOffset = static_cast<int>(FanOnFirst);
    position = {496, 36, 0, 0};

    for (fanOnImageIndex_ = 0; fanOnImageIndex_ <= (static_cast<int>(FanOnLast) - fanOnImageIndexOffset); fanOnImageIndex_++) {

        element = fanOnImageIndex_ + fanOnImageIndexOffset;
        text.clear();
        text = sdlBitMapDir_ + "fan-pos" + CO2::zeroPadNumber(2, fanOnImageIndex_) + ".bmp";

        addElement(element, &position, bgColour, text);
    }

    fanOnImageIndex_ = 0;

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOff);
    text.clear();
    text = sdlBitMapDir_ + "fan-off.bmp";

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(WiFiStateOff);
    text.clear();
    text = sdlBitMapDir_ + "wireless-off.bmp";
    bgColour = {0, 0, 0};
    position = {496, 316, 0, 0};
    SDL_Point colourKeyPos = { 1, 32 };

    addElement(element, &position, bgColour, text, colourKeyPos);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(WiFiStateOn);
    text.clear();
    text = sdlBitMapDir_ + "wireless-on.bmp";

    addElement(element, &position, bgColour, text, colourKeyPos);

    initComplete_ = true;
}

void StatusScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    std::vector<int> elements;

    if (needsRedraw()) {
        refreshOnly = false;
    }

    if (refreshOnly) {
        if (temperatureChanged_) {
            elements.push_back(static_cast<int>(TemperatureValue));
        }
        if (relHumChanged_) {
            elements.push_back(static_cast<int>(RelHumValue));
        }
        if (co2Changed_) {
            elements.push_back(static_cast<int>(Co2Value));
        }
        if (fanAutoChanged_) {
            if (fanAuto_) {
                elements.push_back(static_cast<int>(FanOverrideAutoText));
            } else {
                elements.push_back(static_cast<int>(FanOverrideManText));
            }
        }
        if (fanStateChanged_ && !fanStateOn_) {
            elements.push_back(static_cast<int>(FanOff));
        } else {
            elements.push_back(static_cast<int>(FanOnFirst) + fanOnImageIndex_);
        }
        if (fanStateOn_ && !fanAuto_) {
            updateFanManOnCountdown();
            elements.push_back(static_cast<int>(FanManOnCountdown));
        }
        if (wifiStateChanged_) {
            if (wifiStateOn_) {
                elements.push_back(static_cast<int>(WiFiStateOn));
            } else {
                elements.push_back(static_cast<int>(WiFiStateOff));
            }
        }
    } else {
        elements.push_back(static_cast<int>(TemperatureText));
        elements.push_back(static_cast<int>(TemperatureValue));
        elements.push_back(static_cast<int>(TemperatureUnitText_1));
        elements.push_back(static_cast<int>(TemperatureUnitText_2));
        elements.push_back(static_cast<int>(RelHumText));
        elements.push_back(static_cast<int>(RelHumValue));
        elements.push_back(static_cast<int>(RelHumUnitText));
        elements.push_back(static_cast<int>(Co2Text_1));
        elements.push_back(static_cast<int>(Co2Text_2));
        elements.push_back(static_cast<int>(Co2Value));
        elements.push_back(static_cast<int>(Co2UnitText));
        if (fanAuto_) {
            elements.push_back(static_cast<int>(FanOverrideAutoText));
        } else {
            elements.push_back(static_cast<int>(FanOverrideManText));
            if (fanStateOn_) {
                updateFanManOnCountdown();
                elements.push_back(static_cast<int>(FanManOnCountdown));
            }
        }
        if (fanStateOn_) {
            elements.push_back(static_cast<int>(FanOnFirst) + fanOnImageIndex_);
        } else {
            elements.push_back(static_cast<int>(FanOff));
        }
        if (wifiStateOn_) {
            elements.push_back(static_cast<int>(WiFiStateOn));
        } else {
            elements.push_back(static_cast<int>(WiFiStateOff));
        }
    }

    this->Co2Screen::draw(elements, !refreshOnly, refreshOnly);

    if (fanStateOn_) {
        fanOnImageIndex_++;
        if ((fanOnImageIndex_ + static_cast<int>(FanOnFirst)) > static_cast<int>(FanOnLast)) {
            fanOnImageIndex_ = 0;
        }
        setNeedsRedraw();
    }
}

void StatusScreen::setTemperature(int temperature)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    double fTemperature = (temperature * 1.0) / 100;

    int element = static_cast<int>(TemperatureValue);
    std::string text = CO2::zeroPadNumber(2, fTemperature, ' ', 1);

    setElementText(element, text);

    temperatureChanged_ = true;
    setNeedsRedraw();
}

void StatusScreen::setRelHumidity(int relHumidity)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    double fRelHumidity = (relHumidity * 1.0) / 100;

    int element = static_cast<int>(RelHumValue);
    std::string text = CO2::zeroPadNumber(2, fRelHumidity, ' ', 1);

    setElementText(element, text);

    relHumChanged_ = true;
    setNeedsRedraw();
}

void StatusScreen::setCo2(int co2)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    int element = static_cast<int>(Co2Value);
    std::string text = CO2::zeroPadNumber(4, co2, ' ');

    setElementText(element, text);

    co2Changed_ = true;
    setNeedsRedraw();
}

void StatusScreen::setFanState(bool isOn)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    if (fanStateOn_ != isOn) {
        fanStateOn_ = isOn;
        fanStateChanged_ = true;
        setNeedsRedraw();
    }
}

void StatusScreen::setFanAuto(bool isAuto)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    if (fanAuto_ != isAuto) {
        fanAuto_ = isAuto;
        fanAutoChanged_ = true;
        setNeedsRedraw();
    }
}

void StatusScreen::startFanManOnTimer(time_t duration)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    fanManOnEndTime_ = time(0) + duration;
}

void StatusScreen::stopFanManOnTimer()
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    if (fanManOnEndTime_) {
        fanManOnEndTime_ = 0;
        setNeedsRedraw();
    }
}

void StatusScreen::updateFanManOnCountdown()
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    int element = static_cast<int>(FanManOnCountdown);
    std::string text;

    if (fanManOnEndTime_) {
        time_t timeRemaining = fanManOnEndTime_ - time(0);
        if (timeRemaining > 0) {
            time_t hours = timeRemaining / 3600;
            time_t minutes = (timeRemaining % 3600) / 60;
            time_t seconds = timeRemaining % 60;
            std::ostringstream ss;

            if (hours > 0) {
                ss << std::setw(1) << std::setfill('0') << hours;
                ss << ":" << std::setw(2) << std::setfill('0') << minutes;
                ss << ":" << std::setw(2) << std::setfill('0') << seconds;
            } else if (minutes > 0) {
                ss << "  " << std::setw(2) << std::setfill(' ') << minutes;
                ss << ":" << std::setw(2) << std::setfill('0') << seconds;
            } else {
                ss << "     " << std::setw(2) << std::setfill(' ') << seconds;
            }

            text = ss.str();
        } else {
            fanManOnEndTime_ = 0;
            text = "      0";
        }
    } else {
        text = "        ";
    }
    setElementText(element, text);
}

void StatusScreen::setWiFiState(bool isOn)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    if (wifiStateOn_ != isOn) {
        wifiStateOn_ = isOn;
        wifiStateChanged_ = true;
        setNeedsRedraw();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  RelHumCo2ThresholdScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

RelHumCo2ThresholdScreen::RelHumCo2ThresholdScreen() :
    relHumThresholdChanged_(false),
    co2ThresholdChanged_(false)
{
}

RelHumCo2ThresholdScreen::~RelHumCo2ThresholdScreen()
{
    // Delete all dynamic memory.
}

void RelHumCo2ThresholdScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    int          element;
    SDL_Color    fgColour;
    SDL_Color    bgColour;
    SDL_Rect     position;
    std::string  text;
    Co2Display::FontSizes fontSize;

    this->Co2Screen::init(window, sdlBmpDir, fonts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(TitleText);
    text = "Thresholds";
    fgColour = {0xff, 0x00, 0xe6};
    bgColour = {0, 0, 0};
    position = {100, 0, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumText);
    text.clear();
    text = "Humidity:";
    fgColour = {0x33, 0x66, 0xff};
    bgColour = {0, 0, 0};
    position = {0, 130, 0, 0};
    fontSize = Co2Display::Medium;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumValue);
    text.clear();
    text = CO2::zeroPadNumber(2, 0, ' ');
    position = {280, 110, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumUnitText);
    text.clear();
    text = "%";
    bgColour = {0, 0, 0};
    position = {380, 110, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2Text_1);
    text.clear();
    text = "CO :";
    fgColour = {0x33, 0xcc, 0x33};
    bgColour = {0, 0, 0};
    position = {0, 310, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2Text_2);
    text.clear();
    text = "2";
    position = {120, 358, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2Value);
    text.clear();
    text = CO2::zeroPadNumber(2, 0, ' ');
    position = {180, 310, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2UnitText);
    text = "ppm";
    position = {360, 350, 0, 0};
    fontSize = Co2Display::Small;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumControlUp);
    text.clear();
    text = sdlBitMapDir_ + "arrow-up-blue.bmp";
    bgColour = {0, 0, 0};
    position = {500, 90, 0, 0};
    SDL_Point colourKeyPos = { 1, 1 };

    addElement(element, &position, bgColour, text, colourKeyPos);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumControlDown);
    text.clear();
    text = sdlBitMapDir_ + "arrow-down-orange.bmp";
    bgColour = {0, 0, 0};
    position = {500, 186, 0, 0};
    colourKeyPos = { 1, 39 };

    addElement(element, &position, bgColour, text, colourKeyPos);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2ControlUp);
    text.clear();
    text = sdlBitMapDir_ + "arrow-up-red.bmp";
    bgColour = {0, 0, 0};
    position = {500, 300, 0, 0};
    colourKeyPos = { 1, 1 };

    addElement(element, &position, bgColour, text, colourKeyPos);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2ControlDown);
    text.clear();
    text = sdlBitMapDir_ + "arrow-down-green.bmp";
    bgColour = {0, 0, 0};
    position = {500, 396, 0, 0};
    colourKeyPos = { 1, 39 };

    addElement(element, &position, bgColour, text, colourKeyPos);

    initComplete_ = true;
}

void RelHumCo2ThresholdScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    std::vector<int> elements;

    if (refreshOnly) {
        if (relHumThresholdChanged_) {
            elements.push_back(static_cast<int>(RelHumValue));
        }
        if (co2ThresholdChanged_) {
            elements.push_back(static_cast<int>(Co2Value));
        }
    } else {
        elements.push_back(static_cast<int>(TitleText));
        elements.push_back(static_cast<int>(RelHumText));
        elements.push_back(static_cast<int>(RelHumValue));
        elements.push_back(static_cast<int>(RelHumUnitText));
        elements.push_back(static_cast<int>(Co2Text_1));
        elements.push_back(static_cast<int>(Co2Text_2));
        elements.push_back(static_cast<int>(Co2Value));
        elements.push_back(static_cast<int>(Co2UnitText));
        elements.push_back(static_cast<int>(RelHumControlUp));
        elements.push_back(static_cast<int>(RelHumControlDown));
        elements.push_back(static_cast<int>(Co2ControlUp));
        elements.push_back(static_cast<int>(Co2ControlDown));
    }

    this->Co2Screen::draw(elements, !refreshOnly, refreshOnly);
}

Co2Display::ScreenEvents RelHumCo2ThresholdScreen::getScreenEvent(SDL_Point pos)
{
    Co2Display::ScreenEvents screenEvent = Co2Display::None;

    for (Elements e = FirstElement;
         (e < LastElement) && (screenEvent == Co2Display::None);
         e = static_cast<Elements>(static_cast<int>(e) + 1)) {
        if (displayElements_[e]->wasHit(pos)) {
            switch (e) {
            case RelHumControlUp:
                screenEvent = Co2Display::RelHumUp;
                break;
            case RelHumControlDown:
                screenEvent = Co2Display::RelHumDown;
                break;
            case Co2ControlUp:
                screenEvent = Co2Display::Co2Up;
                break;
            case Co2ControlDown:
                screenEvent = Co2Display::Co2Down;
                break;
            default:
                break;
            }
        }
    }

    return screenEvent;
}

void RelHumCo2ThresholdScreen::setRelHumThreshold(int relHumThreshold)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    int element = static_cast<int>(RelHumValue);
    std::string text = std::to_string(relHumThreshold);

    setElementText(element, text);

    relHumThresholdChanged_ = true;
}

void RelHumCo2ThresholdScreen::setCo2Threshold(int co2Threshold)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    int element = static_cast<int>(Co2Value);
    std::string text = std::to_string(co2Threshold);

    setElementText(element, text);

    co2ThresholdChanged_ = true;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  FanControlScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

FanControlScreen::FanControlScreen() :
    fanAutoChanged_(false)
{
}

FanControlScreen::~FanControlScreen()
{
    // Delete all dynamic memory.
}

void FanControlScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    int          element;
    SDL_Color    fgColour;
    SDL_Color    bgColour;
    SDL_Rect     position;
    std::string  text;
    Co2Display::FontSizes fontSize;

    this->Co2Screen::init(window, sdlBmpDir, fonts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(TitleText);
    text = "Fan Control";
    fgColour = {0xff, 0x99, 0x00};
    bgColour = {0, 0, 0};
    position = {100, 0, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOverrideAutoText);
    text.clear();
    text = "      Auto";
    fgColour = {0xff, 0xff, 0x00};
    position = {110, 110, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOverrideManOnText);
    text.clear();
    text = "Manual - On";
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOverrideManOffText);
    text.clear();
    text = "Manual - Off";
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOnActive);
    text.clear();
    text = sdlBitMapDir_ + "on-active.bmp";
    bgColour = {0, 0, 0};
    position = {40, 260, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOnInactive);
    text.clear();
    text = sdlBitMapDir_ + "on-inactive.bmp";

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanAutoActive);
    text.clear();
    text = sdlBitMapDir_ + "auto-active.bmp";
    position = {240, 260, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanAutoInactive);
    text.clear();
    text = sdlBitMapDir_ + "auto-inactive.bmp";

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOffActive);
    text.clear();
    text = sdlBitMapDir_ + "off-active.bmp";
    position = {440, 260, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(FanOffInactive);
    text.clear();
    text = sdlBitMapDir_ + "off-inactive.bmp";

    addElement(element, &position, bgColour, text);

    initComplete_ = true;
}

void FanControlScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    std::vector<int> elements;

    if (fanAutoChanged_) {
        refreshOnly = false;
        fanAutoChanged_ = false;
    }

    if (!refreshOnly) {
        elements.push_back(static_cast<int>(TitleText));

        switch (fanAutoState_) {
        case Co2Display::ManOn:
            elements.push_back(static_cast<int>(FanOverrideManOnText));
            elements.push_back(static_cast<int>(FanOnActive));
            elements.push_back(static_cast<int>(FanAutoInactive));
            elements.push_back(static_cast<int>(FanOffInactive));
            break;

        case Co2Display::ManOff:
            elements.push_back(static_cast<int>(FanOverrideManOffText));
            elements.push_back(static_cast<int>(FanOnInactive));
            elements.push_back(static_cast<int>(FanAutoInactive));
            elements.push_back(static_cast<int>(FanOffActive));
            break;

        case Co2Display::Auto:
            elements.push_back(static_cast<int>(FanOverrideAutoText));
            elements.push_back(static_cast<int>(FanOnInactive));
            elements.push_back(static_cast<int>(FanAutoActive));
            elements.push_back(static_cast<int>(FanOffInactive));
            break;

        default:
            break;
        }
    }

    this->Co2Screen::draw(elements, !refreshOnly, refreshOnly);
}

Co2Display::ScreenEvents FanControlScreen::getScreenEvent(SDL_Point pos)
{
    Co2Display::ScreenEvents screenEvent = Co2Display::None;

    for (Elements e = FirstElement;
         (e < LastElement) && (screenEvent == Co2Display::None);
         e = static_cast<Elements>(static_cast<int>(e) + 1)) {
        if (displayElements_[e]->wasHit(pos)) {
            switch (e) {
            case FanOnActive:
            case FanOnInactive:
                if (fanAutoState_ != Co2Display::ManOn) {
                    screenEvent = Co2Display::FanOn;
                }
                break;
            case FanAutoActive:
            case FanAutoInactive:
                if (fanAutoState_ != Co2Display::Auto) {
                    screenEvent = Co2Display::FanAuto;
                }
                break;
            case FanOffActive:
            case FanOffInactive:
                if (fanAutoState_ != Co2Display::ManOff) {
                    screenEvent = Co2Display::FanOff;
                }
                break;
            default:
                break;
            }
        }
    }
    return screenEvent;
}

void FanControlScreen::setFanAuto(Co2Display::FanAutoManStates fanAutoState)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    fanAutoState_ = fanAutoState;
    fanAutoChanged_ = true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  ShutdownRebootScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

ShutdownRebootScreen::ShutdownRebootScreen()
{
}

ShutdownRebootScreen::~ShutdownRebootScreen()
{
    // Delete all dynamic memory.
}

void ShutdownRebootScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    int          element;
    SDL_Color    fgColour;
    SDL_Color    bgColour;
    SDL_Rect     position;
    std::string  text;
    Co2Display::FontSizes fontSize;

    this->Co2Screen::init(window, sdlBmpDir, fonts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Reboot);
    text.clear();
    text = sdlBitMapDir_ + "reboot.bmp";
    bgColour = {0, 0, 0};
    position = {40, 80, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RebootText);
    text = "Reboot";
    fgColour = {0xcc, 0xcc, 0xff};
    position = {50, 350, 0, 0};
    fontSize = Co2Display::Medium;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Shutdown);
    text.clear();
    text = sdlBitMapDir_ + "shutdown.bmp";
    position = {360, 80, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(ShutdownText);
    text = "Shutdown";
    position = {340, 350, 0, 0};
    fontSize = Co2Display::Medium;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    initComplete_ = true;
}

void ShutdownRebootScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    std::vector<int> elements;

    if (!refreshOnly) {
        elements.push_back(static_cast<int>(Reboot));
        elements.push_back(static_cast<int>(RebootText));
        elements.push_back(static_cast<int>(Shutdown));
        elements.push_back(static_cast<int>(ShutdownText));

        this->Co2Screen::draw(elements, !refreshOnly, refreshOnly);
    }
}

Co2Display::ScreenEvents ShutdownRebootScreen::getScreenEvent(SDL_Point pos)
{
    Co2Display::ScreenEvents screenEvent = Co2Display::None;

    for (Elements e = FirstElement; e < LastElement; e = static_cast<Elements>(static_cast<int>(e) + 1)) {
        if (displayElements_[e]->wasHit(pos)) {
            switch (e) {
            case Reboot:
                screenEvent = Co2Display::Reboot;
                break;
            case Shutdown:
                screenEvent = Co2Display::Shutdown;
                break;
            default:
                break;
            }
        }
    }
    return screenEvent;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  ConfirmCancelScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

ConfirmCancelScreen::ConfirmCancelScreen() :
    confirmAction_(Co2Display::None)
{
}

ConfirmCancelScreen::~ConfirmCancelScreen()
{
    // Delete all dynamic memory.
}

void ConfirmCancelScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    int          element;
    SDL_Color    fgColour;
    SDL_Color    bgColour;
    SDL_Rect     position;
    std::string  text;
    Co2Display::FontSizes fontSize;

    this->Co2Screen::init(window, sdlBmpDir, fonts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RebootText);
    text = "   Reboot?";
    fgColour = {0xff, 0x24, 0x00};
    bgColour = {0, 0, 0};
    position = {100, 80, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(ShutdownText);
    text = "Shutdown?";
    fgColour = {0xff, 0x24, 0x00};
    bgColour = {0, 0, 0};
    position = {100, 80, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Confirm);
    text.clear();
    text = sdlBitMapDir_ + "confirm-button.bmp";
    bgColour = {0, 0, 0};
    position = {40, 280, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Cancel);
    text.clear();
    text = sdlBitMapDir_ + "cancel-button.bmp";
    bgColour = {0, 0, 0};
    position = {400, 280, 0, 0};

    addElement(element, &position, bgColour, text);

    initComplete_ = true;
}

void ConfirmCancelScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    std::vector<int> elements;

    if (!refreshOnly) {
        switch (confirmAction_) {
        case Co2Display::Reboot:
            elements.push_back(static_cast<int>(RebootText));
            break;
        case Co2Display::Shutdown:
            elements.push_back(static_cast<int>(ShutdownText));
            break;
        default:
            throw CO2::exceptionLevel("Unknown confirmAction in ConfirmCancel", false);
        }

        elements.push_back(static_cast<int>(Confirm));
        elements.push_back(static_cast<int>(Cancel));

        this->Co2Screen::draw(elements, !refreshOnly, refreshOnly);
    }

}

void ConfirmCancelScreen::setConfirmAction(Co2Display::ScreenEvents confirmAction)
{
    switch (confirmAction) {
    case Co2Display::Reboot:
    case Co2Display::Shutdown:
        confirmAction_ = confirmAction;
        setNeedsRedraw();
        break;
    default:
        break;
    }
}

Co2Display::ScreenEvents ConfirmCancelScreen::getScreenEvent(SDL_Point pos)
{
    Co2Display::ScreenEvents screenEvent = Co2Display::None;

    for (Elements e = FirstElement; e < LastElement; e = static_cast<Elements>(static_cast<int>(e) + 1)) {
        if (displayElements_[e]->wasHit(pos)) {
            switch (e) {
            case Confirm:
                screenEvent = Co2Display::Confirm;
                break;
            case Cancel:
                screenEvent = Co2Display::Cancel;
                break;
            default:
                break;
            }
        }
    }
    return screenEvent;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  BlankScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

BlankScreen::BlankScreen()
{
}

BlankScreen::~BlankScreen()
{
    // Delete all dynamic memory.
}

void BlankScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    this->Co2Screen::init(window, sdlBmpDir, fonts);

    initComplete_ = true;
}

void BlankScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    this->Co2Screen::clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
///
///
///  SplashScreen
///
///
/////////////////////////////////////////////////////////////////////////////////////////////////////////

SplashScreen::SplashScreen()
{
}

SplashScreen::~SplashScreen()
{
    // Delete all dynamic memory.
}

void SplashScreen::init(SDL_Window* window, std::string& sdlBmpDir, std::array<Co2Display::FontInfo, Co2Display::NumberOfFontSizes>* fonts)
{
    int          element;
    SDL_Color    fgColour;
    SDL_Color    bgColour;
    SDL_Rect     position;
    std::string  text;
    Co2Display::FontSizes fontSize;
    this->Co2Screen::init(window, sdlBmpDir, fonts);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Splash);
    text.clear();
    text = sdlBitMapDir_ + "pwsdSplash-small.bmp";
    bgColour = {0, 0, 0};
    position = {0, 0, 0, 0};

    addElement(element, &position, bgColour, text);

    initComplete_ = true;
}

void SplashScreen::draw(bool refreshOnly)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    std::vector<int> elements;

    if (!refreshOnly) {
        elements.push_back(static_cast<int>(Splash));

        this->Co2Screen::draw(elements, !refreshOnly, refreshOnly);
    }
}


