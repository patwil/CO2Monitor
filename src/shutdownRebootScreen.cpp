/*
 * shutdownRebootScreen.cpp
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
    text = "Restart";
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
    text = "Shut Down";
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


