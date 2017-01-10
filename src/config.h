/*
 * config.h
 *
 *  Created on: 2015-11-22
 *      Author: patw
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <iostream>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstring>
#include <climits>

class Config; // forward

typedef std::unordered_map<std::string, Config*> ConfigMap;
typedef std::unordered_map<std::string, Config*>::const_iterator ConfigMapCI;

class Config
{
    public:
        Config(std::string& defaultVal);
        Config(const char* defaultVal);
        Config(int defaultVal, int lo=INT_MIN, int hi=INT_MAX);
        Config(double defaultVal);
        ~Config();
        void set(std::string val);
        void set(const char* val);
        void set(int val);
        void set(double val);
        const char* getStr();
        int getInt();
        bool isInRange(int val);
        double getDouble();
        friend std::ostream& operator<< (std::ostream& outs, const Config& rhs);
    private:
        Config();
        union {
            char*  strVal;
            int    iVal;
            double dVal;
        };
        int lower_bound_;
        int higher_bound_;
        enum valTypes { strType, intType, doubleType };
        valTypes valType;
};

#endif /* _CONFIG_H_ */

