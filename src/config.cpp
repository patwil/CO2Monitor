/*
 * config.cpp
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#include "config.h"

Config::Config(string& defaultVal)
{
    try {
        if (defaultVal.empty()) {
            throw std::length_error {"Config::Config(string& defaultVal)"};
        }

        strVal = new char[strlen(defaultVal.c_str()) + 1];
        strcpy(strVal, defaultVal.c_str());
        valType = strType;
    } catch (std::length_error) {
        cerr << "null or zero length string" << endl;
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
    } catch (std::length_error) {
        cerr << "null or zero length string" << endl;
    } catch (...) {
        delete[] strVal;
    }
}

Config::Config(int defaultVal)
{
    iVal = defaultVal;
    valType = intType;
}

Config::Config(double defaultVal)
{
    dVal = defaultVal;
    valType = doubleType;
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

void Config::set(string val)
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
    if (valType == strType) {
        delete[] strVal;
    }

    iVal = val;
    valType = intType;
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

double Config::getDouble()
{
    return dVal;
}

ostream& operator<< (ostream& outs, const Config& rhs)
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

