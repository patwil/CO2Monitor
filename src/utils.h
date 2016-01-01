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
#include <stdlib.h>
#include <string.h>

using namespace std;

class Globals
{
    static mutex _mutex;

    Globals() {};
    Globals(const Globals& rhs);
    Globals& operator=(const Globals& rhs);

    char* progName;

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
    const char* getProgName();
};

extern shared_ptr<Globals> globals;

int getLogLevelFromStr(const char* pLogLevelStr);

#endif /*_UTILS_H_*/