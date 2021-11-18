/*
 * blankScreen.cpp
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


