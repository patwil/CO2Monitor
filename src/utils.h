/*
 * utils.h
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <typeinfo>
#include <exception>
#include <new>
#include <cstdlib>
#include <cstring>
#include <syslog.h>

#include "config.h"

using namespace std;

class exceptionLevel: public exception
{
    string _errorStr;
    bool _isFatal;
public:
    exceptionLevel(const string errorStr="exception", bool isFatal=false) noexcept :
         _errorStr(errorStr), _isFatal(isFatal) {}
    virtual const char* what() const throw()
    {
        return _errorStr.c_str();
    }

    bool isFatal() noexcept
    {
        return _isFatal;
    }
};

class Globals
{
    static mutex _mutex;

    Globals() {};
    Globals(const Globals& rhs);
    Globals& operator=(const Globals& rhs);

    char* _progName;
    ConfigMap* _pCfg;

public:
    static shared_ptr<Globals>& getInstance() {
        static shared_ptr<Globals> instance = nullptr;
        if (!instance) {
            lock_guard<mutex> lock(_mutex);

            if (!instance) {
                instance.reset(new Globals());
            }
        }
        return instance;
    }

    void setProgName(char* pathname);
    const char* getProgName()
    {
        return _progName;
    }

    void setCfg(ConfigMap* pCfg)
    {
        lock_guard<mutex> lock(_mutex);
        if (pCfg) {
            _pCfg = pCfg;
        }
    }

    ConfigMap* getCfg()
    {
        return _pCfg;
    }
};

extern shared_ptr<Globals> globals;

int getLogLevelFromStr(const char* pLogLevelStr);
const char* getLogLevelStr(int logLevel);

#endif /*_UTILS_H_*/