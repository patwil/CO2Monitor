/*
 * parseConfigFile.h
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#ifndef _PARSECONFIRGFILE_H_
#define _PARSECONFIRGFILE_H_

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <cstring>

int parseStringForKeyAndValue(std::string& str, std::string& key, std::string& value);

#endif /*_PARSECONFIRGFILE_H_*/
