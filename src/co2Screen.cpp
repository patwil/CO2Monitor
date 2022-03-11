/*
 * co2Screen.cpp
 *
 * Created on: 2016-11-13
 *     Author: patw
 */

#include "co2Screen.h"

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
                           std::string bitmap)
{
    displayElements_[element] = new DisplayImage(screen_,
                                                 position,
                                                 backgroundColour,
                                                 bitmap);
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
        return EXIT_SUCCESS;       /* shouldn't happen, but avoids warnings */
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


