/*
 * relHumCo2ThresholdScreen.cpp
 *
 * Created on: 2021-11-17
 *     Author: patw
 */

#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include "displayElement.h"
#include "co2Screen.h"
#include "co2Display.h"

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
    text = CO2::zeroPadNumber(4, 0, ' ');
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

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(RelHumControlDown);
    text.clear();
    text = sdlBitMapDir_ + "arrow-down-orange.bmp";
    bgColour = {0, 0, 0};
    position = {500, 186, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2ControlUp);
    text.clear();
    text = sdlBitMapDir_ + "arrow-up-red.bmp";
    bgColour = {0, 0, 0};
    position = {500, 300, 0, 0};

    addElement(element, &position, bgColour, text);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(Co2ControlDown);
    text.clear();
    text = sdlBitMapDir_ + "arrow-down-green.bmp";
    bgColour = {0, 0, 0};
    position = {500, 396, 0, 0};

    addElement(element, &position, bgColour, text);

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
    std::string text = CO2::zeroPadNumber(2, relHumThreshold, ' ');

    setElementText(element, text);

    relHumThresholdChanged_ = true;
}

void RelHumCo2ThresholdScreen::setCo2Threshold(int co2Threshold)
{
    if (!initComplete_) {
        throw CO2::exceptionLevel("Screen not initialised", true);
    }

    int element = static_cast<int>(Co2Value);
    std::string text = CO2::zeroPadNumber(4, co2Threshold, ' ');

    setElementText(element, text);

    co2ThresholdChanged_ = true;
}



