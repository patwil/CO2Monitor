/*
 * splashScreen.cpp
 *
 * Created on: 2021-11-17
 *     Author: patw
 */

#include "co2Screen.h"

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


