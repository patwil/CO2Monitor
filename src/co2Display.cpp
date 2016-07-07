/*
 * co2Display.cpp
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#include <stdlib.h>
#include "co2Display.h"

Co2Display::Co2Display(zmq::context_t &ctx, int sockType)
{
    int rc = 0;

    if (cfg.find("SDL_FBDEV") != cfg.end()) {
        rc = setenv("SDL_FBDEV", cfg.find("SDL_FBDEV")->second->getStr(), 0);
        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_FBDEV\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_FBDEV\" missing from config\n");
    }

    if (cfg.find("SDL_MOUSEDEV") != cfg.end()) {
        rc = setenv("SDL_MOUSEDEV", cfg.find("SDL_MOUSEDEV")->second->getStr(), 0);
        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_MOUSEDEV\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_MOUSEDEV\" missing from config\n");
    }

    if (cfg.find("SDL_MOUSEDRV") != cfg.end()) {
        rc = setenv("SDL_MOUSEDRV", cfg.find("SDL_MOUSEDRV")->second->getStr(), 0);
        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_MOUSEDRV\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_MOUSEDRV\" missing from config\n");
    }

    if (cfg.find("SDL_VIDEODRIVER") != cfg.end()) {
        rc = setenv("SDL_VIDEODRIVER", cfg.find("SDL_VIDEODRIVER")->second->getStr(), 0);
        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_VIDEODRIVER\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_VIDEODRIVER\" missing from config\n");
    }

    if (cfg.find("SDL_MOUSE_RELATIVE") != cfg.end()) {
        rc = setenv("SDL_MOUSE_RELATIVE", cfg.find("SDL_MOUSE_RELATIVE")->second->getStr(), 0);
        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_MOUSE_RELATIVE\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_MOUSE_RELATIVE\" missing from config\n");
    }
}

Co2Display::Co2Display(const Co2Display& rhs)
{
    // Assign new dynamic memory and and copy over data.

}


Co2Display::~Co2Display()
{
    // Delete all dynamic memory.
}

Co2Display& Co2Display::operator=(const Co2Display& rhs)
{
    // Don't copy to self.
    if (this != &rhs) {
        // Make sure that the base class assigns too.
        // BaseClass::operator= (rhs);

        // Delete existing memory from this and assign new.
        // Copy data from (non-NULL) pointers into new memory.
        // Assign all data members.

    }

    //
    return *this;
}

// Address of operator (non const).
//  The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//    Co2Display *c1 = &c2;

Co2Display* Co2Display::operator&()
{
    return (this);
}

// Address of operator (const).
//  The system will always provide one so make this private if you don't
//  want it. Remove the function if the default is ok.
// Used:
//    const Co2Display *c1 = &c2;

const Co2Display* Co2Display::operator&() const
{
    return (this);
}

void Co2Display::run()
{
}
