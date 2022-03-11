/*
 * parseConfigFile.h
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#ifndef _PARSECONFIRGFILE_H_
#define _PARSECONFIRGFILE_H_

#include <string>

namespace CO2 {

int parseStringForKeyAndValue(std::string& str, std::string& key, std::string& value, bool* pIsQuoted);

} // namespace CO2

#endif /*_PARSECONFIRGFILE_H_*/
