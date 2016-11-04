/*
 * co2Display.cpp
 *
 * Created on: 2016-06-18
 *     Author: patw
 */

#include <stdlib.h>
#include <syslog.h>
#include "co2Display.h"
#include "utils.h"
#include "co2Message.pb.h"
#include <google/protobuf/text_format.h>

Co2Display::Co2Display(zmq::context_t& ctx, int sockType)
{
    ConfigMap *pCfg = CO2::globals->cfg();
    int rc = 0;

    if (!pCfg) {
        throw CO2::exceptionLevel("No config avaliable", true);
    }

    if (pCfg->find("SDL_FBDEV") != pCfg->end()) {
        rc = setenv("SDL_FBDEV", pCfg->find("SDL_FBDEV")->second->getStr(), 0);

        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_FBDEV\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_FBDEV\" missing from config\n");
    }

    if (pCfg->find("SDL_MOUSEDEV") != pCfg->end()) {
        rc = setenv("SDL_MOUSEDEV", pCfg->find("SDL_MOUSEDEV")->second->getStr(), 0);

        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_MOUSEDEV\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_MOUSEDEV\" missing from config\n");
    }

    if (pCfg->find("SDL_MOUSEDRV") != pCfg->end()) {
        rc = setenv("SDL_MOUSEDRV", pCfg->find("SDL_MOUSEDRV")->second->getStr(), 0);

        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_MOUSEDRV\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_MOUSEDRV\" missing from config\n");
    }

    if (pCfg->find("SDL_VIDEODRIVER") != pCfg->end()) {
        rc = setenv("SDL_VIDEODRIVER", pCfg->find("SDL_VIDEODRIVER")->second->getStr(), 0);

        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_VIDEODRIVER\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_VIDEODRIVER\" missing from config\n");
    }

    if (pCfg->find("SDL_MOUSE_RELATIVE") != pCfg->end()) {
        rc = setenv("SDL_MOUSE_RELATIVE", pCfg->find("SDL_MOUSE_RELATIVE")->second->getStr(), 0);

        if (rc) {
            syslog(LOG_ERR, "setenv(\"SDL_MOUSE_RELATIVE\") returned error (%d)\n", rc);
        }
    } else {
        syslog(LOG_ERR, "\"SDL_MOUSE_RELATIVE\" missing from config\n");
    }
}

Co2Display::~Co2Display()
{
    // Delete all dynamic memory.
}

