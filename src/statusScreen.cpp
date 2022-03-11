/*
 * statusScreen.cpp
 *
 * Created on: 2021-11-17
 *     Author: patw
 */

//#include <iostream>
//#include <iomanip>
//#include <sstream>
//#include <string>
//#include "displayElement.h"
#include "co2Screen.h"
//#include "co2Display.h"

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
    position = {412, 0, 0, 0};
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
    bgColour = {0, 0, 0};
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

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(WiFiStateOn);
    text.clear();
    text = sdlBitMapDir_ + "wireless-on.bmp";

    addElement(element, &position, bgColour, text);

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
        } else if (fanStateOn_) {
            elements.push_back(static_cast<int>(FanOnFirst) + fanOnImageIndex_);
            displayElements_[static_cast<int>(FanOnFirst) + fanOnImageIndex_]->setClearBeforeDraw();
        }
        if (fanStateOn_ && !fanAuto_) {
            updateFanManOnCountdown();
            elements.push_back(static_cast<int>(FanManOnCountdown));
        }
        if (wifiStateChanged_) {
            int wifiStateIdx = wifiStateOn_ ? static_cast<int>(WiFiStateOn) : static_cast<int>(WiFiStateOff);
            elements.push_back(wifiStateIdx);
            displayElements_[wifiStateIdx]->setClearBeforeDraw();
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
            displayElements_[static_cast<int>(FanOnFirst) + fanOnImageIndex_]->setClearBeforeDraw();
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
        fanOnImageIndex_ = 0;
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
    }
}


