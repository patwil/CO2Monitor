/*
 * displayElement.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef DISPLAYELEMENT_H
#define DISPLAYELEMENT_H

#include <iostream>
#include "SDL.h"
#include "SDL_thread.h"
#include <SDL_ttf.h>

class DisplayElement
{
    public:

        virtual ~DisplayElement();

        void draw(bool doNotClear = false);
        void redraw();
        void setNeedsRedraw() { needsRedraw_ = true; };
        void clear();
        bool wasHit(SDL_Point point);
        void setClearBeforeDraw() { clearBeforeDraw_ = true; needsRedraw_ = true; };

    private:


    protected:
        DisplayElement();

        bool needsRedraw_;
        bool clearBeforeDraw_;
        SDL_Surface* screen_;
        SDL_Surface* display_;
        SDL_Rect position_;
        SDL_Color backgroundColour_;
        uint32_t backgroundColourRGB_;
};


class DisplayImage : public DisplayElement
{
    public:
        DisplayImage(SDL_Surface* screen,
                     SDL_Rect* position,
                     SDL_Color backgroundColour,
                     std::string& bitmap,
                     SDL_Point colourKey = {1, 1});

        virtual ~DisplayImage();

    private:
        DisplayImage();

    protected:
};


class DisplayText : public DisplayElement
{
    public:
        DisplayText(SDL_Surface* screen,
                    SDL_Rect* position,
                    SDL_Color foregroundColour,
                    SDL_Color backgroundColour,
                    std::string& text,
                    TTF_Font* font);

        virtual ~DisplayText();

        void setText(std::string& text);

    private:
        DisplayText();

        TTF_Font* font_;
        SDL_Color foregroundColour_;

    protected:
};
#endif /* DISPLAYELEMENT_H */
