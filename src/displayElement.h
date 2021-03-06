/*
 * displayElement.h
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#ifndef DISPLAYELEMENT_H
#define DISPLAYELEMENT_H

#include <iostream>
#include <SDL_ttf.h>

class DisplayElement
{
    public:

        virtual ~DisplayElement();

        void draw(bool doNotClear = false);
        void redraw();
        void setNeedsRedraw() {
            needsRedraw_ = true;
        };
        void clear();
        bool wasHit(SDL_Point point);
        void setClearBeforeDraw() {
            clearBeforeDraw_ = true;
            needsRedraw_ = true;
        };

    private:


    protected:
        DisplayElement();

        bool needsRedraw_;
        bool clearBeforeDraw_;
        SDL_Surface* screen_;
        SDL_Surface* display_;
        SDL_Rect position_;
        SDL_Rect alignedPosition_;
        SDL_Color backgroundColour_;
        uint32_t backgroundColourRGB_;
};


class DisplayImage : public DisplayElement
{
    public:
        DisplayImage(SDL_Surface* screen,
                     SDL_Rect* position,
                     SDL_Color backgroundColour,
                     std::string& bitmap);

        virtual ~DisplayImage();

    private:
        DisplayImage();

    protected:
};


class DisplayText : public DisplayElement
{
    public:
        typedef enum {
            Left,
            Centre,
            Center = Centre,
            Right
        } Horizontal_Alignment;

        typedef enum {
            Top,
            Middle,
            Bottom
        } Vertical_Alignment;

        DisplayText(SDL_Surface* screen,
                    SDL_Rect* position,
                    SDL_Color foregroundColour,
                    SDL_Color backgroundColour,
                    std::string& text,
                    TTF_Font* font,
                    Horizontal_Alignment hAlign = Left,
                    Vertical_Alignment vAlign = Top);

        virtual ~DisplayText();

        void setText(std::string& text);

    private:
        DisplayText();

        void alignText(std::string& text);

        TTF_Font* font_;
        SDL_Color foregroundColour_;
        Horizontal_Alignment hAlign_;
        Vertical_Alignment vAlign_;

    protected:
};
#endif /* DISPLAYELEMENT_H */
