/*
 * utils.cpp
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#include "utils.h"


void Globals::setProgName(char* pathname)
{
    lock_guard<mutex> lock(_mutex);

    char* p = strrchr(pathname, '/');

    if (p) {
        p++; // skip '/'
    }
    else {
        p = pathname;
    }
    progName = new char[strlen(p)];
    strcpy(progName, p);
}

const char* Globals::getProgName()
{
    return progName;
}

//Globals* Globals::pInstance = nullptr;

shared_ptr<Globals> globals = Globals::getInstance();
mutex Globals::_mutex;

int getLogLevelFromStr(const char* pLogLevelStr)
{
    //DEBUG (verbose), INFO, NOTICE, WARNING, ERR, CRIT, ALERT
    const char* logName2Level[] = {
        "EMERG",
        "ALERT",
        "CRIT",
        "ERR",
        "WARNING",
        "NOTICE",
        "INFO",
        "DEBUG",
        0
    };

    for (int i = 0; logName2Level[i]; i++) {
        if (!strcmp(pLogLevelStr, logName2Level[i])) {
            return i;
        }
    }
    return -1;
}

