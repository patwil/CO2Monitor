/*
 * fanControlScreen.cpp
 *
 * Created on: 2021-11-17
 *     Author: patw
 */

#include "co2Screen.h"

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


