/*
 * confirmCancelScreen.cpp
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
    text = "   Restart?";
    fgColour = {0xff, 0x24, 0x00};
    bgColour = {0, 0, 0};
    position = {100, 80, 0, 0};
    fontSize = Co2Display::Large;

    addElement(element, &position, fgColour, bgColour, text, fontSize);

    /////////////////////////////////////////////////////////////////////////////////////////////////////
    element = static_cast<int>(ShutdownText);
    text = "Shut Down?";
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


