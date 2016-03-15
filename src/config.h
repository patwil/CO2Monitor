/*
 * config.h
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstring>

using namespace std;

class Config; // forward

typedef unordered_map<string, Config*> ConfigMap;
typedef unordered_map<string, Config*>::const_iterator ConfigMapCI;

class Config {
public:
    Config(string& defaultVal);
    Config(const char* defaultVal);
    Config(int defaultVal);
    Config(double defaultVal);
    ~Config();
    void set(string val);
    void set(const char* val);
    void set(int val);
    void set(double val);
    const char* getStr();
    int getInt();
    double getDouble();
    friend ostream& operator<< (ostream& outs, const Config& rhs);
private:
    Config();
    union {
        char*  strVal;
        int    iVal;
        double dVal;
    };
    enum valTypes { strType, intType, doubleType };
    valTypes valType;
};

#endif /* _CONFIG_H_ */

