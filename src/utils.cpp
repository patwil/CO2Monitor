/*
 * utils.cpp
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#include "utils.h"


void Globals::setProgName(char* pathname)
{
    lock_guard<mutex> lock(mutex_);

    if (progName_) {
        // can only set this once
        return;
    }

    char* p = strrchr(pathname, '/');

    if (p) {
        p++; // skip '/'
    }
    else {
        p = pathname;
    }
    progName_ = new char[strlen(p)];
    strcpy(progName_, p);
}

//Globals* Globals::pInstance = nullptr;

shared_ptr<Globals> globals = Globals::getInstance();
mutex Globals::mutex_;

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

const char* getLogLevelStr(int logLevel)
{
    switch (logLevel) {
    case LOG_EMERG:
        return "EMERG";
    case LOG_ALERT:
        return "ALERT";
    case LOG_CRIT:
        return "CRIT";
    case LOG_ERR:
        return "ERR";
    case LOG_WARNING:
        return "WARNING";
    case LOG_NOTICE:
        return "NOTICE";
    case LOG_INFO:
        return "INFO";
    case LOG_DEBUG:
        return "DEBUG";
    default:
        return "UNKNOWN";
    }
}
