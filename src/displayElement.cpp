/*
 * displayElement.cpp
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#include <syslog.h>
#include "displayElement.h"
//#include "co2Screen.h"
#include "co2Display.h"

DisplayElement::DisplayElement()
{
}

DisplayElement::~DisplayElement()
{
    // Delete all dynamic memory.
    if (display_) {
        SDL_FreeSurface(display_);
        display_ = 0;
    }
}

void DisplayElement::draw(bool doNotClear)
{
    if (doNotClear) {
        clearBeforeDraw_ = false;
    } else if (clearBeforeDraw_) {
        clear();
    }
    if (SDL_BlitSurface(display_, NULL, screen_, &position_)) {
        syslog(LOG_ERR, "SDL_BlitSurface error: %s", SDL_GetError());
        throw CO2::exceptionLevel("SDL_BlitSurface error", true);
    }
    // syslog(LOG_DEBUG, "%s OK (pos={%d,%d}", __FUNCTION__, position_.x, position_.y);
    needsRedraw_ = false;
}

void DisplayElement::redraw()
{
    if (needsRedraw_) {
        draw();
    }
}

void DisplayElement::clear()
{
    SDL_FillRect(screen_, &position_, backgroundColourRGB_);
    GPIO_DBG_FLIP(Co2Display::GPIO_Debug_6);
    clearBeforeDraw_ = false;
}


bool DisplayElement::wasHit(SDL_Point point)
{
    bool bWasHit = (point.x >= position_.x) && (point.x <= (position_.x + display_->w)) &&
            (point.y >= position_.y) && (point.y <= (position_.y + display_->h));

    // syslog(LOG_DEBUG, "%s %s (%d,%d) {%d,%d,%d,%d}", __FUNCTION__, bWasHit ? "HIT" : "MISS",
    //        point.x, point.y, position_.x, position_.y, position_.x + display_->w, position_.y + display_->h);
    return (point.x >= position_.x) && (point.x <= (position_.x + display_->w)) &&
            (point.y >= position_.y) && (point.y <= (position_.y + display_->h));
}

DisplayImage::DisplayImage(SDL_Surface* screen,
                           SDL_Rect* position,
                           SDL_Color backgroundColour,
                           std::string& bitmap)
{
    needsRedraw_ = true;
    clearBeforeDraw_ = false;
    screen_ = screen;
    position_ = *position;
    backgroundColour_ = backgroundColour;
    backgroundColourRGB_ = SDL_MapRGB(screen_->format, backgroundColour_.r, backgroundColour_.g, backgroundColour_.b);

    display_ = SDL_LoadBMP(bitmap.c_str());
    if (!display_) {
        syslog(LOG_ERR, "SDL_DisplayFormat error for \"%s\": %s", bitmap.c_str(), SDL_GetError());
        throw CO2::exceptionLevel("SDL_DisplayFormat error", true);
    }
}

DisplayImage::~DisplayImage()
{
    // Delete all dynamic memory.
}

DisplayText::DisplayText(SDL_Surface* screen,
                         SDL_Rect* position,
                         SDL_Color foregroundColour,
                         SDL_Color backgroundColour,
                         std::string& text,
                         TTF_Font* font) :
    font_(font),
    foregroundColour_(foregroundColour)
{
    needsRedraw_ = true;
    clearBeforeDraw_ = false;
    screen_ = screen;
    position_ = *position;
    backgroundColour_ = backgroundColour;
    backgroundColourRGB_ = SDL_MapRGB(screen_->format, backgroundColour_.r, backgroundColour_.g, backgroundColour_.b);

    display_ = TTF_RenderText_Shaded(font_, text.c_str(), foregroundColour_, backgroundColour_);
    if (!display_) {
        syslog(LOG_ERR, "TTF_RenderText_Shaded return error (%s) for \"%s\"", TTF_GetError(), text.c_str());
        throw CO2::exceptionLevel("TTF_RenderText_Shaded() error", true);
    }
}

DisplayText::~DisplayText()
{
    // Delete all dynamic memory.
}

void DisplayText::setText(std::string& text)
{
    needsRedraw_ = true;
    clearBeforeDraw_ = true;

    if ((display_)) {
        SDL_FreeSurface(display_);
    }
    display_ = TTF_RenderText_Shaded(font_, text.c_str(), foregroundColour_, backgroundColour_);
    if (!display_) {
        syslog(LOG_ERR, "TTF_RenderText_Shaded return error (%s) for \"%s\"", TTF_GetError(), text.c_str());
        throw CO2::exceptionLevel("TTF_RenderText_Shaded() error", true);
    }
}


