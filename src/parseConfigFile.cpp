/*
 * parseConfigFile.h
 *
 *  Created on: 2015-11-23
 *      Author: patw
 */

#include "parseConfigFile.h"

int CO2::parseStringForKeyAndValue(std::string& str, std::string& key, std::string& value, bool* pIsQuoted)
{
    int rc = 0;

    do { // once

        // skip blank (zero length) line
        if (str.length() == 0) {
            rc = 0;
            break;
        }

        // find start of key
        unsigned int keyStartPos = str.find_first_not_of(" \t\n");

        // skip blank (white space) line
        if (keyStartPos < 0) {
            rc = 0;
            break;
        }

        // skip comment line
        if (str[keyStartPos] == '#') {
            rc = 0;
            break;
        }

        // config line key should start with a letter
        if (!isalpha(str[keyStartPos])) {
            rc = -1;
            break;
        }

        // look for equal sign
        unsigned int valueStartPos = str.find_first_of("=") + 1;

        if ( (valueStartPos <= keyStartPos) || (valueStartPos >= str.length() ) ) {
            rc = -1;
            break;
        }

        // trim trailing spaces from key
        std::string str2 = str.substr(keyStartPos, valueStartPos - keyStartPos - 1);
        unsigned int keyEndPos = str2.find_last_not_of(" \t");
        key = str2.substr(0, keyEndPos + 1);

        // Now that we have key we can concentrate on its value
        valueStartPos += str.substr(valueStartPos).find_first_not_of(" \t\n");

        // skip line if value is missing
        if (valueStartPos >= str.length()) {
            rc = -1;
            break;
        }

        // look for quoted values
        bool isDoubleQuoted = (str[valueStartPos] == '"');
        bool isSingleQuoted = (str[valueStartPos] == '\'');

        *pIsQuoted = (isDoubleQuoted || isSingleQuoted);

        const char* excludeStr = " \t\r\n#;";

        if (isDoubleQuoted) {
            valueStartPos++;
            excludeStr = "\"";
        } else if (isSingleQuoted) {
            valueStartPos++;
            excludeStr = "'";
        }

        // include everything up to closing quote or, if not quoted, white space
        int valueEndPos = str.substr(valueStartPos).find_first_of(excludeStr);

        if (valueEndPos < 0) {
            valueEndPos = str.length() - valueStartPos;
        } else if (valueEndPos == 0) {
            // there is nothing between the quotes, so we have an empty string
            value = "";
        }

        value = str.substr(valueStartPos, valueEndPos);

        rc = value.length();

    } while (false);

    return rc;
}

