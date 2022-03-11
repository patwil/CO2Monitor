/*
 * config.cpp
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#include <syslog.h>
#include <string.h>

#include "config.h"

Config::Config(std::string& defaultVal)
{
    try {
        if (defaultVal.empty()) {
            throw std::length_error {"Config::Config(string& defaultVal)"};
        }

        strVal = new char[strlen(defaultVal.c_str()) + 1];
        strcpy(strVal, defaultVal.c_str());
        valType = strType;
    } catch (const std::length_error& e) {
        syslog(LOG_ERR, "null or zero length string");
    } catch (...) {
        delete[] strVal;
    }

}

Config::Config(const char* defaultVal)
{
    try {
        if ( (defaultVal == nullptr) || (*defaultVal == 0) ) {
            throw std::length_error {"Config::Config(const char* defaultVal)"};
        }

        strVal = new char[strlen(defaultVal) + 1];
        strcpy(strVal, defaultVal);
        valType = strType;
        lower_bound_ = INT_MIN;
        higher_bound_ = INT_MAX;
    } catch (const std::length_error& e) {
        syslog(LOG_ERR, "null or zero length string");
    } catch (...) {
        delete[] strVal;
    }
}

Config::Config(int defaultVal, int lo, int hi)
{
    if (lo <= hi) {
        lower_bound_ = lo;
        higher_bound_ = hi;
    } else {
        lower_bound_ = hi;
        higher_bound_ = lo;
    }

    if ((lower_bound_ <= defaultVal) && (higher_bound_ >= defaultVal)) {
        iVal = defaultVal;
    } else {
        // default is out of range
        iVal = (lower_bound_ + higher_bound_) / 2; // halfway
    }
    valType = intType;
}

Config::Config(double defaultVal)
{
    dVal = defaultVal;
    valType = doubleType;
    lower_bound_ = INT_MIN;
    higher_bound_ = INT_MAX;
}

Config::~Config()
{
    switch (valType) {
        case strType:
            delete[] strVal;
            break;

        case intType:
        case doubleType:
            break;
    }
}

void Config::set(std::string val)
{
    this->set(val.c_str());
}

void Config::set(const char* val)
{
    if (valType == strType) {
        delete[] strVal;
    }

    strVal = new char[strlen(val) + 1];
    strcpy(strVal, val);
    valType = strType;
}

void Config::set(int val)
{
    // only change if within range
    if ((lower_bound_ <= val) && (higher_bound_ >= val)) {
        if (valType == strType) {
            delete[] strVal;
        }

        iVal = val;
        valType = intType;
    }
}

void Config::set(double val)
{
    if (valType == strType) {
        delete[] strVal;
    }

    dVal = val;
    valType = doubleType;
}

const char* Config::getStr()
{
    return (const char*)strVal;
}

int Config::getInt()
{
    return iVal;
}

bool Config::isInRange(int val)
{
    return (lower_bound_ <= val) && (higher_bound_ >= val);
}

double Config::getDouble()
{
    return dVal;
}

std::ostream& operator<< (std::ostream& outs, const Config& rhs)
{
    switch (rhs.valType) {
        case Config::valTypes::strType:
            outs << rhs.strVal;
            break;

        case Config::valTypes::intType:
            outs << rhs.iVal;
            break;

        case Config::valTypes::doubleType:
            outs << rhs.dVal;
            break;
    }

    return outs;
}

